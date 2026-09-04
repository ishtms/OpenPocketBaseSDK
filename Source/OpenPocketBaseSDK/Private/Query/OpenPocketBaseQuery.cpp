// Copyright 2026 Ishtmeet Singh.

#include "OpenPocketBaseQuery.h"

bool FOpenPocketBaseSort::IsSet() const
{
    FOpenPocketBaseFieldRef Current;
    return Field.ResolveCurrent(Current);
}

bool FOpenPocketBaseSort::BelongsTo(
    const FOpenPocketBaseCollectionRef& Collection) const
{
    return Field.BelongsTo(Collection);
}

FString FOpenPocketBaseSort::ToQueryValue() const
{
    if (!IsSet())
    {
        return FString();
    }
    FOpenPocketBaseFieldRef Current;
    Field.ResolveCurrent(Current);
    return Direction == EOpenPocketBaseSortDirection::Descending
        ? TEXT("-") + Current.Name
        : Current.Name;
}

bool FOpenPocketBaseExpand::IsSet() const
{
    if (!bValid || Path.IsEmpty())
    {
        return false;
    }
    for (const FOpenPocketBaseRelationFieldRef& Relation : Path)
    {
        FOpenPocketBaseRelationFieldRef Current;
        if (!Relation.ResolveCurrentAs(Current))
        {
            return false;
        }
    }
    return true;
}

bool FOpenPocketBaseExpand::BelongsTo(
    const FOpenPocketBaseCollectionRef& Collection) const
{
    return IsSet() && Path[0].BelongsTo(Collection);
}

FString FOpenPocketBaseExpand::ToQueryValue() const
{
    if (!IsSet())
    {
        return FString();
    }

    TArray<FString> Names;
    Names.Reserve(Path.Num());
    for (const FOpenPocketBaseRelationFieldRef& Relation : Path)
    {
        FOpenPocketBaseRelationFieldRef Current;
        if (!Relation.ResolveCurrentAs(Current))
        {
            return FString();
        }
        Names.Add(Current.Name);
    }
    return FString::Join(Names, TEXT("."));
}

bool FOpenPocketBaseFieldSelection::IsSet() const
{
    FOpenPocketBaseFieldRef Current;
    return bValid &&
        ((bAllExpandedFields && Expand.IsSet()) ||
         (!bAllExpandedFields && Field.ResolveCurrent(Current)));
}

bool FOpenPocketBaseFieldSelection::BelongsTo(
    const FOpenPocketBaseCollectionRef& Collection) const
{
    if (!IsSet())
    {
        return false;
    }
    return Expand.IsSet() ? Expand.BelongsTo(Collection) : Field.BelongsTo(Collection);
}

FString FOpenPocketBaseFieldSelection::ToQueryValue() const
{
    if (!IsSet())
    {
        return {};
    }

    FOpenPocketBaseFieldRef Current;
    if (!bAllExpandedFields && !Field.ResolveCurrent(Current))
    {
        return {};
    }
    FString Terminal = bAllExpandedFields ? TEXT("*") : Current.Name;
    if (bExcerpt)
    {
        Terminal += FString::Printf(
            TEXT(":excerpt(%d,%s)"),
            ExcerptMaxLength,
            bExcerptWithEllipsis ? TEXT("true") : TEXT("false"));
    }
    return Expand.IsSet()
        ? FString::Printf(TEXT("expand.%s.%s"), *Expand.ToQueryValue(), *Terminal)
        : Terminal;
}

FOpenPocketBaseSort OpenPocketBase::Query::Sort(
    const FOpenPocketBaseFieldRef& Field,
    const EOpenPocketBaseSortDirection Direction)
{
    FOpenPocketBaseSort Result;
    Field.ResolveCurrent(static_cast<FOpenPocketBaseFieldRef&>(Result.Field));
    Result.Direction = Direction;
    return Result;
}

FOpenPocketBaseExpand OpenPocketBase::Query::Expand(
    const FOpenPocketBaseRelationFieldRef& Relation)
{
    FOpenPocketBaseExpand Result;
    FOpenPocketBaseRelationFieldRef Current;
    if (!Relation.ResolveCurrentAs(Current))
    {
        Result.bValid = false;
        Result.ErrorMessage = TEXT("Expand Relation is missing or stale. Choose a relation field again from the current collection schema.");
        return Result;
    }
    Result.Path.Add(MoveTemp(Current));
    return Result;
}

FOpenPocketBaseExpand OpenPocketBase::Query::ThenExpand(
    FOpenPocketBaseExpand Path,
    const FOpenPocketBaseRelationFieldRef& Relation)
{
    if (!Path.IsSet())
    {
        Path.bValid = false;
        if (Path.ErrorMessage.IsEmpty())
        {
            Path.ErrorMessage = TEXT("The existing Expand path is invalid. Start a new path with Expand Relation using a current relation field.");
        }
        return Path;
    }
    FOpenPocketBaseRelationFieldRef Current;
    if (!Relation.ResolveCurrentAs(Current))
    {
        Path.bValid = false;
        Path.ErrorMessage = TEXT("Then Expand Relation is missing or stale. Choose a relation field from the collection targeted by the previous relation.");
        return Path;
    }

    FOpenPocketBaseRelationFieldRef Previous;
    if (!Path.Path.Last().ResolveCurrentAs(Previous) ||
        Previous.SchemaId != Current.SchemaId ||
        Previous.RelatedCollectionId.IsEmpty() ||
        Previous.RelatedCollectionId != Current.CollectionId)
    {
        Path.bValid = false;
        Path.ErrorMessage = FString::Printf(
            TEXT("Relation '%s' does not belong to the collection targeted by '%s'. Choose the next relation from that target collection."),
            *Current.Name,
            *Previous.Name);
        return Path;
    }

    Path.Path.Last() = MoveTemp(Previous);
    Path.Path.Add(MoveTemp(Current));
    return Path;
}

namespace
{
FOpenPocketBaseFieldSelection InvalidSelection(FString Message)
{
    FOpenPocketBaseFieldSelection Result;
    Result.bValid = false;
    Result.ErrorMessage = MoveTemp(Message);
    return Result;
}

bool FieldMatchesExpandTarget(
    const FOpenPocketBaseExpand& Expand,
    const FOpenPocketBaseFieldRef& Field)
{
    FOpenPocketBaseRelationFieldRef CurrentRelation;
    FOpenPocketBaseFieldRef CurrentField;
    return Expand.IsSet() &&
        Expand.Path.Last().ResolveCurrentAs(CurrentRelation) &&
        Field.ResolveCurrent(CurrentField) &&
        CurrentRelation.SchemaId == CurrentField.SchemaId &&
        CurrentRelation.RelatedCollectionId == CurrentField.CollectionId;
}
}

FOpenPocketBaseFieldSelection OpenPocketBase::Query::Select(
    const FOpenPocketBaseFieldRef& Field)
{
    FOpenPocketBaseFieldRef Current;
    if (!Field.ResolveCurrent(Current))
    {
        return InvalidSelection(TEXT("Selected Field is missing or stale. Choose it again from the current collection schema."));
    }
    FOpenPocketBaseFieldSelection Result;
    static_cast<FOpenPocketBaseFieldRef&>(Result.Field) = MoveTemp(Current);
    return Result;
}

FOpenPocketBaseFieldSelection OpenPocketBase::Query::SelectExcerpt(
    const FOpenPocketBaseStringFieldRef& Field,
    const int32 MaxLength,
    const bool bWithEllipsis)
{
    FOpenPocketBaseStringFieldRef Current;
    if (!Field.ResolveCurrentAs(Current) || MaxLength < 0)
    {
        return InvalidSelection(FString::Printf(
            TEXT("Select Excerpt requires a current string field and Max Length of 0 or greater. Received Max Length %d."),
            MaxLength));
    }
    FOpenPocketBaseFieldSelection Result;
    Result.Field = MoveTemp(Current);
    Result.bExcerpt = true;
    Result.ExcerptMaxLength = MaxLength;
    Result.bExcerptWithEllipsis = bWithEllipsis;
    return Result;
}

FOpenPocketBaseFieldSelection OpenPocketBase::Query::SelectExpanded(
    FOpenPocketBaseExpand Path,
    const FOpenPocketBaseFieldRef& Field)
{
    FOpenPocketBaseFieldRef Current;
    if (!Field.ResolveCurrent(Current) || !FieldMatchesExpandTarget(Path, Current))
    {
        return InvalidSelection(TEXT("The selected field does not belong to the final collection in this Expand path. Choose the field from that expanded collection."));
    }
    FOpenPocketBaseFieldSelection Result;
    static_cast<FOpenPocketBaseFieldRef&>(Result.Field) = MoveTemp(Current);
    Result.Expand = MoveTemp(Path);
    return Result;
}

FOpenPocketBaseFieldSelection OpenPocketBase::Query::SelectExpandedExcerpt(
    FOpenPocketBaseExpand Path,
    const FOpenPocketBaseStringFieldRef& Field,
    const int32 MaxLength,
    const bool bWithEllipsis)
{
    FOpenPocketBaseStringFieldRef Current;
    if (!Field.ResolveCurrentAs(Current) || MaxLength < 0 ||
        !FieldMatchesExpandTarget(Path, Current))
    {
        return InvalidSelection(FString::Printf(
            TEXT("Select Expanded Excerpt requires a string field from the final expanded collection and Max Length of 0 or greater. Received Max Length %d."),
            MaxLength));
    }
    FOpenPocketBaseFieldSelection Result;
    Result.Field = MoveTemp(Current);
    Result.Expand = MoveTemp(Path);
    Result.bExcerpt = true;
    Result.ExcerptMaxLength = MaxLength;
    Result.bExcerptWithEllipsis = bWithEllipsis;
    return Result;
}

FOpenPocketBaseFieldSelection OpenPocketBase::Query::SelectExpandedRecord(
    FOpenPocketBaseExpand Path)
{
    if (!Path.IsSet())
    {
        return InvalidSelection(TEXT("Select Expanded Record requires a valid Expand path. Build the path with Expand Relation before selecting the expanded record."));
    }
    FOpenPocketBaseFieldSelection Result;
    Result.Expand = MoveTemp(Path);
    Result.bAllExpandedFields = true;
    return Result;
}
