#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AsyncActions/OpenPocketBaseAdminAsyncActions.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_AsyncAction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "OpenPocketBaseEditorValidation.h"
#include "OpenPocketBaseProjectSettings.h"
#include "UObject/Package.h"

namespace
{
bool ContainsIssue(
    const FOpenPocketBaseEditorValidationReport& Report,
    const EOpenPocketBaseEditorValidationCode Code)
{
    for (const FOpenPocketBaseEditorValidationIssue& Issue : Report.Issues)
    {
        if (Issue.Code == Code)
        {
            return true;
        }
    }
    return false;
}

UK2Node_AsyncAction* AddSuperuserNode(UEdGraph* Graph)
{
    const UFunction* Factory =
        UOpenPocketBaseAuthenticateSuperuserAsyncAction::StaticClass()->FindFunctionByName(
            TEXT("AuthenticateSuperuser"));
    if (Factory == nullptr)
    {
        return nullptr;
    }

    UK2Node_AsyncAction* Node = NewObject<UK2Node_AsyncAction>(Graph);
    Node->InitializeProxyFromFunction(Factory);
    Node->CreateNewGuid();
    Node->PostPlacedNewNode();
    Node->AllocateDefaultPins();
    Graph->AddNode(Node, true, false);
    return Node;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseEditorSettingsValidationTest,
    "OpenPocketBase.Editor.Validation.SettingsAndCapabilities",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseEditorSettingsValidationTest::RunTest(const FString& Parameters)
{
    UOpenPocketBaseProjectSettings* Settings = NewObject<UOpenPocketBaseProjectSettings>();
    Settings->DefaultProfile = TEXT("Tooling");
    Settings->bRequireRealtimeStreaming = true;
    Settings->bRequireOfflineModule = true;
    Settings->bRequirePrivilegedModule = true;

    FOpenPocketBaseProjectProfile Profile;
    Profile.Name = TEXT("Tooling");
    Profile.BaseUrl = TEXT("https://pb.example.test");
    Profile.SessionPersistence = EOpenPocketBaseSessionPersistence::RequireSecureStorage;
    Profile.bEnableAssistedOAuth = true;
    Settings->Profiles.Add(Profile);

    FOpenPocketBaseEditorValidationEnvironment Environment;
    Environment.bHttpStreamingAvailable = false;
    Environment.bSecurePersistenceAvailable = false;
    Environment.bOAuthCallbackAvailable = false;
    Environment.bOfflineModuleAvailable = false;
    Environment.bPrivilegedModuleAvailable = false;
    Environment.bPrivilegedShippingGatePresent = false;

    const FOpenPocketBaseEditorValidationReport Report =
        FOpenPocketBaseEditorValidator::ValidateSettings(*Settings, Environment);
    TestTrue(TEXT("Missing streaming is diagnosed"),
        ContainsIssue(Report, EOpenPocketBaseEditorValidationCode::StreamingUnavailable));
    TestTrue(TEXT("Missing secure persistence is diagnosed"),
        ContainsIssue(Report, EOpenPocketBaseEditorValidationCode::SecurePersistenceUnavailable));
    TestTrue(TEXT("Missing OAuth callback support is diagnosed"),
        ContainsIssue(Report, EOpenPocketBaseEditorValidationCode::OAuthCallbackUnavailable));
    TestTrue(TEXT("Missing offline module is diagnosed"),
        ContainsIssue(Report, EOpenPocketBaseEditorValidationCode::MissingOfflineModule));
    TestTrue(TEXT("Missing privileged module is diagnosed"),
        ContainsIssue(Report, EOpenPocketBaseEditorValidationCode::MissingPrivilegedModule));
    TestTrue(TEXT("Missing Shipping gate is diagnosed"),
        ContainsIssue(Report, EOpenPocketBaseEditorValidationCode::MissingPrivilegedShippingGate));

    Settings->Profiles.Add(Profile);
    const FOpenPocketBaseEditorValidationReport InvalidReport =
        FOpenPocketBaseEditorValidator::ValidateSettings(*Settings, Environment);
    TestTrue(TEXT("Invalid project settings are diagnosed"),
        ContainsIssue(InvalidReport, EOpenPocketBaseEditorValidationCode::InvalidProjectSettings));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseEditorCredentialValidationTest,
    "OpenPocketBase.Editor.Validation.CredentialSourcesAreSanitized",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseEditorCredentialValidationTest::RunTest(const FString& Parameters)
{
    const FString CommandSecret = TEXT("unique-command-secret-4938");
    const FString ConfigSecret = TEXT("unique-config-secret-9271");
    const FString SourceSecret = TEXT("unique-source-secret-6384");
    FOpenPocketBaseEditorValidationReport Report;
    FOpenPocketBaseEditorValidator::ScanCommandLine(
        TEXT("Editor -OpenPocketBaseSuperuserPassword=unique-command-secret-4938"),
        Report);
    FOpenPocketBaseEditorValidator::ScanTextArtifact(
        EOpenPocketBaseEditorArtifactKind::Config,
        TEXT("Config/DefaultGame.ini"),
        TEXT("OpenPocketBaseSuperuserPassword=unique-config-secret-9271"),
        Report);
    FOpenPocketBaseEditorValidator::ScanTextArtifact(
        EOpenPocketBaseEditorArtifactKind::Source,
        TEXT("Source/OperatorTool.cpp"),
        TEXT("const FString SuperuserPassword = TEXT(\"unique-source-secret-6384\");"),
        Report);

    TestTrue(TEXT("Command-line superuser material is diagnosed"),
        ContainsIssue(Report, EOpenPocketBaseEditorValidationCode::EmbeddedSuperuserCredential));
    TestTrue(TEXT("Every credential-bearing input is reported"), Report.Issues.Num() >= 3);
    const FString SanitizedText = Report.ToSanitizedText();
    TestFalse(TEXT("Command-line material is absent from validation output"),
        SanitizedText.Contains(CommandSecret));
    TestFalse(TEXT("Config material is absent from validation output"),
        SanitizedText.Contains(ConfigSecret));
    TestFalse(TEXT("Source material is absent from validation output"),
        SanitizedText.Contains(SourceSecret));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseEditorBlueprintCredentialValidationTest,
    "OpenPocketBase.Editor.Validation.BlueprintSuperuserDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseEditorBlueprintCredentialValidationTest::RunTest(const FString& Parameters)
{
    const FString BlueprintSecret = TEXT("unique-blueprint-secret-1703");
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        UObject::StaticClass(),
        GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("BP_CredentialFixture")),
        BPTYPE_Normal,
        NAME_None);
    if (!TestNotNull(TEXT("A Blueprint fixture is created"), Blueprint))
    {
        return false;
    }

    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        TEXT("Authenticate"),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, Graph, true, nullptr);
    UK2Node_AsyncAction* Node = AddSuperuserNode(Graph);
    if (!TestNotNull(TEXT("A superuser authentication node is created"), Node))
    {
        return false;
    }
    UEdGraphPin* PasswordPin = Node->FindPin(TEXT("Password"), EGPD_Input);
    if (!TestNotNull(TEXT("The node has a password pin"), PasswordPin))
    {
        return false;
    }
    PasswordPin->DefaultValue = BlueprintSecret;

    FOpenPocketBaseEditorValidationReport Report;
    FOpenPocketBaseEditorValidator::ScanBlueprint(*Blueprint, Report);
    TestTrue(TEXT("A password default on the privileged node is diagnosed"),
        ContainsIssue(Report, EOpenPocketBaseEditorValidationCode::EmbeddedSuperuserCredential));
    TestFalse(TEXT("The Blueprint password is absent from validation output"),
        Report.ToSanitizedText().Contains(BlueprintSecret));
    return true;
}

#endif
