#include "OpenPocketBaseFilter.h"

#include "OpenPocketBaseDate.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

namespace
{
using FCondensedJsonWriter = TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

bool IsValidParameterName(const FString& Name)
{
    if (Name.IsEmpty() || (!FChar::IsAlpha(Name[0]) && Name[0] != TEXT('_')))
    {
        return false;
    }
    for (const TCHAR Character : Name)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
        {
            return false;
        }
    }
    return true;
}

FString EncodeString(const FString& Value)
{
    FString Json;
    const TSharedRef<FCondensedJsonWriter> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    Writer->WriteValue(Value);
    Writer->Close();
    return Json;
}

FString EncodeNumber(const double Value)
{
    FString Json;
    const TSharedRef<FCondensedJsonWriter> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    Writer->WriteArrayStart();
    Writer->WriteValue(Value);
    Writer->WriteArrayEnd();
    Writer->Close();
    return Json.Mid(1, Json.Len() - 2);
}

template <typename WriteValuesType>
FString EncodeArray(WriteValuesType&& WriteValues)
{
    FString Json;
    const TSharedRef<FCondensedJsonWriter> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    Writer->WriteArrayStart();
    WriteValues(Writer);
    Writer->WriteArrayEnd();
    Writer->Close();
    return EncodeString(Json);
}

FOpenPocketBaseError MakeBindingError(const TCHAR* Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
    Error.ServerMessage = Message;
    return Error;
}
}

bool FOpenPocketBaseFilterParams::AddEncoded(const FString& Name, FString EncodedValue)
{
    if (!IsValidParameterName(Name))
    {
        return false;
    }
    EncodedValues.Add(Name, MoveTemp(EncodedValue));
    return true;
}

bool FOpenPocketBaseFilterParams::AddString(const FString& Name, const FString& Value)
{
    return AddEncoded(Name, EncodeString(Value));
}

bool FOpenPocketBaseFilterParams::AddNumber(const FString& Name, const double Value)
{
    return FMath::IsFinite(Value) && AddEncoded(Name, EncodeNumber(Value));
}

bool FOpenPocketBaseFilterParams::AddBoolean(const FString& Name, const bool bValue)
{
    return AddEncoded(Name, bValue ? TEXT("true") : TEXT("false"));
}

bool FOpenPocketBaseFilterParams::AddDate(const FString& Name, const FDateTime& Value)
{
    return AddEncoded(Name, EncodeString(OpenPocketBase::Date::Format(Value)));
}

bool FOpenPocketBaseFilterParams::AddNull(const FString& Name)
{
    return AddEncoded(Name, TEXT("null"));
}

bool FOpenPocketBaseFilterParams::AddStringArray(
    const FString& Name,
    const TArray<FString>& Value)
{
    return AddEncoded(
        Name,
        EncodeArray(
            [&Value](const TSharedRef<FCondensedJsonWriter>& Writer)
            {
                for (const FString& Item : Value)
                {
                    Writer->WriteValue(Item);
                }
            }));
}

bool FOpenPocketBaseFilterParams::AddNumberArray(
    const FString& Name,
    const TArray<double>& Value)
{
    for (const double Item : Value)
    {
        if (!FMath::IsFinite(Item))
        {
            return false;
        }
    }
    return AddEncoded(
        Name,
        EncodeArray(
            [&Value](const TSharedRef<FCondensedJsonWriter>& Writer)
            {
                for (const double Item : Value)
                {
                    Writer->WriteValue(Item);
                }
            }));
}

bool FOpenPocketBaseFilterParams::AddBooleanArray(
    const FString& Name,
    const TArray<bool>& Value)
{
    return AddEncoded(
        Name,
        EncodeArray(
            [&Value](const TSharedRef<FCondensedJsonWriter>& Writer)
            {
                for (const bool bItem : Value)
                {
                    Writer->WriteValue(bItem);
                }
            }));
}

int32 FOpenPocketBaseFilterParams::Num() const
{
    return EncodedValues.Num();
}

void FOpenPocketBaseFilterParams::Reset()
{
    EncodedValues.Reset();
}

bool FOpenPocketBaseFilter::TryBind(
    const FString& Expression,
    const FOpenPocketBaseFilterParams& Params,
    FString& OutFilter,
    FOpenPocketBaseError& OutError)
{
    OutFilter.Reset();
    OutError = {};
    if (Expression.Len() > 64 * 1024)
    {
        OutError = MakeBindingError(TEXT("The filter expression exceeds the supported length."));
        return false;
    }

    TSet<FString> UsedParameters;
    int32 Position = 0;
    while (Position < Expression.Len())
    {
        const int32 PlaceholderStart = Expression.Find(
            TEXT("{:"),
            ESearchCase::CaseSensitive,
            ESearchDir::FromStart,
            Position);
        if (PlaceholderStart == INDEX_NONE)
        {
            OutFilter += Expression.Mid(Position);
            break;
        }

        OutFilter += Expression.Mid(Position, PlaceholderStart - Position);
        const int32 PlaceholderEnd = Expression.Find(
            TEXT("}"),
            ESearchCase::CaseSensitive,
            ESearchDir::FromStart,
            PlaceholderStart + 2);
        if (PlaceholderEnd == INDEX_NONE)
        {
            OutFilter.Reset();
            OutError = MakeBindingError(TEXT("The filter contains an unclosed parameter placeholder."));
            return false;
        }

        const FString Name = Expression.Mid(
            PlaceholderStart + 2,
            PlaceholderEnd - PlaceholderStart - 2);
        const FString* EncodedValue = Params.EncodedValues.Find(Name);
        if (!IsValidParameterName(Name) || EncodedValue == nullptr)
        {
            OutFilter.Reset();
            OutError = MakeBindingError(TEXT("The filter contains an unknown parameter placeholder."));
            return false;
        }

        OutFilter += *EncodedValue;
        UsedParameters.Add(Name);
        Position = PlaceholderEnd + 1;
    }

    if (UsedParameters.Num() != Params.EncodedValues.Num())
    {
        OutFilter.Reset();
        OutError = MakeBindingError(TEXT("The filter contains unused parameters."));
        return false;
    }
    if (OutFilter.Len() > 64 * 1024)
    {
        OutFilter.Reset();
        OutError = MakeBindingError(TEXT("The bound filter exceeds the supported length."));
        return false;
    }
    return true;
}
