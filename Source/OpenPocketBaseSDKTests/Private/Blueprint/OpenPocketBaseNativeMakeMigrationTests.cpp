#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MakeStruct.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "OpenPocketBaseBatch.h"
#include "OpenPocketBaseBatchLibrary.h"
#include "OpenPocketBaseBlueprintMigration.h"
#include "OpenPocketBaseRecord.h"
#include "OpenPocketBaseRecordLibrary.h"

namespace
{
UK2Node_MakeStruct* AddLegacyMakeNode(UEdGraph& Graph, UScriptStruct* Struct)
{
    UK2Node_MakeStruct* Node = NewObject<UK2Node_MakeStruct>(&Graph);
    Node->StructType = Struct;
    Node->CreateNewGuid();
    Node->PostPlacedNewNode();
    Node->AllocateDefaultPins();
    Graph.AddNode(Node, true, false);
    return Node;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseNativeMakeMigrationTest,
    "OpenPocketBase.Blueprint.Compatibility.MigratesLegacyMakeStructNodes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseNativeMakeMigrationTest::RunTest(const FString& Parameters)
{
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        UObject::StaticClass(),
        GetTransientPackage(),
        MakeUniqueObjectName(
            GetTransientPackage(),
            UBlueprint::StaticClass(),
            TEXT("BP_OpenPocketBaseLegacyMakes")),
        BPTYPE_Normal,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(),
        NAME_None);
    if (!TestNotNull(TEXT("The migration Blueprint exists"), Blueprint) ||
        !TestTrue(TEXT("The migration Blueprint has an event graph"), !Blueprint->UbergraphPages.IsEmpty()))
    {
        return false;
    }

    UEdGraph* Graph = Blueprint->UbergraphPages[0];
    UK2Node_MakeStruct* ListOptions = AddLegacyMakeNode(
        *Graph,
        FOpenPocketBaseListOptions::StaticStruct());
    UK2Node_MakeStruct* FullListOptions = AddLegacyMakeNode(
        *Graph,
        FOpenPocketBaseFullListOptions::StaticStruct());
    UK2Node_MakeStruct* BatchOptions = AddLegacyMakeNode(
        *Graph,
        FOpenPocketBaseBatchOptions::StaticStruct());
    AddLegacyMakeNode(*Graph, FOpenPocketBaseRecordOptions::StaticStruct());

    ListOptions->FindPinChecked(TEXT("PerPage"))->DefaultValue = TEXT("7");
    FullListOptions->FindPinChecked(TEXT("MaxItems"))->DefaultValue = TEXT("12");
    BatchOptions->FindPinChecked(TEXT("MaxOperations"))->DefaultValue = TEXT("3");
    GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(
        ListOptions->FindPinChecked(FOpenPocketBaseListOptions::StaticStruct()->GetFName()),
        FullListOptions->FindPinChecked(TEXT("ListOptions")));

    TestEqual(
        TEXT("Every legacy options node is migrated"),
        FOpenPocketBaseBlueprintMigration::UpgradeNativeMakeNodes(*Blueprint),
        4);

    TArray<UK2Node_MakeStruct*> RemainingMakes;
    Graph->GetNodesOfClass(RemainingMakes);
    TestEqual(TEXT("No invalid Make Struct node remains"), RemainingMakes.Num(), 0);

    UK2Node_CallFunction* MigratedFullList = nullptr;
    UK2Node_CallFunction* MigratedBatch = nullptr;
    TArray<UK2Node_CallFunction*> Calls;
    Graph->GetNodesOfClass(Calls);
    for (UK2Node_CallFunction* Call : Calls)
    {
        const FName FunctionName = Call->FunctionReference.GetMemberName();
        if (FunctionName == GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseRecordLibrary,
                MakeLegacyFullListOptions))
        {
            MigratedFullList = Call;
        }
        else if (FunctionName == GET_FUNCTION_NAME_CHECKED(
                     UOpenPocketBaseBatchLibrary,
                     MakeLegacyBatchOptions))
        {
            MigratedBatch = Call;
        }
    }

    if (TestNotNull(TEXT("The full-list node uses its compatibility maker"), MigratedFullList))
    {
        TestEqual(
            TEXT("The full-list item limit is preserved"),
            MigratedFullList->FindPinChecked(TEXT("MaxItems"))->DefaultValue,
            FString(TEXT("12")));
        TestEqual(
            TEXT("The nested List Options connection is preserved"),
            MigratedFullList->FindPinChecked(TEXT("ListOptions"))->LinkedTo.Num(),
            1);
    }
    if (TestNotNull(TEXT("The batch node uses its compatibility maker"), MigratedBatch))
    {
        TestEqual(
            TEXT("The batch operation limit is preserved"),
            MigratedBatch->FindPinChecked(TEXT("MaxOperations"))->DefaultValue,
            FString(TEXT("3")));
    }

    FKismetEditorUtilities::CompileBlueprint(
        Blueprint,
        EBlueprintCompileOptions::SkipGarbageCollection);
    TestTrue(TEXT("The migrated Blueprint compiles"), Blueprint->Status != BS_Error);
    return true;
}

#endif
