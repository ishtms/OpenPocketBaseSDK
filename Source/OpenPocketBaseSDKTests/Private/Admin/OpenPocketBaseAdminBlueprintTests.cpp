#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AsyncActions/OpenPocketBaseAdminAsyncActions.h"
#include "Blueprint/OpenPocketBaseAsyncActionTestReceiver.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_AsyncAction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "OpenPocketBaseAdminBlueprintClient.h"
#include "OpenPocketBaseAdminQueryLibrary.h"
#include "OpenPocketBaseSchema.h"
#include "OpenPocketBaseSchemaPicker.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include <type_traits>

using FExpectedAdminGetCollection = UOpenPocketBaseAdminDocumentAsyncAction* (*)(
    UOpenPocketBaseAdminClient*,
    FOpenPocketBaseCollectionRef,
    FOpenPocketBaseRequestOptions);
using FExpectedAdminUpdateCollection = UOpenPocketBaseAdminDocumentAsyncAction* (*)(
    UOpenPocketBaseAdminClient*,
    FOpenPocketBaseCollectionRef,
    FOpenPocketBaseAdminDocument,
    FOpenPocketBaseRequestOptions);
using FExpectedAdminDeleteCollection = UOpenPocketBaseAdminCommandAsyncAction* (*)(
    UOpenPocketBaseAdminClient*,
    FOpenPocketBaseCollectionRef,
    FOpenPocketBaseRequestOptions);
using FExpectedAdminImpersonate = UOpenPocketBaseAdminImpersonateAsyncAction* (*)(
    UOpenPocketBaseAdminClient*,
    FOpenPocketBaseAuthCollectionRef,
    FString,
    int64,
    FOpenPocketBaseRequestOptions);
using FExpectedAdminListCollections = UOpenPocketBaseAdminPageAsyncAction* (*)(
    UOpenPocketBaseAdminClient*,
    FOpenPocketBaseAdminCollectionListOptions);
using FExpectedAdminListLogs = UOpenPocketBaseAdminPageAsyncAction* (*)(
    UOpenPocketBaseAdminClient*,
    FOpenPocketBaseAdminLogListOptions);

static_assert(std::is_same_v<
    decltype(&UOpenPocketBaseAdminDocumentAsyncAction::GetCollection),
    FExpectedAdminGetCollection>);
static_assert(std::is_same_v<
    decltype(&UOpenPocketBaseAdminDocumentAsyncAction::UpdateCollection),
    FExpectedAdminUpdateCollection>);
static_assert(std::is_same_v<
    decltype(&UOpenPocketBaseAdminCommandAsyncAction::DeleteCollection),
    FExpectedAdminDeleteCollection>);
static_assert(std::is_same_v<
    decltype(&UOpenPocketBaseAdminImpersonateAsyncAction::Impersonate),
    FExpectedAdminImpersonate>);
static_assert(std::is_same_v<
    decltype(&UOpenPocketBaseAdminPageAsyncAction::ListCollections),
    FExpectedAdminListCollections>);
static_assert(std::is_same_v<
    decltype(&UOpenPocketBaseAdminPageAsyncAction::ListLogs),
    FExpectedAdminListLogs>);

namespace
{
bool VerifyDevelopmentOnlyFunction(
    FAutomationTestBase& Test,
    UClass* Owner,
    const FName FunctionName)
{
    const UFunction* Function = Owner != nullptr
        ? Owner->FindFunctionByName(FunctionName)
        : nullptr;
    Test.TestNotNull(
        *FString::Printf(TEXT("%s exposes %s"), *GetNameSafe(Owner), *FunctionName.ToString()),
        Function);
    if (Function == nullptr)
    {
        return false;
    }
    Test.TestTrue(
        *FString::Printf(TEXT("%s is disabled in Shipping by default"),
            *FunctionName.ToString()),
        Function->HasMetaData(TEXT("DevelopmentOnly")));
    return true;
}

UK2Node_AsyncAction* AddAdminAsyncNode(
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAdminShutdownAsyncActionTest,
    "OpenPocketBase.Admin.Blueprint.ShutdownClientBroadcastsMissingClient",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAdminShutdownAsyncActionTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("http://127.0.0.1:18094");
    FOpenPocketBaseAdminPolicy Policy;
    Policy.bEnablePrivilegedRequests = true;
    FOpenPocketBaseError CreateError;
    UOpenPocketBaseAdminClient* Client = UOpenPocketBaseAdminClient::Create(
        GetTransientPackage(),
        Config,
        Policy,
        CreateError);
    if (!TestNotNull(TEXT("The admin client is created"), Client))
    {
        AddError(CreateError.Message);
        return false;
    }
    Client->Shutdown();

    UOpenPocketBaseAsyncActionTestReceiver* Receiver =
        NewObject<UOpenPocketBaseAsyncActionTestReceiver>();
    UOpenPocketBaseAdminDocumentAsyncAction* Action =
        UOpenPocketBaseAdminDocumentAsyncAction::GetDynamicCollection(
            Client,
            TEXT("sdk_tasks"),
            {});
    Action->Failed.AddDynamic(
        Receiver,
        &UOpenPocketBaseAsyncActionTestReceiver::HandleAdminDocumentFailure);
    Action->Activate();

    TestTrue(TEXT("A shut-down admin client reaches the Failed pin"), Receiver->bFailed);
    TestEqual(
        TEXT("The failure is an invalid argument"),
        Receiver->Error.Kind,
        EOpenPocketBaseErrorKind::InvalidArgument);
    TestEqual(
        TEXT("The failure identifies the unavailable admin client"),
        Receiver->Error.Message,
        FString(TEXT("The privileged PocketBase client is missing or has already shut down. Create or retrieve an active admin client before starting this operation.")));
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAdminBlueprintSurfaceTest,
    "OpenPocketBase.Admin.BlueprintSurfaceIsDevelopmentOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAdminBlueprintSurfaceTest::RunTest(const FString& Parameters)
{
    const UFunction* InitializeAdmin = UOpenPocketBaseAdminClient::StaticClass()
        ->FindFunctionByName(TEXT("InitializeAdminClient"));
    TestNotNull(TEXT("Initialize Privileged PocketBase exists"), InitializeAdmin);
    if (InitializeAdmin != nullptr)
    {
        TestTrue(
            TEXT("Initialize Privileged PocketBase is disabled in Shipping by default"),
            InitializeAdmin->HasMetaData(TEXT("DevelopmentOnly")));
        TestEqual(
            TEXT("Admin initialization resolves its world automatically"),
            InitializeAdmin->GetMetaData(TEXT("WorldContext")),
            FString(TEXT("WorldContextObject")));
        TestEqual(
            TEXT("Admin initialization hides its world pin"),
            InitializeAdmin->GetMetaData(TEXT("HidePin")),
            FString(TEXT("WorldContextObject")));
        TestEqual(
            TEXT("Admin initialization exposes success and failure paths"),
            InitializeAdmin->GetMetaData(TEXT("ExpandBoolAsExecs")),
            FString(TEXT("ReturnValue")));
    }
    TestNull(
        TEXT("The old admin client factory is no longer exposed"),
        UOpenPocketBaseAdminClient::StaticClass()->FindFunctionByName(
            TEXT("CreateAdminClient")));

    const UFunction* ListCollections =
        UOpenPocketBaseAdminPageAsyncAction::StaticClass()->FindFunctionByName(
            TEXT("ListCollections"));
    const UFunction* ListLogs =
        UOpenPocketBaseAdminPageAsyncAction::StaticClass()->FindFunctionByName(
            TEXT("ListLogs"));
    const FStructProperty* CollectionOptions = ListCollections != nullptr
        ? FindFProperty<FStructProperty>(ListCollections, TEXT("Options"))
        : nullptr;
    const FStructProperty* LogOptions = ListLogs != nullptr
        ? FindFProperty<FStructProperty>(ListLogs, TEXT("Options"))
        : nullptr;
    TestTrue(
        TEXT("Collection lists accept typed collection options"),
        CollectionOptions != nullptr &&
            CollectionOptions->Struct == FOpenPocketBaseAdminCollectionListOptions::StaticStruct());
    TestTrue(
        TEXT("Log lists accept typed log options"),
        LogOptions != nullptr &&
            LogOptions->Struct == FOpenPocketBaseAdminLogListOptions::StaticStruct());
    TestNotNull(
        TEXT("Collection filters have a typed Blueprint builder"),
        UOpenPocketBaseAdminQueryLibrary::StaticClass()->FindFunctionByName(
            TEXT("CollectionTextFilter")));
    TestNotNull(
        TEXT("Log sorts have a typed Blueprint builder"),
        UOpenPocketBaseAdminQueryLibrary::StaticClass()->FindFunctionByName(
            TEXT("ThenSortLogsBy")));

    struct FExpectedFunction
    {
        UClass* Owner;
        FName Name;
    };
    const TArray<FExpectedFunction> Functions = {
        {UOpenPocketBaseAuthenticateSuperuserAsyncAction::StaticClass(), TEXT("AuthenticateSuperuser")},
        {UOpenPocketBaseAdminPageAsyncAction::StaticClass(), TEXT("ListCollections")},
        {UOpenPocketBaseAdminPageAsyncAction::StaticClass(), TEXT("ListDynamicCollections")},
        {UOpenPocketBaseAdminPageAsyncAction::StaticClass(), TEXT("ListLogs")},
        {UOpenPocketBaseAdminPageAsyncAction::StaticClass(), TEXT("ListDynamicLogs")},
        {UOpenPocketBaseAdminDocumentAsyncAction::StaticClass(), TEXT("GetCollection")},
        {UOpenPocketBaseAdminDocumentAsyncAction::StaticClass(), TEXT("GetDynamicCollection")},
        {UOpenPocketBaseAdminDocumentAsyncAction::StaticClass(), TEXT("CreateCollection")},
        {UOpenPocketBaseAdminDocumentAsyncAction::StaticClass(), TEXT("UpdateCollection")},
        {UOpenPocketBaseAdminDocumentAsyncAction::StaticClass(), TEXT("UpdateDynamicCollection")},
        {UOpenPocketBaseAdminDocumentAsyncAction::StaticClass(), TEXT("GetSettings")},
        {UOpenPocketBaseAdminDocumentAsyncAction::StaticClass(), TEXT("UpdateSettings")},
        {UOpenPocketBaseAdminDocumentAsyncAction::StaticClass(), TEXT("GetLog")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("DeleteCollection")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("DeleteDynamicCollection")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("ImportCollections")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("TestS3")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("TestEmail")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("CreateBackup")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("UploadBackupFromPath")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("UploadBackupFromBytes")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("RestoreBackup")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("DeleteBackup")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("RunCron")},
        {UOpenPocketBaseAdminBackupListAsyncAction::StaticClass(), TEXT("ListBackups")},
        {UOpenPocketBaseAdminBackupDownloadAsyncAction::StaticClass(), TEXT("DownloadBackup")},
        {UOpenPocketBaseAdminDocumentListAsyncAction::StaticClass(), TEXT("ListCrons")},
        {UOpenPocketBaseAdminSqlAsyncAction::StaticClass(), TEXT("RunSql")},
        {UOpenPocketBaseAdminImpersonateAsyncAction::StaticClass(), TEXT("Impersonate")},
        {UOpenPocketBaseAdminImpersonateAsyncAction::StaticClass(), TEXT("ImpersonateDynamicUser")}};
    for (const FExpectedFunction& Function : Functions)
    {
        VerifyDevelopmentOnlyFunction(*this, Function.Owner, Function.Name);
        const UFunction* Factory = Function.Owner->FindFunctionByName(Function.Name);
        TestNull(
            *FString::Printf(
                TEXT("%s resolves lifetime through the admin client"),
                *Function.Name.ToString()),
            Factory != nullptr
                ? FindFProperty<FProperty>(Factory, TEXT("WorldContextObject"))
                : nullptr);
    }

    const FName BlueprintName = MakeUniqueObjectName(
        GetTransientPackage(),
        UBlueprint::StaticClass(),
        TEXT("BP_OpenPocketBaseAdminConsumer"));
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        UObject::StaticClass(),
        GetTransientPackage(),
        BlueprintName,
        BPTYPE_Normal,
        NAME_None);
    if (!TestNotNull(TEXT("A privileged Blueprint consumer is created"), Blueprint))
    {
        return false;
    }
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        TEXT("ExerciseOpenPocketBaseAdmin"),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, Graph, true, nullptr);

    UOpenPocketBaseSchema* TestSchema = NewObject<UOpenPocketBaseSchema>(
        GetTransientPackage(),
        TEXT("OpenPocketBaseAdminConsumerSchema"));
    TestSchema->SchemaId = FGuid(233, 377, 610, 987);
    FOpenPocketBaseSchemaCollection TasksCollection;
    TasksCollection.Id = TEXT("tasks_id");
    TasksCollection.Name = TEXT("sdk_tasks");
    TasksCollection.Type = EOpenPocketBaseCollectionType::Base;
    FOpenPocketBaseSchemaCollection UsersCollection;
    UsersCollection.Id = TEXT("users_id");
    UsersCollection.Name = TEXT("sdk_users");
    UsersCollection.Type = EOpenPocketBaseCollectionType::Auth;
    TestSchema->Collections = {TasksCollection, UsersCollection};

    FOpenPocketBaseCollectionRef TasksRef;
    FOpenPocketBaseAuthCollectionRef UsersRef;
    TestTrue(
        TEXT("The admin consumer has a valid collection fixture"),
        TestSchema->MakeCollectionRef(TasksCollection.Id, TasksRef));
    TestTrue(
        TEXT("The admin consumer has a valid auth collection fixture"),
        TestSchema->MakeTypedCollectionRef(UsersCollection.Id, UsersRef));
    const FString TasksDefault =
        FOpenPocketBaseSchemaPickerModel::ExportCollectionDefault(TasksRef);
    const FString UsersDefault =
        FOpenPocketBaseSchemaPickerModel::ExportCollectionDefault(UsersRef);
    int32 CollectionPinCount = 0;
    int32 AuthCollectionPinCount = 0;
    for (const FExpectedFunction& Function : Functions)
    {
        UK2Node_AsyncAction* AsyncNode = AddAdminAsyncNode(
            Graph,
            Function.Owner,
            Function.Name);
        TestNotNull(
            *FString::Printf(TEXT("%s compiles as an async Blueprint node"),
                *Function.Name.ToString()),
            AsyncNode);
        TestNotNull(
            *FString::Printf(TEXT("%s exposes the failure error"),
                *Function.Name.ToString()),
            AsyncNode != nullptr ? AsyncNode->FindPin(TEXT("Error"), EGPD_Output) : nullptr);
        TestNull(
            *FString::Printf(TEXT("%s has no World Context pin"),
                *Function.Name.ToString()),
            AsyncNode != nullptr
                ? AsyncNode->FindPin(TEXT("WorldContextObject"), EGPD_Input)
                : nullptr);
        if (AsyncNode == nullptr)
        {
            continue;
        }
        if (UEdGraphPin* CollectionPin = AsyncNode->FindPin(TEXT("Collection"), EGPD_Input))
        {
            CollectionPin->DefaultValue = TasksDefault;
            ++CollectionPinCount;
        }
        if (UEdGraphPin* AuthCollectionPin =
                AsyncNode->FindPin(TEXT("AuthCollection"), EGPD_Input))
        {
            AuthCollectionPin->DefaultValue = UsersDefault;
            ++AuthCollectionPinCount;
        }
    }
    TestEqual(
        TEXT("Every typed admin collection action receives a schema value"),
        CollectionPinCount,
        3);
    TestEqual(
        TEXT("The typed impersonation action receives an auth schema value"),
        AuthCollectionPinCount,
        1);
    FKismetEditorUtilities::CompileBlueprint(
        Blueprint,
        EBlueprintCompileOptions::SkipGarbageCollection);
    TestEqual(TEXT("The privileged Blueprint-only consumer compiles"),
        Blueprint->Status, BS_UpToDate);
    return true;
}

#endif
