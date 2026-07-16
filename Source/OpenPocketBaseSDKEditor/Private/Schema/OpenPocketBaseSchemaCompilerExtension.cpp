#include "Schema/OpenPocketBaseSchemaCompilerExtension.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "KismetCompiler.h"
#include "OpenPocketBaseSchemaPicker.h"

namespace
{
bool IsOptionalReference(const UEdGraphPin& Pin)
{
    return Pin.GetOwningNode()->GetPinMetaData(
        Pin.PinName,
        TEXT("OpenPocketBaseOptionalSchemaRef")) == TEXT("true");
}

bool FindLiteralCollection(
    const UEdGraphNode& Node,
    const UEdGraphPin& FieldPin,
    FOpenPocketBaseCollectionRef& OutCollection)
{
    OutCollection = {};
    for (const UEdGraphPin* Pin : Node.Pins)
    {
        if (Pin == nullptr ||
            Pin == &FieldPin ||
            Pin->Direction != EGPD_Input ||
            !Pin->LinkedTo.IsEmpty() ||
            !FOpenPocketBaseSchemaPickerModel::SupportsCollectionStruct(
                Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get())))
        {
            continue;
        }
        if (FOpenPocketBaseSchemaPickerModel::ParseCollectionDefault(
                Pin->GetDefaultAsString(),
                OutCollection) &&
            OutCollection.IsSet())
        {
            return true;
        }
    }
    return false;
}

void ReportInvalidReference(
    const FKismetCompilerContext& CompilationContext,
    UEdGraphNode& Node,
    const UEdGraphPin& Pin,
    const FText& Message)
{
    CompilationContext.MessageLog.Error(
        *FString::Printf(
            TEXT("@@ has an invalid PocketBase schema value on pin '%s': %s"),
            *Pin.GetDisplayName().ToString(),
            *Message.ToString()),
        &Node);
}
}

void UOpenPocketBaseSchemaCompilerExtension::ProcessBlueprintCompiled(
    const FKismetCompilerContext& CompilationContext,
    const FBlueprintCompiledData& Data)
{
    if (CompilationContext.Blueprint == nullptr)
    {
        return;
    }

    TArray<UEdGraph*> Graphs;
    CompilationContext.Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        if (Graph == nullptr)
        {
            continue;
        }

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node == nullptr)
            {
                continue;
            }

            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin == nullptr ||
                    Pin->Direction != EGPD_Input ||
                    !Pin->LinkedTo.IsEmpty())
                {
                    continue;
                }

                const UScriptStruct* ReferenceStruct =
                    Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get());
                FText Message;
                EOpenPocketBaseSchemaReferenceStatus Status;
                if (FOpenPocketBaseSchemaPickerModel::SupportsCollectionStruct(ReferenceStruct))
                {
                    FOpenPocketBaseCollectionRef Ref;
                    FOpenPocketBaseSchemaPickerModel::ParseCollectionDefault(
                        Pin->GetDefaultAsString(),
                        Ref);
                    Status = FOpenPocketBaseSchemaPickerModel::ValidateCollection(Ref, Message);
                }
                else if (FOpenPocketBaseSchemaPickerModel::SupportsFieldStruct(ReferenceStruct))
                {
                    FOpenPocketBaseFieldRef Ref;
                    FOpenPocketBaseSchemaPickerModel::ParseFieldDefault(
                        ReferenceStruct,
                        Pin->GetDefaultAsString(),
                        Ref);
                    Status = FOpenPocketBaseSchemaPickerModel::ValidateField(
                        ReferenceStruct,
                        Ref,
                        Message);
                    if (Status == EOpenPocketBaseSchemaReferenceStatus::Valid)
                    {
                        FOpenPocketBaseCollectionRef Collection;
                        if (FindLiteralCollection(*Node, *Pin, Collection) &&
                            !Ref.BelongsTo(Collection))
                        {
                            Status = EOpenPocketBaseSchemaReferenceStatus::MissingCollection;
                            Message = FText::FromString(
                                TEXT("The field belongs to a different collection."));
                        }
                    }
                }
                else
                {
                    continue;
                }

                if (Status == EOpenPocketBaseSchemaReferenceStatus::Valid ||
                    (Status == EOpenPocketBaseSchemaReferenceStatus::Empty &&
                     IsOptionalReference(*Pin)))
                {
                    continue;
                }
                ReportInvalidReference(CompilationContext, *Node, *Pin, Message);
            }
        }
    }
}
