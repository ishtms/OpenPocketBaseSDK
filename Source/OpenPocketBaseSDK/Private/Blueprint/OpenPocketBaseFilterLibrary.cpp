#include "OpenPocketBaseFilterLibrary.h"

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::StringFilter(
    const FOpenPocketBaseStringFieldRef Field,
    const EOpenPocketBaseStringComparison Comparison,
    const FString& Value)
{
    return FOpenPocketBaseFilter::String(Field, Comparison, Value);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::StringArrayFilter(
    const FOpenPocketBaseStringArrayFieldRef Field,
    const EOpenPocketBaseStringComparison Comparison,
    const FString& Value)
{
    return FOpenPocketBaseFilter::StringArray(Field, Comparison, Value);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::NumberFilter(
    const FOpenPocketBaseNumberFieldRef Field,
    const EOpenPocketBaseNumberComparison Comparison,
    const double Value)
{
    return FOpenPocketBaseFilter::Number(Field, Comparison, Value);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::BooleanFilter(
    const FOpenPocketBaseBooleanFieldRef Field,
    const EOpenPocketBaseBooleanComparison Comparison,
    const bool bValue)
{
    return FOpenPocketBaseFilter::Boolean(Field, Comparison, bValue);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::DateFilter(
    const FOpenPocketBaseDateFieldRef Field,
    const EOpenPocketBaseDateComparison Comparison,
    const FDateTime Value)
{
    return FOpenPocketBaseFilter::Date(Field, Comparison, Value);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::NullFilter(
    const FOpenPocketBaseAnyFieldRef Field,
    const EOpenPocketBaseNullComparison Comparison)
{
    return FOpenPocketBaseFilter::Null(Field, Comparison);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::RelatedStringFilter(
    const FOpenPocketBaseExpand Relations,
    const FOpenPocketBaseStringFieldRef Field,
    const EOpenPocketBaseStringComparison Comparison,
    const FString& Value)
{
    return FOpenPocketBaseFilter::RelatedString(Relations, Field, Comparison, Value);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::RelatedNumberFilter(
    const FOpenPocketBaseExpand Relations,
    const FOpenPocketBaseNumberFieldRef Field,
    const EOpenPocketBaseNumberComparison Comparison,
    const double Value)
{
    return FOpenPocketBaseFilter::RelatedNumber(Relations, Field, Comparison, Value);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::RelatedBooleanFilter(
    const FOpenPocketBaseExpand Relations,
    const FOpenPocketBaseBooleanFieldRef Field,
    const EOpenPocketBaseBooleanComparison Comparison,
    const bool bValue)
{
    return FOpenPocketBaseFilter::RelatedBoolean(Relations, Field, Comparison, bValue);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::RelatedDateFilter(
    const FOpenPocketBaseExpand Relations,
    const FOpenPocketBaseDateFieldRef Field,
    const EOpenPocketBaseDateComparison Comparison,
    const FDateTime Value)
{
    return FOpenPocketBaseFilter::RelatedDate(Relations, Field, Comparison, Value);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::RelatedNullFilter(
    const FOpenPocketBaseExpand Relations,
    const FOpenPocketBaseAnyFieldRef Field,
    const EOpenPocketBaseNullComparison Comparison)
{
    return FOpenPocketBaseFilter::RelatedNull(Relations, Field, Comparison);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::AndFilters(
    const FOpenPocketBaseFilter& A,
    const FOpenPocketBaseFilter& B)
{
    return A.And(B);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::OrFilters(
    const FOpenPocketBaseFilter& A,
    const FOpenPocketBaseFilter& B)
{
    return A.Or(B);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::DynamicFilter(const FString& Expression)
{
    return FOpenPocketBaseFilter::DynamicRaw(Expression);
}
