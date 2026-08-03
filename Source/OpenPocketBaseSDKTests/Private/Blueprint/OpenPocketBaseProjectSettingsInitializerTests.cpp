#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenPocketBaseBlueprintClient.h"
#include "OpenPocketBaseClientLibrary.h"
#include "OpenPocketBaseProjectSettings.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseProjectSettingsInitializerContractTest,
    "OpenPocketBase.Blueprint.Client.ProjectSettingsInitializerContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseProjectSettingsInitializerContractTest::RunTest(const FString& Parameters)
{
    const UFunction* Function = UOpenPocketBaseClientLibrary::StaticClass()->FindFunctionByName(
        TEXT("InitializePocketBaseFromProjectSettings"));
    if (!TestNotNull(TEXT("Blueprint exposes project-settings initialization"), Function))
    {
        return false;
    }

    TestTrue(
        TEXT("Project-settings initialization is Blueprint callable"),
        Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
    TestTrue(
        TEXT("Project-settings initialization is a static library node"),
        Function->HasAnyFunctionFlags(FUNC_Static));
    TestEqual(
        TEXT("The node has a concise Blueprint title"),
        Function->GetMetaData(TEXT("DisplayName")),
        FString(TEXT("Initialize PocketBase from Project Settings")));
    TestEqual(
        TEXT("The node stays in the main client category"),
        Function->GetMetaData(TEXT("Category")),
        FString(TEXT("Open PocketBase|Client")));
    TestEqual(
        TEXT("The gameplay context is inferred"),
        Function->GetMetaData(TEXT("WorldContext")),
        FString(TEXT("WorldContextObject")));
    TestEqual(
        TEXT("The gameplay context pin is hidden"),
        Function->GetMetaData(TEXT("HidePin")),
        FString(TEXT("WorldContextObject")));
    TestEqual(
        TEXT("The gameplay context defaults to self"),
        Function->GetMetaData(TEXT("DefaultToSelf")),
        FString(TEXT("WorldContextObject")));
    TestEqual(
        TEXT("The node exposes success and failure execution paths"),
        Function->GetMetaData(TEXT("ExpandBoolAsExecs")),
        FString(TEXT("ReturnValue")));
    TestEqual(
        TEXT("The Boolean result is named for Blueprint users"),
        Function->GetMetaData(TEXT("ReturnDisplayName")),
        FString(TEXT("Succeeded")));

    const FObjectPropertyBase* WorldContext =
        FindFProperty<FObjectPropertyBase>(Function, TEXT("WorldContextObject"));
    const FNameProperty* Profile = FindFProperty<FNameProperty>(Function, TEXT("Profile"));
    const FObjectPropertyBase* Client =
        FindFProperty<FObjectPropertyBase>(Function, TEXT("Client"));
    const FStructProperty* Error = FindFProperty<FStructProperty>(Function, TEXT("Error"));

    TestNotNull(TEXT("The node accepts a gameplay context"), WorldContext);
    TestNotNull(TEXT("The node accepts an optional profile"), Profile);
    TestNotNull(TEXT("The node returns the initialized client"), Client);
    TestNotNull(TEXT("The node returns a typed error"), Error);
    TestTrue(
        TEXT("Profile is an advanced optional pin"),
        Profile != nullptr && Profile->HasAnyPropertyFlags(CPF_AdvancedDisplay));
    TestTrue(
        TEXT("The gameplay context is read only"),
        WorldContext != nullptr && WorldContext->HasAnyPropertyFlags(CPF_ConstParm));
    TestTrue(
        TEXT("The gameplay context accepts any UObject"),
        WorldContext != nullptr && WorldContext->PropertyClass == UObject::StaticClass());
    TestEqual(
        TEXT("Profile defaults to the configured default profile"),
        Function->GetMetaData(TEXT("CPP_Default_Profile")),
        FString(TEXT("None")));
    TestTrue(
        TEXT("Client is an output pin"),
        Client != nullptr && Client->HasAnyPropertyFlags(CPF_OutParm));
    TestTrue(
        TEXT("Client returns the Blueprint client type"),
        Client != nullptr && Client->PropertyClass == UOpenPocketBaseClient::StaticClass());
    TestTrue(
        TEXT("Error is an output pin"),
        Error != nullptr && Error->HasAnyPropertyFlags(CPF_OutParm));
    TestTrue(
        TEXT("Error returns the SDK error type"),
        Error != nullptr && Error->Struct == FOpenPocketBaseError::StaticStruct());
    TestTrue(
        TEXT("The node returns a Boolean"),
        CastField<FBoolProperty>(Function->GetReturnProperty()) != nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseProjectSettingsInitializerBehaviorTest,
    "OpenPocketBase.Blueprint.Client.ProjectSettingsInitializerUsesDefaultProfile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseProjectSettingsInitializerBehaviorTest::RunTest(const FString& Parameters)
{
    UFunction* Function = UOpenPocketBaseClientLibrary::StaticClass()->FindFunctionByName(
        TEXT("InitializePocketBaseFromProjectSettings"));
    if (!TestNotNull(TEXT("Project-settings initialization exists"), Function))
    {
        return false;
    }

    UOpenPocketBaseProjectSettings* Settings = GetMutableDefault<UOpenPocketBaseProjectSettings>();
    const FName PreviousDefaultProfile = Settings->DefaultProfile;
    const TArray<FOpenPocketBaseProjectProfile> PreviousProfiles = Settings->Profiles;

    Settings->DefaultProfile = TEXT("Demo");
    Settings->Profiles.Reset();
    FOpenPocketBaseProjectProfile DemoProfile;
    DemoProfile.Name = TEXT("Demo");
    DemoProfile.BaseUrl = TEXT("http://127.0.0.1:8090/");
    Settings->Profiles.Add(DemoProfile);

    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone(TEXT("OpenPocketBaseProjectSettingsInitializerTest"));

    const FObjectPropertyBase* WorldContext =
        FindFProperty<FObjectPropertyBase>(Function, TEXT("WorldContextObject"));
    const FNameProperty* Profile = FindFProperty<FNameProperty>(Function, TEXT("Profile"));
    const FObjectPropertyBase* ClientProperty =
        FindFProperty<FObjectPropertyBase>(Function, TEXT("Client"));
    const FStructProperty* ErrorProperty =
        FindFProperty<FStructProperty>(Function, TEXT("Error"));
    const FBoolProperty* ReturnProperty =
        CastField<FBoolProperty>(Function->GetReturnProperty());
    if (!TestNotNull(TEXT("The node has a gameplay context input"), WorldContext) ||
        !TestNotNull(TEXT("The node has a profile input"), Profile) ||
        !TestNotNull(TEXT("The node has a client output"), ClientProperty) ||
        !TestNotNull(TEXT("The node has an error output"), ErrorProperty) ||
        !TestNotNull(TEXT("The node has a Boolean result"), ReturnProperty))
    {
        Settings->DefaultProfile = PreviousDefaultProfile;
        Settings->Profiles = PreviousProfiles;
        GameInstance->Shutdown();
        GameInstance->RemoveFromRoot();
        return false;
    }

    FStructOnScope CallParameters(Function);
    void* ParameterMemory = CallParameters.GetStructMemory();
    WorldContext->SetObjectPropertyValue_InContainer(ParameterMemory, GameInstance);
    Profile->SetPropertyValue_InContainer(ParameterMemory, NAME_None);

    UOpenPocketBaseClientLibrary::StaticClass()->GetDefaultObject()->ProcessEvent(
        Function,
        ParameterMemory);

    const bool bSucceeded = ReturnProperty->GetPropertyValue_InContainer(ParameterMemory);
    UOpenPocketBaseClient* Client = Cast<UOpenPocketBaseClient>(
        ClientProperty->GetObjectPropertyValue_InContainer(ParameterMemory));
    const FOpenPocketBaseError& Error =
        *ErrorProperty->ContainerPtrToValuePtr<FOpenPocketBaseError>(ParameterMemory);

    Settings->DefaultProfile = PreviousDefaultProfile;
    Settings->Profiles = PreviousProfiles;

    TestTrue(TEXT("The configured default profile initializes the client"), bSucceeded);
    TestNotNull(TEXT("The initialized client is returned"), Client);
    TestFalse(TEXT("Successful initialization returns no error"), Error.IsSet());
    if (Client != nullptr)
    {
        TestEqual(
            TEXT("The client uses the normalized profile URL"),
            Client->GetBaseUrl(),
            FString(TEXT("http://127.0.0.1:8090")));
    }

    UOpenPocketBaseClientLibrary::ShutdownPocketBase(GameInstance);
    GameInstance->Shutdown();
    GameInstance->RemoveFromRoot();
    return true;
}

#endif
