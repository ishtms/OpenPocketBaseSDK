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

FOpenPocketBaseSort OpenPocketBase::Query::Sort(
    const FOpenPocketBaseAnyFieldRef& Field,
    const EOpenPocketBaseSortDirection Direction)
{
    FOpenPocketBaseSort Result;
    Result.Field = Field;
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
