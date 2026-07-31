#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "OpenPocketBaseBlueprintClient.h"
#include "OpenPocketBaseCustomRoute.h"
#include "OpenPocketBaseStringLibrary.h"
#include "UObject/Package.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectIterator.h"

namespace
{
FEdGraphPinType MakeStringPinType()
{
    FEdGraphPinType Type;
    Type.PinCategory = UEdGraphSchema_K2::PC_String;
    return Type;
}

FEdGraphPinType MakeStructPinType(UScriptStruct* Struct)
{
    FEdGraphPinType Type;
    Type.PinCategory = UEdGraphSchema_K2::PC_Struct;
    Type.PinSubCategoryObject = Struct;
    return Type;
}

FEdGraphPinType MakeEnumPinType(UEnum* Enum)
{
    FEdGraphPinType Type;
    Type.PinCategory = UEdGraphSchema_K2::PC_Byte;
    Type.PinSubCategoryObject = Enum;
    return Type;
}

template <typename Type, typename PinTypeFactory>
void FindMissingAutocasts(
    const UEdGraphSchema_K2& Schema,
    const FName PackageName,
    const FString& ExpectedOwner,
    PinTypeFactory MakePinType,
    TArray<FString>& OutTypeNames,
    TArray<FString>& OutMissing)
{
    const FEdGraphPinType StringType = MakeStringPinType();
    for (TObjectIterator<Type> It; It; ++It)
    {
        Type* ReflectedType = *It;
        if (ReflectedType->GetOutermost()->GetFName() != PackageName ||
            !ReflectedType->GetName().Contains(TEXT("OpenPocketBase")))
        {
            continue;
        }

        OutTypeNames.Add(ReflectedType->GetName());
        const TOptional<UEdGraphSchema_K2::FSearchForAutocastFunctionResults> Conversion =
            Schema.SearchForAutocastFunction(MakePinType(ReflectedType), StringType);
        if (!Conversion.IsSet() ||
            Conversion->FunctionOwner == nullptr ||
            Conversion->FunctionOwner->GetName() != ExpectedOwner)
        {
            OutMissing.Add(ReflectedType->GetName());
        }
    }
}

FString InvokeHealthResultConversion(const FOpenPocketBaseHealthResult& HealthResult)
{
    UClass* Library = FindObject<UClass>(
        nullptr,
        TEXT("/Script/OpenPocketBaseSDK.OpenPocketBaseStringLibrary"));
    if (Library == nullptr)
    {
        return FString();
    }

    UFunction* Function = Library->FindFunctionByName(
        TEXT("Conv_OpenPocketBaseHealthResultToString"));
    if (Function == nullptr)
    {
        return FString();
    }

    FStructProperty* InputProperty = nullptr;
    FStrProperty* ReturnProperty = nullptr;
    for (TFieldIterator<FProperty> It(Function); It; ++It)
    {
        if (It->HasAnyPropertyFlags(CPF_ReturnParm))
        {
            ReturnProperty = CastField<FStrProperty>(*It);
        }
        else if (It->HasAnyPropertyFlags(CPF_Parm))
        {
            InputProperty = CastField<FStructProperty>(*It);
        }
    }
    if (InputProperty == nullptr || ReturnProperty == nullptr)
    {
        return FString();
    }

    FStructOnScope Parameters(Function);
    InputProperty->Struct->CopyScriptStruct(
        InputProperty->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()),
        &HealthResult);
    Library->GetDefaultObject()->ProcessEvent(Function, Parameters.GetStructMemory());
    return ReturnProperty->GetPropertyValue_InContainer(Parameters.GetStructMemory());
}

UK2Node_VariableGet* AddVariableGetter(
    UBlueprint* Blueprint,
    UEdGraph* Graph,
    const FName VariableName,
    const FEdGraphPinType& Type)
{
    if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, VariableName, Type))
    {
        return nullptr;
    }

    UK2Node_VariableGet* Getter = NewObject<UK2Node_VariableGet>(Graph);
    Getter->VariableReference.SetSelfMember(VariableName);
    Getter->CreateNewGuid();
    Getter->PostPlacedNewNode();
    Getter->AllocateDefaultPins();
    Graph->AddNode(Getter, true, false);
    return Getter;
}

UK2Node_CallFunction* AddStringConsumer(UEdGraph* Graph)
{
    UK2Node_CallFunction* Consumer = NewObject<UK2Node_CallFunction>(Graph);
    Consumer->SetFromFunction(UKismetStringLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Len)));
    Consumer->CreateNewGuid();
    Consumer->PostPlacedNewNode();
    Consumer->AllocateDefaultPins();
    Graph->AddNode(Consumer, true, false);
    return Consumer;
}

bool AddAndValidateAutocastConnection(
    UBlueprint* Blueprint,
    UEdGraph* Graph,
    const FEdGraphPinType& Type,
    const FString& ExpectedOwner,
    const int32 Index,
    FString& OutFailure)
{
    UK2Node_VariableGet* Getter = AddVariableGetter(
        Blueprint,
        Graph,
        *FString::Printf(TEXT("AutocastValue_%d"), Index),
        Type);
    UK2Node_CallFunction* Consumer = AddStringConsumer(Graph);
    UEdGraphPin* SourcePin = Getter != nullptr ? Getter->GetValuePin() : nullptr;
    UEdGraphPin* StringPin =
        Consumer != nullptr ? Consumer->FindPin(TEXT("S"), EGPD_Input) : nullptr;
    if (SourcePin == nullptr || StringPin == nullptr)
    {
        OutFailure = TEXT("Could not create source and String consumer pins.");
        return false;
    }

    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    if (!Schema->TryCreateConnection(SourcePin, StringPin) ||
        SourcePin->LinkedTo.Num() != 1 || StringPin->LinkedTo.Num() != 1)
    {
        OutFailure = TEXT("Unreal did not insert a complete autocast connection.");
        return false;
    }

    UK2Node_CallFunction* Conversion =
        Cast<UK2Node_CallFunction>(SourcePin->LinkedTo[0]->GetOwningNode());
    const UFunction* Function = Conversion != nullptr ? Conversion->GetTargetFunction() : nullptr;
    if (Function == nullptr || Function->GetOwnerClass()->GetName() != ExpectedOwner ||
        StringPin->LinkedTo[0]->GetOwningNode() != Conversion)
    {
        OutFailure = TEXT("The inserted node is not the expected PocketBase conversion.");
        return false;
    }
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseStringAutocastTest,
    "OpenPocketBase.Blueprint.AllDataTypesAutocastToString",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseStringAutocastTest::RunTest(const FString& Parameters)
{
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    if (!TestNotNull(TEXT("The Blueprint schema is available"), Schema))
    {
        return false;
    }

    TArray<FString> CoreStructs;
    TArray<FString> CoreEnums;
    TArray<FString> AdminStructs;
    TArray<FString> AdminEnums;
    TArray<FString> Missing;
    FindMissingAutocasts<UScriptStruct>(
        *Schema,
        TEXT("/Script/OpenPocketBaseSDK"),
        TEXT("OpenPocketBaseStringLibrary"),
        MakeStructPinType,
        CoreStructs,
        Missing);
    FindMissingAutocasts<UEnum>(
        *Schema,
        TEXT("/Script/OpenPocketBaseSDK"),
        TEXT("OpenPocketBaseStringLibrary"),
        MakeEnumPinType,
        CoreEnums,
        Missing);
    FindMissingAutocasts<UScriptStruct>(
        *Schema,
        TEXT("/Script/OpenPocketBaseSDKAdmin"),
        TEXT("OpenPocketBaseAdminStringLibrary"),
        MakeStructPinType,
        AdminStructs,
        Missing);
    FindMissingAutocasts<UEnum>(
        *Schema,
        TEXT("/Script/OpenPocketBaseSDKAdmin"),
        TEXT("OpenPocketBaseAdminStringLibrary"),
        MakeEnumPinType,
        AdminEnums,
        Missing);

    TestEqual(TEXT("Every core struct is covered"), CoreStructs.Num(), 80);
    TestEqual(TEXT("Every core enum is covered"), CoreEnums.Num(), 30);
    TestEqual(TEXT("Every admin struct is covered"), AdminStructs.Num(), 17);
    TestEqual(TEXT("Every admin enum is covered"), AdminEnums.Num(), 8);
    TestTrue(
        *FString::Printf(
            TEXT("Every SDK data type autocasts to String. Missing: %s"),
            *FString::Join(Missing, TEXT(", "))),
        Missing.IsEmpty());

    const FName BlueprintName = MakeUniqueObjectName(
        GetTransientPackage(),
        UBlueprint::StaticClass(),
        TEXT("BP_OpenPocketBaseStringAutocasts"));
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        UObject::StaticClass(),
        GetTransientPackage(),
        BlueprintName,
        BPTYPE_Normal,
        NAME_None);
    if (!TestNotNull(TEXT("A Blueprint autocast consumer is created"), Blueprint))
    {
        return false;
    }

    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        TEXT("ExerciseStringAutocasts"),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, Graph, true, nullptr);

    int32 ConnectionIndex = 0;
    TArray<FString> ConnectionFailures;
    const auto ConnectTypes =
        [&](const FName PackageName, const FString& ExpectedOwner, const bool bEnums)
        {
            if (bEnums)
            {
                for (TObjectIterator<UEnum> It; It; ++It)
                {
                    UEnum* Type = *It;
                    if (Type->GetOutermost()->GetFName() != PackageName ||
                        !Type->GetName().Contains(TEXT("OpenPocketBase")))
                    {
                        continue;
                    }
                    FString Failure;
                    if (!AddAndValidateAutocastConnection(
                            Blueprint,
                            Graph,
                            MakeEnumPinType(Type),
                            ExpectedOwner,
                            ConnectionIndex++,
                            Failure))
                    {
                        ConnectionFailures.Add(Type->GetName() + TEXT(": ") + Failure);
                    }
                }
                return;
            }

            for (TObjectIterator<UScriptStruct> It; It; ++It)
            {
                UScriptStruct* Type = *It;
                if (Type->GetOutermost()->GetFName() != PackageName ||
                    !Type->GetName().Contains(TEXT("OpenPocketBase")))
                {
                    continue;
                }
                FString Failure;
                if (!AddAndValidateAutocastConnection(
                        Blueprint,
                        Graph,
                        MakeStructPinType(Type),
                        ExpectedOwner,
                        ConnectionIndex++,
                        Failure))
                {
                    ConnectionFailures.Add(Type->GetName() + TEXT(": ") + Failure);
                }
            }
        };

    ConnectTypes(
        TEXT("/Script/OpenPocketBaseSDK"),
        TEXT("OpenPocketBaseStringLibrary"),
        false);
    ConnectTypes(
        TEXT("/Script/OpenPocketBaseSDK"),
        TEXT("OpenPocketBaseStringLibrary"),
        true);
    ConnectTypes(
        TEXT("/Script/OpenPocketBaseSDKAdmin"),
        TEXT("OpenPocketBaseAdminStringLibrary"),
        false);
    ConnectTypes(
        TEXT("/Script/OpenPocketBaseSDKAdmin"),
        TEXT("OpenPocketBaseAdminStringLibrary"),
        true);

    TestTrue(
        *FString::Printf(
            TEXT("Every SDK data type connects through its String autocast. Failures: %s"),
            *FString::Join(ConnectionFailures, TEXT(", "))),
        ConnectionFailures.IsEmpty());
    TestEqual(
        TEXT("Every discovered SDK data type was connected"),
        ConnectionIndex,
        CoreStructs.Num() + CoreEnums.Num() + AdminStructs.Num() + AdminEnums.Num());

    FEdGraphPinType ClientType;
    ClientType.PinCategory = UEdGraphSchema_K2::PC_Object;
    ClientType.PinSubCategoryObject = UOpenPocketBaseClient::StaticClass();
    UK2Node_VariableGet* ClientGetter = AddVariableGetter(
        Blueprint,
        Graph,
        TEXT("SessionClient"),
        ClientType);
    UK2Node_CallFunction* SessionNode = NewObject<UK2Node_CallFunction>(Graph);
    SessionNode->SetFromFunction(UOpenPocketBaseClient::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, TryGetCurrentSession)));
    SessionNode->CreateNewGuid();
    SessionNode->PostPlacedNewNode();
    SessionNode->AllocateDefaultPins();
    Graph->AddNode(SessionNode, true, false);
    UK2Node_CallFunction* SessionStringConsumer = AddStringConsumer(Graph);
    TestTrue(
        TEXT("Get Current Session receives a Blueprint client"),
        ClientGetter != nullptr &&
            Schema->TryCreateConnection(
                ClientGetter->GetValuePin(),
                SessionNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input)));
    TestNotNull(
        TEXT("Get Current Session exposes the authenticated path"),
        SessionNode->FindPin(TEXT("True"), EGPD_Output));
    TestNotNull(
        TEXT("Get Current Session exposes the unauthenticated path"),
        SessionNode->FindPin(TEXT("False"), EGPD_Output));
    TestTrue(
        TEXT("A Get Current Session result connects through its String autocast"),
        SessionStringConsumer != nullptr &&
            Schema->TryCreateConnection(
                SessionNode->FindPin(TEXT("OutSession"), EGPD_Output),
                SessionStringConsumer->FindPin(TEXT("S"), EGPD_Input)));

    FKismetEditorUtilities::CompileBlueprint(
        Blueprint,
        EBlueprintCompileOptions::SkipGarbageCollection);
    TestTrue(
        TEXT("All SDK String autocasts compile in a Blueprint graph"),
        Blueprint->Status != BS_Error);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseDebugStringFormattingTest,
    "OpenPocketBase.Blueprint.DataTypeStringsAreReadable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseDebugStringFormattingTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseHealthResult HealthResult;
    HealthResult.bHealthy = true;
    HealthResult.HttpStatus = 200;
    HealthResult.Code = 200;
    HealthResult.Message = TEXT("API is healthy.");
    HealthResult.DurationSeconds = 0.001;

    const FString Output = InvokeHealthResultConversion(HealthResult);
    TestTrue(TEXT("The type name is included"), Output.Contains(TEXT("Health Result")));
    TestTrue(TEXT("Boolean fields are readable"), Output.Contains(TEXT("\"healthy\": true")));
    TestTrue(TEXT("Status fields are readable"), Output.Contains(TEXT("\"httpStatus\": 200")));
    TestTrue(TEXT("Messages are readable"), Output.Contains(TEXT("API is healthy.")));
    TestTrue(TEXT("The output uses multiple lines"), Output.Contains(TEXT("\n")));

    FOpenPocketBaseClientConfig Config;
    Config.DefaultHeaders.Add(TEXT("Authorization"), TEXT("secret-value"));
    const FString ConfigOutput =
        UOpenPocketBaseStringLibrary::Conv_OpenPocketBaseClientConfigToString(Config);
    TestFalse(TEXT("Sensitive values are not printed"), ConfigOutput.Contains(TEXT("secret-value")));
    TestTrue(TEXT("Sensitive values are marked as redacted"), ConfigOutput.Contains(TEXT("<redacted>")));

    FOpenPocketBaseAuthMethods AuthMethods;
    AuthMethods.Password.bEnabled = true;
    AuthMethods.Password.IdentityFields.Add(TEXT("email"));
    const FString AuthMethodsOutput =
        UOpenPocketBaseStringLibrary::Conv_OpenPocketBaseAuthMethodsToString(AuthMethods);
    TestTrue(
        TEXT("Password authentication configuration remains readable"),
        AuthMethodsOutput.Contains(TEXT("\"password\":")) &&
            !AuthMethodsOutput.Contains(TEXT("\"password\": \"<redacted>\"")) &&
            AuthMethodsOutput.Contains(TEXT("\"identityFields\":")) &&
            AuthMethodsOutput.Contains(TEXT("\"email\"")));
    TestTrue(
        TEXT("OAuth2 authentication configuration uses PocketBase casing"),
        AuthMethodsOutput.Contains(
            TEXT("\"oauth2\":"), ESearchCase::CaseSensitive) &&
            !AuthMethodsOutput.Contains(
                TEXT("\"oAuth2\":"), ESearchCase::CaseSensitive));

    FOpenPocketBaseFileDownloadResult Download;
    Download.Bytes.SetNumZeroed(32);
    const FString DownloadOutput =
        UOpenPocketBaseStringLibrary::Conv_OpenPocketBaseFileDownloadResultToString(Download);
    TestTrue(TEXT("Byte arrays are summarized"), DownloadOutput.Contains(TEXT("<32 bytes>")));

    TestEqual(
        TEXT("Enums use their readable names"),
        UOpenPocketBaseStringLibrary::Conv_OpenPocketBaseErrorKindToString(
            EOpenPocketBaseErrorKind::Transport),
        FString(TEXT("Transport")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseDebugStringRedactionTest,
    "OpenPocketBase.Blueprint.DataTypeStringsRedactSecrets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseDebugStringRedactionTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseOAuth2Callback Callback;
    Callback.TransactionId = TEXT("callback-transaction-secret");
    Callback.CallbackUrl =
        TEXT("https://game.example/oauth/callback?code=callback-code-secret&state=callback-state-secret&screen=login");
    Callback.Mfa.Id = TEXT("callback-mfa-secret");
    const FString CallbackOutput =
        UOpenPocketBaseStringLibrary::Conv_OpenPocketBaseOAuth2CallbackToString(Callback);
    TestFalse(
        TEXT("OAuth callback strings omit the transaction ID"),
        CallbackOutput.Contains(TEXT("callback-transaction-secret")));
    TestFalse(
        TEXT("OAuth callback strings omit callback query secrets"),
        CallbackOutput.Contains(TEXT("callback-code-secret")) ||
            CallbackOutput.Contains(TEXT("callback-state-secret")));
    TestFalse(
        TEXT("OAuth callback strings omit the nested MFA ID"),
        CallbackOutput.Contains(TEXT("callback-mfa-secret")));
    TestTrue(
        TEXT("OAuth callback strings retain useful URL and query context"),
        CallbackOutput.Contains(TEXT("https://game.example/oauth/callback")) &&
            CallbackOutput.Contains(TEXT("screen=login")) &&
            CallbackOutput.Contains(TEXT("<redacted>")));

    FOpenPocketBaseOAuth2Authorization Authorization;
    Authorization.TransactionId = TEXT("authorization-transaction-secret");
    Authorization.AuthorizationUrl =
        TEXT("https://identity.example/authorize?state=authorization-state-query-secret&code_challenge=authorization-challenge-secret&prompt=consent");
    Authorization.State = TEXT("authorization-state-secret");
    const FString AuthorizationOutput =
        UOpenPocketBaseStringLibrary::Conv_OpenPocketBaseOAuth2AuthorizationToString(
            Authorization);
    TestFalse(
        TEXT("OAuth authorization strings omit the transaction ID"),
        AuthorizationOutput.Contains(TEXT("authorization-transaction-secret")));
    TestFalse(
        TEXT("OAuth authorization strings omit the state"),
        AuthorizationOutput.Contains(TEXT("authorization-state-secret")));
    TestFalse(
        TEXT("OAuth authorization strings omit URL query secrets"),
        AuthorizationOutput.Contains(TEXT("authorization-state-query-secret")) ||
            AuthorizationOutput.Contains(TEXT("authorization-challenge-secret")));
    TestTrue(
        TEXT("OAuth authorization strings retain useful URL context"),
        AuthorizationOutput.Contains(TEXT("https://identity.example/authorize")) &&
            AuthorizationOutput.Contains(TEXT("prompt=consent")) &&
            AuthorizationOutput.Contains(TEXT("<redacted>")));

    FOpenPocketBaseMfaContinuation Mfa;
    Mfa.Id = TEXT("standalone-mfa-secret");
    const FString MfaOutput =
        UOpenPocketBaseStringLibrary::Conv_OpenPocketBaseMfaContinuationToString(Mfa);
    TestFalse(
        TEXT("MFA continuation strings omit the continuation ID"),
        MfaOutput.Contains(TEXT("standalone-mfa-secret")));
    TestTrue(
        TEXT("MFA continuation strings mark the ID as redacted"),
        MfaOutput.Contains(TEXT("<redacted>")));

    FOpenPocketBaseCustomRouteRequest Request;
    Request.Method = EOpenPocketBaseCustomRouteMethod::Post;
    Request.Path = TEXT("/api/chunk71");
    Request.Query.Add(TEXT("access_token"), TEXT("query-token-secret"));
    Request.Query.Add(TEXT("view"), TEXT("summary"));
    Request.FormFields.Add(TEXT("password"), TEXT("form-password-secret"));
    Request.FormFields.Add(TEXT("label"), TEXT("chunk71"));
    Request.JsonBody.JsonObject = MakeShared<FJsonObject>();
    Request.JsonBody.JsonObject->SetStringField(TEXT("clientSecret"), TEXT("json-client-secret"));
    Request.JsonBody.JsonObject->SetStringField(TEXT("name"), TEXT("visible-name"));
    const TSharedPtr<FJsonObject> Nested = MakeShared<FJsonObject>();
    Nested->SetStringField(TEXT("refresh_token"), TEXT("json-refresh-secret"));
    Nested->SetStringField(TEXT("mode"), TEXT("safe-mode"));
    Request.JsonBody.JsonObject->SetObjectField(TEXT("nested"), Nested);

    const FString RequestOutput =
        UOpenPocketBaseStringLibrary::Conv_OpenPocketBaseCustomRouteRequestToString(Request);
    TestFalse(
        TEXT("Custom route strings omit sensitive query values"),
        RequestOutput.Contains(TEXT("query-token-secret")));
    TestFalse(
        TEXT("Custom route strings omit sensitive form values"),
        RequestOutput.Contains(TEXT("form-password-secret")));
    TestFalse(
        TEXT("Custom route strings omit nested sensitive JSON values"),
        RequestOutput.Contains(TEXT("json-client-secret")) ||
            RequestOutput.Contains(TEXT("json-refresh-secret")));
    TestTrue(
        TEXT("Custom route strings retain non-sensitive request context"),
        RequestOutput.Contains(TEXT("/api/chunk71")) &&
            RequestOutput.Contains(TEXT("summary")) &&
            RequestOutput.Contains(TEXT("chunk71")) &&
            RequestOutput.Contains(TEXT("visible-name")) &&
            RequestOutput.Contains(TEXT("safe-mode")) &&
            RequestOutput.Contains(TEXT("<redacted>")));
    return true;
}

#endif
