#include "OpenPocketBaseFilterLibrary.h"

bool UOpenPocketBaseFilterLibrary::AddStringParameter(
    FOpenPocketBaseFilterParams& Params,
    const FString& Name,
    const FString& Value)
{
    return Params.AddString(Name, Value);
}

bool UOpenPocketBaseFilterLibrary::AddNumberParameter(
    FOpenPocketBaseFilterParams& Params,
    const FString& Name,
    const double Value)
{
    return Params.AddNumber(Name, Value);
}

bool UOpenPocketBaseFilterLibrary::AddBooleanParameter(
    FOpenPocketBaseFilterParams& Params,
    const FString& Name,
    const bool bValue)
{
    return Params.AddBoolean(Name, bValue);
}

bool UOpenPocketBaseFilterLibrary::AddDateParameter(
    FOpenPocketBaseFilterParams& Params,
    const FString& Name,
    const FDateTime Value)
{
    return Params.AddDate(Name, Value);
}

bool UOpenPocketBaseFilterLibrary::AddNullParameter(
    FOpenPocketBaseFilterParams& Params,
    const FString& Name)
{
    return Params.AddNull(Name);
}

bool UOpenPocketBaseFilterLibrary::AddStringArrayParameter(
    FOpenPocketBaseFilterParams& Params,
    const FString& Name,
    const TArray<FString>& Value)
{
    return Params.AddStringArray(Name, Value);
}

bool UOpenPocketBaseFilterLibrary::AddNumberArrayParameter(
    FOpenPocketBaseFilterParams& Params,
    const FString& Name,
    const TArray<double>& Value)
{
    return Params.AddNumberArray(Name, Value);
}

bool UOpenPocketBaseFilterLibrary::AddBooleanArrayParameter(
    FOpenPocketBaseFilterParams& Params,
    const FString& Name,
    const TArray<bool>& Value)
{
    return Params.AddBooleanArray(Name, Value);
}

void UOpenPocketBaseFilterLibrary::ClearParameters(FOpenPocketBaseFilterParams& Params)
{
    Params.Reset();
}

bool UOpenPocketBaseFilterLibrary::BindFilter(
    const FString& Expression,
    const FOpenPocketBaseFilterParams& Params,
    FString& OutFilter,
    FOpenPocketBaseError& OutError)
{
    return FOpenPocketBaseFilter::TryBind(Expression, Params, OutFilter, OutError);
}
