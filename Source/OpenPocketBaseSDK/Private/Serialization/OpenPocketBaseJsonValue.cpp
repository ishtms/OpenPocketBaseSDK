#include "OpenPocketBaseJsonValue.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
EOpenPocketBaseJsonValueType ConvertJsonType(const EJson Type)
{
    switch (Type)
    {
    case EJson::Null:
        return EOpenPocketBaseJsonValueType::Null;
    case EJson::Object:
        return EOpenPocketBaseJsonValueType::Object;
    case EJson::Array:
        return EOpenPocketBaseJsonValueType::Array;
    case EJson::String:
        return EOpenPocketBaseJsonValueType::String;
    case EJson::Number:
        return EOpenPocketBaseJsonValueType::Number;
    case EJson::Boolean:
        return EOpenPocketBaseJsonValueType::Boolean;
    default:
        return EOpenPocketBaseJsonValueType::Invalid;
    }
}
}

bool FOpenPocketBaseJsonValue::IsValid() const
{
    return Type != EOpenPocketBaseJsonValueType::Invalid && ToJsonValue().IsValid();
}

TSharedPtr<FJsonValue> FOpenPocketBaseJsonValue::ToJsonValue() const
{
    if (Type == EOpenPocketBaseJsonValueType::Invalid || Json.IsEmpty())
    {
        return nullptr;
    }

    // UE 5.8 drops scalar JSON roots when deserializing directly into FJsonValue.
    TArray<TSharedPtr<FJsonValue>> Values;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(
        TEXT("[") + Json + TEXT("]"));
    if (!FJsonSerializer::Deserialize(Reader, Values) || Values.Num() != 1 ||
        !Values[0].IsValid() || ConvertJsonType(Values[0]->Type) != Type)
    {
        return nullptr;
    }
    return Values[0];
}

FOpenPocketBaseJsonValue FOpenPocketBaseJsonValue::FromJsonValue(
    const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        return Invalid(TEXT("A valid JSON value is required."));
    }

    FOpenPocketBaseJsonValue Result;
    Result.Type = ConvertJsonType(Value->Type);
    // Serialize through an array for the same scalar-root compatibility reason.
    FString WrappedJson;
    const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&WrappedJson);
    const TArray<TSharedPtr<FJsonValue>> WrappedValues = {Value};
    if (Result.Type == EOpenPocketBaseJsonValueType::Invalid ||
        !FJsonSerializer::Serialize(WrappedValues, Writer) ||
        WrappedJson.Len() < 2 || WrappedJson[0] != TEXT('[') ||
        WrappedJson[WrappedJson.Len() - 1] != TEXT(']'))
    {
        return Invalid(TEXT("The JSON value could not be serialized."));
    }
    Result.Json = WrappedJson.Mid(1, WrappedJson.Len() - 2);
    return Result;
}

FOpenPocketBaseJsonValue FOpenPocketBaseJsonValue::Invalid(FString Message)
{
    FOpenPocketBaseJsonValue Result;
    Result.ErrorMessage = MoveTemp(Message);
    return Result;
}
