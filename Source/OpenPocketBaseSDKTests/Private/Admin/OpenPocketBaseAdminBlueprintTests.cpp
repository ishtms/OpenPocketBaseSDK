#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AsyncActions/OpenPocketBaseAdminAsyncActions.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_AsyncAction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "OpenPocketBaseAdminBlueprintClient.h"
#include "UObject/Package.h"

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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAdminBlueprintSurfaceTest,
    "OpenPocketBase.Admin.BlueprintSurfaceIsDevelopmentOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAdminBlueprintSurfaceTest::RunTest(const FString& Parameters)
{
    VerifyDevelopmentOnlyFunction(*this, UOpenPocketBaseAdminClient::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseAdminClient, CreateAdminClient));

    struct FExpectedFunction
    {
        UClass* Owner;
        FName Name;
    };
    const TArray<FExpectedFunction> Functions = {
        {UOpenPocketBaseAuthenticateSuperuserAsyncAction::StaticClass(), TEXT("AuthenticateSuperuser")},
        {UOpenPocketBaseAdminPageAsyncAction::StaticClass(), TEXT("ListCollections")},
        {UOpenPocketBaseAdminPageAsyncAction::StaticClass(), TEXT("ListLogs")},
        {UOpenPocketBaseAdminDocumentAsyncAction::StaticClass(), TEXT("GetCollection")},
        {UOpenPocketBaseAdminDocumentAsyncAction::StaticClass(), TEXT("CreateCollection")},
        {UOpenPocketBaseAdminDocumentAsyncAction::StaticClass(), TEXT("UpdateCollection")},
        {UOpenPocketBaseAdminDocumentAsyncAction::StaticClass(), TEXT("GetSettings")},
        {UOpenPocketBaseAdminDocumentAsyncAction::StaticClass(), TEXT("UpdateSettings")},
        {UOpenPocketBaseAdminDocumentAsyncAction::StaticClass(), TEXT("GetLog")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("DeleteCollection")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("ImportCollections")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("TestS3")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("TestEmail")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("CreateBackup")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("UploadBackup")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("RestoreBackup")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("DeleteBackup")},
        {UOpenPocketBaseAdminCommandAsyncAction::StaticClass(), TEXT("RunCron")},
        {UOpenPocketBaseAdminBackupListAsyncAction::StaticClass(), TEXT("ListBackups")},
        {UOpenPocketBaseAdminBackupDownloadAsyncAction::StaticClass(), TEXT("DownloadBackup")},
        {UOpenPocketBaseAdminDocumentListAsyncAction::StaticClass(), TEXT("ListCrons")},
        {UOpenPocketBaseAdminSqlAsyncAction::StaticClass(), TEXT("RunSql")},
        {UOpenPocketBaseAdminImpersonateAsyncAction::StaticClass(), TEXT("Impersonate")}};
    for (const FExpectedFunction& Function : Functions)
    {
        VerifyDevelopmentOnlyFunction(*this, Function.Owner, Function.Name);
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
    for (const FExpectedFunction& Function : Functions)
    {
        TestNotNull(
            *FString::Printf(TEXT("%s compiles as an async Blueprint node"),
                *Function.Name.ToString()),
            AddAdminAsyncNode(Graph, Function.Owner, Function.Name));
    }
    FKismetEditorUtilities::CompileBlueprint(
        Blueprint,
        EBlueprintCompileOptions::SkipGarbageCollection);
    TestEqual(TEXT("The privileged Blueprint-only consumer compiles"),
        Blueprint->Status, BS_UpToDate);
    return true;
}

#endif
