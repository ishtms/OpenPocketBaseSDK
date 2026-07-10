#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AsyncActions/OpenPocketBaseRecordAsyncActions.h"
#include "AsyncActions/OpenPocketBaseBatchAsyncAction.h"
#include "AsyncActions/OpenPocketBaseFileAsyncActions.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_AsyncAction.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "OpenPocketBaseFilterLibrary.h"
#include "OpenPocketBaseClientLibrary.h"
#include "OpenPocketBaseFileLibrary.h"
#include "OpenPocketBaseBatchLibrary.h"
#include "OpenPocketBaseRecordLibrary.h"
#include "OpenPocketBaseRecord.h"
#include "OpenPocketBaseSubsystem.h"
#include "UObject/Field.h"
#include "UObject/Package.h"

namespace
{
UK2Node_AsyncAction* AddAsyncConsumerNode(
    UEdGraph* Graph,
    UClass* FactoryClass,
    const FName FactoryFunctionName)
{
    const UFunction* FactoryFunction = FactoryClass->FindFunctionByName(FactoryFunctionName);
    if (FactoryFunction == nullptr)
    {
        return nullptr;
    }

    UK2Node_AsyncAction* Node = NewObject<UK2Node_AsyncAction>(Graph);
    Node->InitializeProxyFromFunction(FactoryFunction);
    Node->CreateNewGuid();
    Node->PostPlacedNewNode();
    Node->AllocateDefaultPins();
    Graph->AddNode(Node, true, false);
    return Node;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBlueprintValueApiTest,
    "OpenPocketBase.Blueprint.Values.UsesComposablePureNodes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBlueprintValueApiTest::RunTest(const FString& Parameters)
{
    const auto TestPureFunction = [this](UClass* Library, const FName Name)
    {
        const UFunction* Function = Library->FindFunctionByName(Name);
        TestNotNull(*FString::Printf(TEXT("%s exists"), *Name.ToString()), Function);
        if (Function != nullptr)
        {
            TestTrue(
                *FString::Printf(TEXT("%s is pure"), *Name.ToString()),
                Function->HasAnyFunctionFlags(FUNC_BlueprintPure));
        }
    };

    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("StringFilter"));
    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("NumberFilter"));
    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("BooleanFilter"));
    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("DateFilter"));
    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("NullFilter"));
    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("AndFilters"));
    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("OrFilters"));
    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("RawFilter"));

    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("NewRecordBody"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithStringField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithNumberField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithBooleanField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithNullField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithStringArrayField"));

    TestPureFunction(UOpenPocketBaseBatchLibrary::StaticClass(), TEXT("NewBatch"));
    TestPureFunction(UOpenPocketBaseBatchLibrary::StaticClass(), TEXT("WithCreate"));
    TestPureFunction(UOpenPocketBaseBatchLibrary::StaticClass(), TEXT("WithUpdate"));
    TestPureFunction(UOpenPocketBaseBatchLibrary::StaticClass(), TEXT("WithUpsert"));
    TestPureFunction(UOpenPocketBaseBatchLibrary::StaticClass(), TEXT("WithDelete"));

    TestNull(
        TEXT("The mutable filter accumulator is no longer exposed"),
        UOpenPocketBaseFilterLibrary::StaticClass()->FindFunctionByName(TEXT("AddBooleanParameter")));
    TestNull(
        TEXT("Manual filter binding is no longer part of the normal Blueprint flow"),
        UOpenPocketBaseFilterLibrary::StaticClass()->FindFunctionByName(TEXT("BindFilter")));
    TestNull(
        TEXT("Record body mutation is no longer exposed"),
        UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(TEXT("SetRecordBodyStringField")));
    TestNull(
        TEXT("Batch mutation is no longer exposed"),
        UOpenPocketBaseBatchLibrary::StaticClass()->FindFunctionByName(TEXT("AddCreate")));
    TestNull(
        TEXT("Batch reset is unnecessary for value flows"),
        UOpenPocketBaseBatchLibrary::StaticClass()->FindFunctionByName(TEXT("Clear")));

    const FProperty* FilterProperty =
        FOpenPocketBaseListOptions::StaticStruct()->FindPropertyByName(TEXT("Filter"));
    const FStructProperty* FilterStructProperty = CastField<FStructProperty>(FilterProperty);
    TestNotNull(TEXT("List options accept a filter value"), FilterStructProperty);
    if (FilterStructProperty != nullptr)
    {
        TestEqual(
            TEXT("List options use the Open PocketBase filter type"),
            FilterStructProperty->Struct->GetFName(),
            FName(TEXT("OpenPocketBaseFilter")));
    }

    const FProperty* RealtimeFilterProperty =
        FOpenPocketBaseRealtimeOptions::StaticStruct()->FindPropertyByName(TEXT("Filter"));
    const FStructProperty* RealtimeFilterStructProperty =
        CastField<FStructProperty>(RealtimeFilterProperty);
    TestNotNull(TEXT("Realtime options accept a filter value"), RealtimeFilterStructProperty);
    if (RealtimeFilterStructProperty != nullptr)
    {
        TestEqual(
            TEXT("Realtime options use the Open PocketBase filter type"),
            RealtimeFilterStructProperty->Struct->GetFName(),
            FName(TEXT("OpenPocketBaseFilter")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBlueprintClientEntryApiTest,
    "OpenPocketBase.Blueprint.Client.UsesDirectEntryPoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBlueprintClientEntryApiTest::RunTest(const FString& Parameters)
{
    UClass* Library = FindObject<UClass>(
        nullptr,
        TEXT("/Script/OpenPocketBaseSDK.OpenPocketBaseClientLibrary"));
    if (!TestNotNull(TEXT("The Blueprint client library exists"), Library))
    {
        return false;
    }

    const UFunction* Initialize = Library->FindFunctionByName(TEXT("InitializePocketBase"));
    TestNotNull(TEXT("Initialize PocketBase exists"), Initialize);
    if (Initialize != nullptr)
    {
        TestEqual(
            TEXT("Initialize PocketBase expands into success and failure execution paths"),
            Initialize->GetMetaData(TEXT("ExpandBoolAsExecs")),
            FString(TEXT("ReturnValue")));
        TestEqual(
            TEXT("Initialize PocketBase resolves its world automatically"),
            Initialize->GetMetaData(TEXT("WorldContext")),
            FString(TEXT("WorldContextObject")));
        TestEqual(
            TEXT("Initialize PocketBase hides its world pin"),
            Initialize->GetMetaData(TEXT("HidePin")),
            FString(TEXT("WorldContextObject")));
    }

    const UFunction* GetClient = Library->FindFunctionByName(TEXT("GetPocketBaseClient"));
    TestNotNull(TEXT("Get PocketBase Client exists"), GetClient);
    if (GetClient != nullptr)
    {
        TestTrue(
            TEXT("Get PocketBase Client is pure"),
            GetClient->HasAnyFunctionFlags(FUNC_BlueprintPure));
    }

    TestNotNull(
        TEXT("Advanced named client creation remains available"),
        Library->FindFunctionByName(TEXT("CreateNamedPocketBaseClient")));
    TestNotNull(
        TEXT("Advanced named client lookup remains available"),
        Library->FindFunctionByName(TEXT("GetNamedPocketBaseClient")));

    TestNull(
        TEXT("The subsystem no longer exposes client construction nodes"),
        UOpenPocketBaseSubsystem::StaticClass()->FindFunctionByName(TEXT("CreateClient")));
    TestNull(
        TEXT("The subsystem no longer exposes name-sensitive lookup nodes"),
        UOpenPocketBaseSubsystem::StaticClass()->FindFunctionByName(TEXT("GetClient")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBlueprintConsumerTest,
    "OpenPocketBase.Blueprint.Consumer.CompilesPublicNodes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBlueprintConsumerTest::RunTest(const FString& Parameters)
{
    const FName BlueprintName = MakeUniqueObjectName(
        GetTransientPackage(),
        UBlueprint::StaticClass(),
        TEXT("BP_OpenPocketBaseConsumer"));
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        UObject::StaticClass(),
        GetTransientPackage(),
        BlueprintName,
        BPTYPE_Normal,
        NAME_None);
    if (!TestNotNull(TEXT("A Blueprint-only consumer is created"), Blueprint))
    {
        return false;
    }

    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        TEXT("ExerciseOpenPocketBase"),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, Graph, true, nullptr);

    UK2Node_CallFunction* InitializeNode = NewObject<UK2Node_CallFunction>(Graph);
    InitializeNode->SetFromFunction(UOpenPocketBaseClientLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClientLibrary, InitializePocketBase)));
    InitializeNode->CreateNewGuid();
    InitializeNode->PostPlacedNewNode();
    InitializeNode->AllocateDefaultPins();
    Graph->AddNode(InitializeNode, true, false);

    UK2Node_CallFunction* GetClientNode = NewObject<UK2Node_CallFunction>(Graph);
    GetClientNode->SetFromFunction(UOpenPocketBaseClientLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClientLibrary, GetPocketBaseClient)));
    GetClientNode->CreateNewGuid();
    GetClientNode->PostPlacedNewNode();
    GetClientNode->AllocateDefaultPins();
    Graph->AddNode(GetClientNode, true, false);

    UK2Node_AsyncAction* HealthNode = AddAsyncConsumerNode(
        Graph,
        UOpenPocketBaseHealthAsyncAction::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseHealthAsyncAction, CheckHealth));
    TestNotNull(
        TEXT("Check Health is available as an async Blueprint node"),
        HealthNode);
    TestNotNull(
        TEXT("Check Health exposes the failure error"),
        HealthNode != nullptr ? HealthNode->FindPin(TEXT("Error"), EGPD_Output) : nullptr);
    TestNotNull(
        TEXT("Send Custom Route is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseCustomRouteAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseCustomRouteAsyncAction,
                SendCustomRoute)));

    UK2Node_AsyncAction* GetRecordNode = AddAsyncConsumerNode(
        Graph,
        UOpenPocketBaseGetRecordAsyncAction::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseGetRecordAsyncAction, GetRecord));
    TestNotNull(
        TEXT("Get Record is available as an async Blueprint node"),
        GetRecordNode);
    UEdGraphPin* CollectionPin =
        GetRecordNode != nullptr ? GetRecordNode->FindPin(TEXT("Collection"), EGPD_Input) : nullptr;
    TestNotNull(TEXT("Record actions accept a collection value"), CollectionPin);
    TestTrue(
        TEXT("Record actions use the Open PocketBase collection type"),
            CollectionPin != nullptr &&
            CollectionPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
            CollectionPin->PinType.PinSubCategoryObject != nullptr &&
            CollectionPin->PinType.PinSubCategoryObject->GetName() == TEXT("OpenPocketBaseCollection"));
    TestNull(
        TEXT("Record actions do not repeat the client pin"),
        GetRecordNode != nullptr
            ? GetRecordNode->FindPin(TEXT("PocketBaseClient"), EGPD_Input)
            : nullptr);

    UK2Node_CallFunction* CollectionNode = NewObject<UK2Node_CallFunction>(Graph);
    CollectionNode->SetFromFunction(UOpenPocketBaseClient::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, Collection)));
    CollectionNode->CreateNewGuid();
    CollectionNode->PostPlacedNewNode();
    CollectionNode->AllocateDefaultPins();
    Graph->AddNode(CollectionNode, true, false);
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    TestTrue(
        TEXT("A retrieved client connects directly to Collection"),
        Schema->TryCreateConnection(
            GetClientNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
            CollectionNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input)));
    TestTrue(
        TEXT("A Collection value connects directly to record actions"),
        Schema->TryCreateConnection(
            CollectionNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
            CollectionPin));
    TestNotNull(
        TEXT("List Records is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseListRecordsAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseListRecordsAsyncAction, ListRecords)));
    TestNotNull(
        TEXT("Get Full Record List is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseGetFullListAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseGetFullListAsyncAction, GetFullList)));
    TestNotNull(
        TEXT("Get First Record is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseGetFirstRecordAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseGetFirstRecordAsyncAction, GetFirstRecord)));
    TestNotNull(
        TEXT("Create Record is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseCreateRecordAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseCreateRecordAsyncAction, CreateRecord)));
    TestNotNull(
        TEXT("Create Record with Files is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseCreateRecordWithFilesAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseCreateRecordWithFilesAsyncAction,
                CreateRecordWithFiles)));
    TestNotNull(
        TEXT("Update Record is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseUpdateRecordAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseUpdateRecordAsyncAction, UpdateRecord)));
    TestNotNull(
        TEXT("Update Record with Files is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseUpdateRecordWithFilesAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseUpdateRecordWithFilesAsyncAction,
                UpdateRecordWithFiles)));
    TestNotNull(
        TEXT("Delete Record is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseDeleteRecordAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseDeleteRecordAsyncAction, DeleteRecord)));
    TestNotNull(
        TEXT("Send Batch is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseSendBatchAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseSendBatchAsyncAction, SendBatch)));
    TestNotNull(
        TEXT("Get Protected File Token is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseGetFileTokenAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseGetFileTokenAsyncAction, GetProtectedFileToken)));
    TestNotNull(
        TEXT("Download File is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseDownloadFileAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseDownloadFileAsyncAction, DownloadFile)));
    TestNotNull(
        TEXT("Refresh Auth is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseRefreshAuthAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRefreshAuthAsyncAction, RefreshAuth)));
    TestNotNull(
        TEXT("Restore Session is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseRestoreSessionAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRestoreSessionAsyncAction, RestoreSession)));
    TestNotNull(
        TEXT("Log In with Password is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBasePasswordAuthAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBasePasswordAuthAsyncAction, LogInWithPassword)));
    TestNotNull(
        TEXT("List Authentication Methods is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseListAuthMethodsAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseListAuthMethodsAsyncAction,
                ListAuthenticationMethods)));
    TestNotNull(
        TEXT("Request One-Time Password is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseRequestOtpAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseRequestOtpAsyncAction,
                RequestOneTimePassword)));
    TestNotNull(
        TEXT("Log In with One-Time Password is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseOtpAuthAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseOtpAuthAsyncAction,
                LogInWithOneTimePassword)));
    TestNotNull(
        TEXT("Begin Manual OAuth2 is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseBeginOAuth2AsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseBeginOAuth2AsyncAction,
                BeginManualOAuth2)));
    TestNotNull(
        TEXT("Complete Manual OAuth2 is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseCompleteOAuth2AsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseCompleteOAuth2AsyncAction,
                CompleteManualOAuth2)));
    TestNotNull(
        TEXT("OAuth exchange exposes an MFA Required terminal delegate"),
        UOpenPocketBaseCompleteOAuth2AsyncAction::StaticClass()->FindPropertyByName(
            TEXT("MfaRequired")));
    TestNotNull(
        TEXT("Log In with OAuth2 is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAssistedOAuth2AsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAssistedOAuth2AsyncAction,
                LogInWithOAuth2)));
    TestNotNull(
        TEXT("Assisted OAuth exposes an MFA Required terminal delegate"),
        UOpenPocketBaseAssistedOAuth2AsyncAction::StaticClass()->FindPropertyByName(
            TEXT("MfaRequired")));
    TestNotNull(
        TEXT("Password login exposes an MFA Required terminal delegate"),
        UOpenPocketBasePasswordAuthAsyncAction::StaticClass()->FindPropertyByName(
            TEXT("MfaRequired")));
    TestNotNull(
        TEXT("Request Password Reset is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAccountAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                RequestPasswordReset)));
    TestNotNull(
        TEXT("Confirm Password Reset is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAccountAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                ConfirmPasswordReset)));
    TestNotNull(
        TEXT("Request Verification is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAccountAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                RequestVerification)));
    TestNotNull(
        TEXT("Confirm Verification is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAccountAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                ConfirmVerification)));
    TestNotNull(
        TEXT("Request Email Change is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAccountAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                RequestEmailChange)));
    TestNotNull(
        TEXT("Confirm Email Change is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAccountAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                ConfirmEmailChange)));
    TestNotNull(
        TEXT("List Linked External Auths is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseListExternalAuthsAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseListExternalAuthsAsyncAction,
                ListLinkedExternalAuths)));
    TestNotNull(
        TEXT("Unlink External Auth is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAccountAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                UnlinkExternalAuth)));

    UK2Node_CallFunction* FieldNode = NewObject<UK2Node_CallFunction>(Graph);
    FieldNode->SetFromFunction(UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, GetStringFieldState)));
    FieldNode->CreateNewGuid();
    FieldNode->PostPlacedNewNode();
    FieldNode->AllocateDefaultPins();
    Graph->AddNode(FieldNode, true, false);

    UK2Node_CallFunction* BodyNode = NewObject<UK2Node_CallFunction>(Graph);
    BodyNode->SetFromFunction(UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, WithStringField)));
    BodyNode->CreateNewGuid();
    BodyNode->PostPlacedNewNode();
    BodyNode->AllocateDefaultPins();
    Graph->AddNode(BodyNode, true, false);

    UK2Node_CallFunction* FilterNode = NewObject<UK2Node_CallFunction>(Graph);
    FilterNode->SetFromFunction(UOpenPocketBaseFilterLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseFilterLibrary, BooleanFilter)));
    FilterNode->CreateNewGuid();
    FilterNode->PostPlacedNewNode();
    FilterNode->AllocateDefaultPins();
    Graph->AddNode(FilterNode, true, false);

    UK2Node_CallFunction* FileUrlNode = NewObject<UK2Node_CallFunction>(Graph);
    FileUrlNode->SetFromFunction(UOpenPocketBaseFileLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseFileLibrary, TryBuildFileUrl)));
    FileUrlNode->CreateNewGuid();
    FileUrlNode->PostPlacedNewNode();
    FileUrlNode->AllocateDefaultPins();
    Graph->AddNode(FileUrlNode, true, false);

    UK2Node_CallFunction* BatchNode = NewObject<UK2Node_CallFunction>(Graph);
    BatchNode->SetFromFunction(UOpenPocketBaseBatchLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseBatchLibrary, WithCreate)));
    BatchNode->CreateNewGuid();
    BatchNode->PostPlacedNewNode();
    BatchNode->AllocateDefaultPins();
    Graph->AddNode(BatchNode, true, false);

    UK2Node_CallFunction* LogoutNode = NewObject<UK2Node_CallFunction>(Graph);
    LogoutNode->SetFromFunction(UOpenPocketBaseClient::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, Logout)));
    LogoutNode->CreateNewGuid();
    LogoutNode->PostPlacedNewNode();
    LogoutNode->AllocateDefaultPins();
    Graph->AddNode(LogoutNode, true, false);

    TestNotNull(
        TEXT("Blueprint clients publish session changes"),
        UOpenPocketBaseClient::StaticClass()->FindPropertyByName(TEXT("SessionChanged")));

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_AsyncAction* AsyncNode = Cast<UK2Node_AsyncAction>(Node);
        if (AsyncNode == nullptr)
        {
            continue;
        }

        TestNotNull(
            *FString::Printf(
                TEXT("%s exposes the failure error"),
                *AsyncNode->GetNodeTitle(ENodeTitleType::ListView).ToString()),
            AsyncNode->FindPin(TEXT("Error"), EGPD_Output));
        TestNull(
            *FString::Printf(
                TEXT("%s resolves its Game Instance from the client"),
                *AsyncNode->GetNodeTitle(ENodeTitleType::ListView).ToString()),
            AsyncNode->FindPin(TEXT("WorldContextObject"), EGPD_Input));
    }

    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
    TestTrue(TEXT("The Blueprint consumer compiles without errors"), Blueprint->Status != BS_Error);
    return true;
}

#endif
