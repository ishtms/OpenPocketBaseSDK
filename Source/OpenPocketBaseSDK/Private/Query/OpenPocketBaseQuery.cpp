#include "OpenPocketBaseQuery.h"

bool FOpenPocketBaseSort::IsSet() const
{
    return Field.IsSet();
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
    return Direction == EOpenPocketBaseSortDirection::Descending
        ? TEXT("-") + Field.Name
        : Field.Name;
}

bool FOpenPocketBaseExpand::IsSet() const
{
    return bValid && !Path.IsEmpty();
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
        Names.Add(Relation.Name);
    }
    return FString::Join(Names, TEXT("."));
}

bool FOpenPocketBaseFieldSelection::IsSet() const
{
    return bValid &&
        ((bAllExpandedFields && Expand.IsSet()) || (!bAllExpandedFields && Field.IsSet()));
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

    FString Terminal = bAllExpandedFields ? TEXT("*") : Field.Name;
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
    static_cast<FOpenPocketBaseFieldRef&>(Result.Field) = Field;
    Result.Direction = Direction;
    return Result;
}

FOpenPocketBaseExpand OpenPocketBase::Query::Expand(
    const FOpenPocketBaseRelationFieldRef& Relation)
{
    FOpenPocketBaseExpand Result;
    if (!Relation.IsSet())
    {
        Result.bValid = false;
        Result.ErrorMessage = TEXT("Choose a relation field to expand.");
        return Result;
    }
    Result.Path.Add(Relation);
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
            Path.ErrorMessage = TEXT("Start with a valid relation field.");
        }
        return Path;
    }
    if (!Relation.IsSet())
    {
        Path.bValid = false;
        Path.ErrorMessage = TEXT("Choose a relation field to append.");
        return Path;
    }

    const FOpenPocketBaseRelationFieldRef& Previous = Path.Path.Last();
    if (Previous.SchemaId != Relation.SchemaId ||
        Previous.RelatedCollectionId.IsEmpty() ||
        Previous.RelatedCollectionId != Relation.CollectionId)
    {
        Path.bValid = false;
        Path.ErrorMessage = FString::Printf(
            TEXT("Relation '%s' does not belong to the collection targeted by '%s'."),
            *Relation.Name,
            *Previous.Name);
        return Path;
    }

    Path.Path.Add(Relation);
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
    return Expand.IsSet() && Field.IsSet() &&
        Expand.Path.Last().SchemaId == Field.SchemaId &&
        Expand.Path.Last().RelatedCollectionId == Field.CollectionId;
}
}

FOpenPocketBaseFieldSelection OpenPocketBase::Query::Select(
    const FOpenPocketBaseFieldRef& Field)
{
    if (!Field.IsSet())
    {
        return InvalidSelection(TEXT("Choose a field to select."));
    }
    FOpenPocketBaseFieldSelection Result;
    static_cast<FOpenPocketBaseFieldRef&>(Result.Field) = Field;
    return Result;
}

FOpenPocketBaseFieldSelection OpenPocketBase::Query::SelectExcerpt(
    const FOpenPocketBaseStringFieldRef& Field,
    const int32 MaxLength,
    const bool bWithEllipsis)
{
    if (!FOpenPocketBaseStringFieldRef::Accepts(Field) || MaxLength < 0)
    {
        return InvalidSelection(TEXT("Choose a string field and a non-negative excerpt length."));
    }
    FOpenPocketBaseFieldSelection Result;
    Result.Field = Field;
    Result.bExcerpt = true;
    Result.ExcerptMaxLength = MaxLength;
    Result.bExcerptWithEllipsis = bWithEllipsis;
    return Result;
}

FOpenPocketBaseFieldSelection OpenPocketBase::Query::SelectExpanded(
    FOpenPocketBaseExpand Path,
    const FOpenPocketBaseFieldRef& Field)
{
    if (!Field.IsSet() || !FieldMatchesExpandTarget(Path, Field))
    {
        return InvalidSelection(TEXT("The selected field must belong to the expanded collection."));
    }
    FOpenPocketBaseFieldSelection Result;
    static_cast<FOpenPocketBaseFieldRef&>(Result.Field) = Field;
    Result.Expand = MoveTemp(Path);
    return Result;
}

FOpenPocketBaseFieldSelection OpenPocketBase::Query::SelectExpandedExcerpt(
    FOpenPocketBaseExpand Path,
    const FOpenPocketBaseStringFieldRef& Field,
    const int32 MaxLength,
    const bool bWithEllipsis)
{
    if (!FOpenPocketBaseStringFieldRef::Accepts(Field) || MaxLength < 0 ||
        !FieldMatchesExpandTarget(Path, Field))
    {
        return InvalidSelection(TEXT("Choose a string field from the expanded collection and a non-negative excerpt length."));
    }
    FOpenPocketBaseFieldSelection Result;
    Result.Field = Field;
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
        return InvalidSelection(TEXT("Choose a valid relation path to select an expanded record."));
    }
    FOpenPocketBaseFieldSelection Result;
    Result.Expand = MoveTemp(Path);
    Result.bAllExpandedFields = true;
    return Result;
}
