#include "Schema/OpenPocketBaseSchemaGraphContext.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "OpenPocketBaseSchemaPicker.h"

namespace
{
bool IsContextPin(const UEdGraphPin& Pin)
{
    if (Pin.PinType.PinCategory != UEdGraphSchema_K2::PC_Struct)
    {
        return false;
    }
    const UScriptStruct* Struct = Cast<UScriptStruct>(Pin.PinType.PinSubCategoryObject.Get());
    return Struct != nullptr && Struct->GetName().Contains(TEXT("OpenPocketBase"));
}

bool SameCollection(
    const FOpenPocketBaseCollectionRef& Left,
    const FOpenPocketBaseCollectionRef& Right)
{
    return Left.SchemaId == Right.SchemaId && Left.CollectionId == Right.CollectionId;
}

bool AddCandidate(
    const FOpenPocketBaseCollectionRef& Candidate,
    TOptional<FOpenPocketBaseCollectionRef>& Existing)
{
    FOpenPocketBaseCollectionRef Current;
    if (!Candidate.ResolveCurrent(Current))
    {
        return true;
    }
    if (Existing.IsSet() && !SameCollection(Existing.GetValue(), Current))
    {
        return false;
    }
    Existing = MoveTemp(Current);
    return true;
}

bool AddFieldCandidate(
    const FOpenPocketBaseFieldRef& Candidate,
    TOptional<FOpenPocketBaseFieldRef>& Existing)
{
    FOpenPocketBaseFieldRef Current;
    if (!Candidate.ResolveCurrent(Current))
    {
        return true;
    }
    if (Existing.IsSet() &&
        (Existing->SchemaId != Current.SchemaId ||
         Existing->CollectionId != Current.CollectionId ||
         Existing->FieldId != Current.FieldId))
    {
        return false;
    }
    Existing = MoveTemp(Current);
    return true;
}

bool ReadLiteralField(
    const UEdGraphPin& Pin,
    FOpenPocketBaseFieldRef& OutField)
{
    OutField = {};
    const UScriptStruct* Struct =
        Cast<UScriptStruct>(Pin.PinType.PinSubCategoryObject.Get());
    FOpenPocketBaseFieldRef Parsed;
    return Pin.LinkedTo.IsEmpty() &&
        FOpenPocketBaseSchemaPickerModel::SupportsFieldStruct(Struct) &&
        FOpenPocketBaseSchemaPickerModel::ParseFieldDefault(
            Struct,
            Pin.GetDefaultAsString(),
            Parsed) &&
        Parsed.ResolveCurrent(OutField);
}

bool FindRelationTarget(
    const UEdGraphPin& OriginPin,
    const FString& ContextPinName,
    FOpenPocketBaseCollectionRef& OutCollection)
{
    const UEdGraphNode* Node = OriginPin.GetOwningNode();
    const UEdGraphPin* ContextPin = Node != nullptr ? Node->FindPin(*ContextPinName) : nullptr;
    if (ContextPin == nullptr)
    {
        return false;
    }

    TArray<const UEdGraphPin*> CurrentPins{ContextPin};
    TSet<const UEdGraphNode*> Visited;
    for (int32 Depth = 0; Depth < 8 && !CurrentPins.IsEmpty(); ++Depth)
    {
        TArray<const UEdGraphPin*> NextPins;
        for (const UEdGraphPin* Pin : CurrentPins)
        {
            for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                const UEdGraphNode* LinkedNode =
                    LinkedPin != nullptr ? LinkedPin->GetOwningNode() : nullptr;
                if (LinkedNode == nullptr || Visited.Contains(LinkedNode))
                {
                    continue;
                }
                Visited.Add(LinkedNode);

                for (const UEdGraphPin* CandidatePin : LinkedNode->Pins)
                {
                    const UScriptStruct* Struct = CandidatePin != nullptr
                        ? Cast<UScriptStruct>(CandidatePin->PinType.PinSubCategoryObject.Get())
                        : nullptr;
                    if (CandidatePin == nullptr || CandidatePin->Direction != EGPD_Input ||
                        !CandidatePin->LinkedTo.IsEmpty() ||
                        Struct == nullptr ||
                        !Struct->IsChildOf(FOpenPocketBaseRelationFieldRef::StaticStruct()))
                    {
                        continue;
                    }

                    FOpenPocketBaseFieldRef Field;
                    if (!FOpenPocketBaseSchemaPickerModel::ParseFieldDefault(
                            Struct, CandidatePin->GetDefaultAsString(), Field))
                    {
                        continue;
                    }
                    FOpenPocketBaseRelationFieldRef Relation;
                    if (!Field.ResolveCurrentAs(Relation) || Relation.Schema.IsNull())
                    {
                        continue;
                    }
                    UOpenPocketBaseSchema* Schema = Relation.Schema.LoadSynchronous();
                    return Schema != nullptr &&
                        Schema->MakeCollectionRef(Relation.RelatedCollectionId, OutCollection);
                }

                for (const UEdGraphPin* LinkedNodePin : LinkedNode->Pins)
                {
                    if (LinkedNodePin != nullptr && IsContextPin(*LinkedNodePin) &&
                        !LinkedNodePin->LinkedTo.IsEmpty())
                    {
                        NextPins.Add(LinkedNodePin);
                    }
                }
            }
        }
        CurrentPins = MoveTemp(NextPins);
    }
    return false;
}
}

bool OpenPocketBase::Editor::FindCollectionContext(
    const UEdGraphPin& OriginPin,
    FOpenPocketBaseCollectionRef& OutCollection)
{
    OutCollection = {};
    UEdGraphNode* OriginNode = OriginPin.GetOwningNode();
    if (OriginNode == nullptr)
    {
        return false;
    }

    const FString RelationContextPin = OriginNode->GetPinMetaData(
        OriginPin.PinName,
        TEXT("OpenPocketBaseRelationTarget"));
    if (!RelationContextPin.IsEmpty() &&
        FindRelationTarget(OriginPin, RelationContextPin, OutCollection))
    {
        return true;
    }

    TArray<const UEdGraphNode*> CurrentNodes{OriginNode};
    TSet<const UEdGraphNode*> Visited;
    Visited.Add(OriginNode);
    TOptional<FOpenPocketBaseCollectionRef> FieldFallback;
    for (int32 Depth = 0; Depth < 12 && !CurrentNodes.IsEmpty(); ++Depth)
    {
        TOptional<FOpenPocketBaseCollectionRef> ExplicitCollection;
        TArray<const UEdGraphNode*> NextNodes;
        for (const UEdGraphNode* Node : CurrentNodes)
        {
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin == nullptr || Pin == &OriginPin || !IsContextPin(*Pin))
                {
                    continue;
                }

                const UScriptStruct* Struct =
                    Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get());
                if (Pin->Direction == EGPD_Input && Pin->LinkedTo.IsEmpty())
                {
                    if (FOpenPocketBaseSchemaPickerModel::SupportsCollectionStruct(Struct))
                    {
                        FOpenPocketBaseCollectionRef Ref;
                        if (FOpenPocketBaseSchemaPickerModel::ParseCollectionDefault(
                                Pin->GetDefaultAsString(), Ref) &&
                            !AddCandidate(Ref, ExplicitCollection))
                        {
                            return false;
                        }
                    }
                    else if (FOpenPocketBaseSchemaPickerModel::SupportsFieldStruct(Struct))
                    {
                        FOpenPocketBaseFieldRef Field;
                        if (FOpenPocketBaseSchemaPickerModel::ParseFieldDefault(
                                Struct, Pin->GetDefaultAsString(), Field))
                        {
                            FOpenPocketBaseFieldRef CurrentField;
                            if (Field.ResolveCurrent(CurrentField) && !CurrentField.Schema.IsNull())
                            {
                                UOpenPocketBaseSchema* Schema = CurrentField.Schema.LoadSynchronous();
                                FOpenPocketBaseCollectionRef Collection;
                                if (Schema != nullptr && Schema->MakeCollectionRef(
                                        CurrentField.CollectionId, Collection) &&
                                    !AddCandidate(Collection, FieldFallback))
                                {
                                    FieldFallback.Reset();
                                }
                            }
                        }
                    }
                }

                for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    const UEdGraphNode* LinkedNode =
                        LinkedPin != nullptr ? LinkedPin->GetOwningNode() : nullptr;
                    if (LinkedNode != nullptr && !Visited.Contains(LinkedNode))
                    {
                        Visited.Add(LinkedNode);
                        NextNodes.Add(LinkedNode);
                    }
                }
            }
        }

        if (ExplicitCollection.IsSet())
        {
            OutCollection = ExplicitCollection.GetValue();
            return true;
        }
        CurrentNodes = MoveTemp(NextNodes);
    }

    if (FieldFallback.IsSet())
    {
        OutCollection = FieldFallback.GetValue();
        return true;
    }
    return false;
}

EOpenPocketBaseCollectionRequirement OpenPocketBase::Editor::FindCollectionRequirement(
    const UEdGraphPin& OriginPin)
{
    UEdGraphNode* OriginNode = OriginPin.GetOwningNode();
    if (OriginNode == nullptr)
    {
        return EOpenPocketBaseCollectionRequirement::Any;
    }

    EOpenPocketBaseCollectionRequirement Requirement =
        EOpenPocketBaseCollectionRequirement::Any;
    TArray<UEdGraphNode*> CurrentNodes{OriginNode};
    TSet<const UEdGraphNode*> Visited;
    Visited.Add(OriginNode);
    for (int32 Depth = 0; Depth < 12 && !CurrentNodes.IsEmpty(); ++Depth)
    {
        TArray<UEdGraphNode*> NextNodes;
        for (UEdGraphNode* Node : CurrentNodes)
        {
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin == nullptr)
                {
                    continue;
                }

                const FString Access = Node->GetPinMetaData(
                    Pin->PinName,
                    TEXT("OpenPocketBaseCollectionAccess"));
                if (Access == TEXT("Auth"))
                {
                    Requirement = EOpenPocketBaseCollectionRequirement::Auth;
                }
                else if (Access == TEXT("Write") &&
                         Requirement == EOpenPocketBaseCollectionRequirement::Any)
                {
                    Requirement = EOpenPocketBaseCollectionRequirement::Writable;
                }

                if (!IsContextPin(*Pin))
                {
                    continue;
                }
                for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    UEdGraphNode* LinkedNode =
                        LinkedPin != nullptr ? LinkedPin->GetOwningNode() : nullptr;
                    if (LinkedNode != nullptr && !Visited.Contains(LinkedNode))
                    {
                        Visited.Add(LinkedNode);
                        NextNodes.Add(LinkedNode);
                    }
                }
            }
        }
        CurrentNodes = MoveTemp(NextNodes);
    }
    return Requirement;
}

bool FOpenPocketBaseSchemaPickerModel::ResolveFieldFromPinContext(
    const UEdGraphPin& OriginPin,
    const FString& ContextPinName,
    FOpenPocketBaseFieldRef& OutField)
{
    OutField = {};
    const UEdGraphNode* OriginNode = OriginPin.GetOwningNode();
    const UEdGraphPin* ContextPin = OriginNode != nullptr
        ? OriginNode->FindPin(*ContextPinName)
        : nullptr;
    if (ContextPin == nullptr)
    {
        return false;
    }

    if (ReadLiteralField(*ContextPin, OutField))
    {
        return true;
    }

    TArray<const UEdGraphPin*> CurrentPins{ContextPin};
    TSet<const UEdGraphNode*> VisitedNodes;
    TOptional<FOpenPocketBaseFieldRef> Candidate;
    for (int32 Depth = 0; Depth < 12 && !CurrentPins.IsEmpty(); ++Depth)
    {
        TArray<const UEdGraphPin*> NextPins;
        for (const UEdGraphPin* Pin : CurrentPins)
        {
            if (Pin == nullptr)
            {
                continue;
            }

            for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                const UEdGraphNode* LinkedNode =
                    LinkedPin != nullptr ? LinkedPin->GetOwningNode() : nullptr;
                if (LinkedNode == nullptr || VisitedNodes.Contains(LinkedNode))
                {
                    continue;
                }
                VisitedNodes.Add(LinkedNode);

                for (const UEdGraphPin* CandidatePin : LinkedNode->Pins)
                {
                    if (CandidatePin == nullptr || CandidatePin->Direction != EGPD_Input)
                    {
                        continue;
                    }
                    const UScriptStruct* Struct = Cast<UScriptStruct>(
                        CandidatePin->PinType.PinSubCategoryObject.Get());
                    if (!SupportsFieldStruct(Struct))
                    {
                        continue;
                    }

                    if (CandidatePin->LinkedTo.IsEmpty())
                    {
                        FOpenPocketBaseFieldRef Literal;
                        if (ReadLiteralField(*CandidatePin, Literal) &&
                            !AddFieldCandidate(Literal, Candidate))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        NextPins.Add(CandidatePin);
                    }
                }
            }
        }
        CurrentPins = MoveTemp(NextPins);
    }

    if (!Candidate.IsSet())
    {
        return false;
    }
    OutField = Candidate.GetValue();
    return true;
}
