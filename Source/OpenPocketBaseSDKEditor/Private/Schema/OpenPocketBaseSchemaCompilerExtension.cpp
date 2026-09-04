// Copyright 2026 Ishtmeet Singh.

#include "Schema/OpenPocketBaseSchemaCompilerExtension.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "KismetCompiler.h"
#include "OpenPocketBaseBlueprintClient.h"
#include "OpenPocketBaseSchemaPicker.h"
#include "Schema/OpenPocketBaseSchemaGraphContext.h"

namespace
{
bool IsOptionalReference(const UEdGraphPin& Pin)
{
    return Pin.GetOwningNode()->GetPinMetaData(
        Pin.PinName,
        TEXT("OpenPocketBaseOptionalSchemaRef")) == TEXT("true");
}

bool IsCollectionHandle(const UScriptStruct* Struct)
{
    return Struct == FOpenPocketBaseCollection::StaticStruct() ||
        Struct == FOpenPocketBaseWritableCollection::StaticStruct() ||
        Struct == FOpenPocketBaseAuthCollection::StaticStruct();
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
                if (IsCollectionHandle(ReferenceStruct))
                {
                    if (!IsOptionalReference(*Pin))
                    {
                        CompilationContext.MessageLog.Error(
                            *FString::Printf(
                                TEXT("@@ requires PocketBase collection pin '%s' to be connected."),
                                *Pin->GetDisplayName().ToString()),
                            Node);
                    }
                    continue;
                }

                FText Message;
                EOpenPocketBaseSchemaReferenceStatus Status;
                if (FOpenPocketBaseSchemaPickerModel::SupportsCollectionStruct(ReferenceStruct))
                {
                    FOpenPocketBaseCollectionRef Ref;
                    FOpenPocketBaseSchemaPickerModel::ParseCollectionDefault(
                        Pin->GetDefaultAsString(),
                        Ref);
                    Status = FOpenPocketBaseSchemaPickerModel::ValidateCollection(
                        ReferenceStruct,
                        Ref,
                        OpenPocketBase::Editor::FindCollectionRequirement(*Pin),
                        Message);
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
                        Node->GetPinMetaData(
                            Pin->PinName,
                            TEXT("OpenPocketBaseFieldAccess")) == TEXT("Write"),
                        Message);
                    if (Status == EOpenPocketBaseSchemaReferenceStatus::Valid)
                    {
                        FOpenPocketBaseCollectionRef Collection;
                        if (OpenPocketBase::Editor::FindCollectionContext(*Pin, Collection) &&
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
