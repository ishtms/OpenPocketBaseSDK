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
