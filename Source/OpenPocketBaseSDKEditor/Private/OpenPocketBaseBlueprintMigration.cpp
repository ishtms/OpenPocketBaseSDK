#include "OpenPocketBaseBlueprintMigration.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_MakeStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "OpenPocketBaseBatch.h"
#include "OpenPocketBaseBatchLibrary.h"
#include "OpenPocketBaseRecord.h"
#include "OpenPocketBaseRecordLibrary.h"

namespace
{
UFunction* FindMigrationFunction(const UScriptStruct* Struct)
{
    if (Struct == FOpenPocketBaseRecordOptions::StaticStruct())
    {
        return UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, MakeLegacyRecordOptions));
    }
    if (Struct == FOpenPocketBaseListOptions::StaticStruct())
    {
        return UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, MakeLegacyListOptions));
    }
    if (Struct == FOpenPocketBaseFullListOptions::StaticStruct())
    {
        return UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, MakeLegacyFullListOptions));
    }
    if (Struct == FOpenPocketBaseBatchOptions::StaticStruct())
    {
        return UOpenPocketBaseBatchLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseBatchLibrary, MakeLegacyBatchOptions));
    }
    return nullptr;
}
}

int32 FOpenPocketBaseBlueprintMigration::UpgradeNativeMakeNodes(UBlueprint& Blueprint)
{
    TArray<UEdGraph*> Graphs;
    Blueprint.GetAllGraphs(Graphs);
    int32 UpgradedCount = 0;
    for (UEdGraph* Graph : Graphs)
    {
        if (Graph == nullptr)
        {
            continue;
        }

        TArray<UK2Node_MakeStruct*> Nodes;
        Graph->GetNodesOfClass(Nodes);
        const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
        if (Schema == nullptr)
        {
            continue;
        }

        for (UK2Node_MakeStruct* Node : Nodes)
        {
            UFunction* Function = Node != nullptr
                ? FindMigrationFunction(Node->StructType)
                : nullptr;
            if (Function == nullptr)
            {
                continue;
            }

            TMap<FName, FName> PinMap;
            PinMap.Add(Node->StructType->GetFName(), UEdGraphSchema_K2::PN_ReturnValue);
            if (Schema->ConvertDeprecatedNodeToFunctionCall(
                    Node,
                    Function,
                    PinMap,
                    Graph) != nullptr)
            {
                ++UpgradedCount;
            }
        }
    }

    if (UpgradedCount > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(&Blueprint);
    }
    return UpgradedCount;
}
