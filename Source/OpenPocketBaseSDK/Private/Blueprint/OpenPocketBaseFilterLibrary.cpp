#include "OpenPocketBaseFilterLibrary.h"

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::StringFilter(
    const FString& Field,
    const EOpenPocketBaseStringComparison Comparison,
    const FString& Value)
{
    return FOpenPocketBaseFilter::String(Field, Comparison, Value);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::NumberFilter(
    const FString& Field,
    const EOpenPocketBaseNumberComparison Comparison,
    const double Value)
{
    return FOpenPocketBaseFilter::Number(Field, Comparison, Value);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::BooleanFilter(
    const FString& Field,
    const EOpenPocketBaseBooleanComparison Comparison,
    const bool bValue)
{
    return FOpenPocketBaseFilter::Boolean(Field, Comparison, bValue);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::DateFilter(
    const FString& Field,
    const EOpenPocketBaseDateComparison Comparison,
    const FDateTime Value)
{
    return FOpenPocketBaseFilter::Date(Field, Comparison, Value);
}

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::NullFilter(
    const FString& Field,
    const EOpenPocketBaseNullComparison Comparison)
{
    return FOpenPocketBaseFilter::Null(Field, Comparison);
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

FOpenPocketBaseFilter UOpenPocketBaseFilterLibrary::RawFilter(const FString& Expression)
{
    return FOpenPocketBaseFilter::Raw(Expression);
}
