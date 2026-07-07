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
#include "OpenPocketBaseFileLibrary.h"
#include "OpenPocketBaseBatchLibrary.h"
#include "OpenPocketBaseRecordLibrary.h"
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

    TestNotNull(
        TEXT("Get Record is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseGetRecordAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseGetRecordAsyncAction, GetRecord)));
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

    UK2Node_CallFunction* FieldNode = NewObject<UK2Node_CallFunction>(Graph);
    FieldNode->SetFromFunction(UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, GetStringFieldState)));
    FieldNode->CreateNewGuid();
    FieldNode->PostPlacedNewNode();
    FieldNode->AllocateDefaultPins();
    Graph->AddNode(FieldNode, true, false);

    UK2Node_CallFunction* BodyNode = NewObject<UK2Node_CallFunction>(Graph);
    BodyNode->SetFromFunction(UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, SetRecordBodyStringField)));
    BodyNode->CreateNewGuid();
    BodyNode->PostPlacedNewNode();
    BodyNode->AllocateDefaultPins();
    Graph->AddNode(BodyNode, true, false);

    UK2Node_CallFunction* FilterNode = NewObject<UK2Node_CallFunction>(Graph);
    FilterNode->SetFromFunction(UOpenPocketBaseFilterLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseFilterLibrary, BindFilter)));
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
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseBatchLibrary, AddCreate)));
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

    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
    TestTrue(TEXT("The Blueprint consumer compiles without errors"), Blueprint->Status != BS_Error);
    return true;
}

#endif
