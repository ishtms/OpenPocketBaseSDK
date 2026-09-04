// Copyright 2026 Ishtmeet Singh.

#include "OpenPocketBaseQueryLibrary.h"

FOpenPocketBaseSort UOpenPocketBaseQueryLibrary::SortAscending(
    FOpenPocketBaseAnyFieldRef Field)
{
    return OpenPocketBase::Query::Sort(
        Field,
        EOpenPocketBaseSortDirection::Ascending);
}

FOpenPocketBaseSort UOpenPocketBaseQueryLibrary::SortDescending(
    FOpenPocketBaseAnyFieldRef Field)
{
    return OpenPocketBase::Query::Sort(
        Field,
        EOpenPocketBaseSortDirection::Descending);
}

FOpenPocketBaseExpand UOpenPocketBaseQueryLibrary::ExpandRelation(
    FOpenPocketBaseRelationFieldRef Relation)
{
    return OpenPocketBase::Query::Expand(Relation);
}

FOpenPocketBaseExpand UOpenPocketBaseQueryLibrary::ThenExpandRelation(
    FOpenPocketBaseExpand Expand,
    FOpenPocketBaseRelationFieldRef Relation)
{
    return OpenPocketBase::Query::ThenExpand(MoveTemp(Expand), Relation);
}

FOpenPocketBaseFieldSelection UOpenPocketBaseQueryLibrary::SelectField(
    const FOpenPocketBaseAnyFieldRef Field)
{
    return OpenPocketBase::Query::Select(Field);
}

FOpenPocketBaseFieldSelection UOpenPocketBaseQueryLibrary::SelectTextExcerpt(
    const FOpenPocketBaseStringFieldRef Field,
    const int32 MaxLength,
    const bool bWithEllipsis)
{
    return OpenPocketBase::Query::SelectExcerpt(Field, MaxLength, bWithEllipsis);
}

FOpenPocketBaseFieldSelection UOpenPocketBaseQueryLibrary::SelectExpandedField(
    FOpenPocketBaseExpand Expand,
    const FOpenPocketBaseAnyFieldRef Field)
{
    return OpenPocketBase::Query::SelectExpanded(MoveTemp(Expand), Field);
}

FOpenPocketBaseFieldSelection UOpenPocketBaseQueryLibrary::SelectExpandedTextExcerpt(
    FOpenPocketBaseExpand Expand,
    const FOpenPocketBaseStringFieldRef Field,
    const int32 MaxLength,
    const bool bWithEllipsis)
{
    return OpenPocketBase::Query::SelectExpandedExcerpt(
        MoveTemp(Expand),
        Field,
        MaxLength,
        bWithEllipsis);
}

FOpenPocketBaseFieldSelection UOpenPocketBaseQueryLibrary::SelectExpandedRecord(
    FOpenPocketBaseExpand Expand)
{
    return OpenPocketBase::Query::SelectExpandedRecord(MoveTemp(Expand));
}
