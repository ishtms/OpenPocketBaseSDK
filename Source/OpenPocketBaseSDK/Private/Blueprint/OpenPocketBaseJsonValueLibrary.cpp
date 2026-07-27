#include "OpenPocketBaseJsonValueLibrary.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FOpenPocketBaseJsonValue UOpenPocketBaseJsonValueLibrary::MakeJsonObject()
{
    return FOpenPocketBaseJsonValue::FromJsonValue(
        MakeShared<FJsonValueObject>(MakeShared<FJsonObject>()));
}

FOpenPocketBaseJsonValue UOpenPocketBaseJsonValueLibrary::MakeJsonArray()
{
    return FOpenPocketBaseJsonValue::FromJsonValue(
        MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>()));
}

FOpenPocketBaseJsonValue UOpenPocketBaseJsonValueLibrary::JsonString(const FString& Value)
{
    return FOpenPocketBaseJsonValue::FromJsonValue(MakeShared<FJsonValueString>(Value));
}

FOpenPocketBaseJsonValue UOpenPocketBaseJsonValueLibrary::JsonNumber(const double Value)
{
    return FMath::IsFinite(Value)
        ? FOpenPocketBaseJsonValue::FromJsonValue(MakeShared<FJsonValueNumber>(Value))
        : FOpenPocketBaseJsonValue::Invalid(TEXT("JSON Number must be finite. NaN and infinity are not valid JSON numbers."));
}

FOpenPocketBaseJsonValue UOpenPocketBaseJsonValueLibrary::JsonBoolean(const bool bValue)
{
    return FOpenPocketBaseJsonValue::FromJsonValue(MakeShared<FJsonValueBoolean>(bValue));
}

FOpenPocketBaseJsonValue UOpenPocketBaseJsonValueLibrary::JsonNull()
{
    return FOpenPocketBaseJsonValue::FromJsonValue(MakeShared<FJsonValueNull>());
}

FOpenPocketBaseJsonValue UOpenPocketBaseJsonValueLibrary::SetJsonProperty(
    FOpenPocketBaseJsonValue Object,
    const FString& Name,
    FOpenPocketBaseJsonValue Value)
{
    const TSharedPtr<FJsonValue> ObjectValue = Object.ToJsonValue();
    const TSharedPtr<FJsonValue> PropertyValue = Value.ToJsonValue();
    const TSharedPtr<FJsonObject>* SourceObject = nullptr;
    if (Name.IsEmpty())
    {
        return FOpenPocketBaseJsonValue::Invalid(TEXT("JSON Property Name is empty. Enter the object key to set."));
    }
    if (!ObjectValue.IsValid() || !ObjectValue->TryGetObject(SourceObject) ||
        SourceObject == nullptr || !SourceObject->IsValid())
    {
        return FOpenPocketBaseJsonValue::Invalid(TEXT("Set JSON Property requires an Object built by Make JSON Object or Parse JSON. The supplied value is empty or has another JSON type."));
    }
    if (!PropertyValue.IsValid())
    {
        return FOpenPocketBaseJsonValue::Invalid(
            Value.ErrorMessage.IsEmpty()
                ? TEXT("JSON Property Value is empty or invalid. Build a JSON value before setting the property.")
                : Value.ErrorMessage);
    }

    TSharedPtr<FJsonObject> ResultObject = *SourceObject;
    ResultObject->SetField(Name, PropertyValue);
    return FOpenPocketBaseJsonValue::FromJsonValue(MakeShared<FJsonValueObject>(ResultObject));
}

FOpenPocketBaseJsonValue UOpenPocketBaseJsonValueLibrary::AddJsonItem(
    FOpenPocketBaseJsonValue Array,
    FOpenPocketBaseJsonValue Value)
{
    const TSharedPtr<FJsonValue> ArrayValue = Array.ToJsonValue();
    const TSharedPtr<FJsonValue> ItemValue = Value.ToJsonValue();
    const TArray<TSharedPtr<FJsonValue>>* SourceItems = nullptr;
    if (!ArrayValue.IsValid() || !ArrayValue->TryGetArray(SourceItems) || SourceItems == nullptr)
    {
        return FOpenPocketBaseJsonValue::Invalid(TEXT("Add JSON Item requires an Array built by Make JSON Array or Parse JSON. The supplied value is empty or has another JSON type."));
    }
    if (!ItemValue.IsValid())
    {
        return FOpenPocketBaseJsonValue::Invalid(
            Value.ErrorMessage.IsEmpty()
                ? TEXT("JSON Array Item is empty or invalid. Build a JSON value before adding it to the array.")
                : Value.ErrorMessage);
    }

    TArray<TSharedPtr<FJsonValue>> ResultItems = *SourceItems;
    ResultItems.Add(ItemValue);
    return FOpenPocketBaseJsonValue::FromJsonValue(
        MakeShared<FJsonValueArray>(MoveTemp(ResultItems)));
}

bool UOpenPocketBaseJsonValueLibrary::ParseJson(
    const FString& Json,
    FOpenPocketBaseJsonValue& OutValue)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(
        TEXT("[") + Json + TEXT("]"));
    if (!FJsonSerializer::Deserialize(Reader, Values) || Values.Num() != 1 ||
        !Values[0].IsValid())
    {
        const FString ReaderError = Reader->GetErrorMessage();
        OutValue = FOpenPocketBaseJsonValue::Invalid(
            ReaderError.IsEmpty()
                ? TEXT("JSON Text could not be parsed. Check commas, quotes, brackets, braces, and value types.")
                : FString::Printf(
                      TEXT("JSON Text could not be parsed. %s"),
                      *ReaderError));
        return false;
    }
    OutValue = FOpenPocketBaseJsonValue::FromJsonValue(Values[0]);
    return OutValue.IsValid();
}

bool UOpenPocketBaseJsonValueLibrary::TryGetJsonProperty(
    FOpenPocketBaseJsonValue Object,
    const FString& Name,
    FOpenPocketBaseJsonValue& OutValue)
{
    OutValue = {};
    const TSharedPtr<FJsonValue> Value = Object.ToJsonValue();
    const TSharedPtr<FJsonObject>* JsonObject = nullptr;
    if (!Value.IsValid() || !Value->TryGetObject(JsonObject) || JsonObject == nullptr ||
        !JsonObject->IsValid())
    {
        return false;
    }
    const TSharedPtr<FJsonValue> Property = (*JsonObject)->TryGetField(Name);
    if (!Property.IsValid())
    {
        return false;
    }
    OutValue = FOpenPocketBaseJsonValue::FromJsonValue(Property);
    return OutValue.IsValid();
}

bool UOpenPocketBaseJsonValueLibrary::TryGetJsonArrayItem(
    FOpenPocketBaseJsonValue Array,
    const int32 Index,
    FOpenPocketBaseJsonValue& OutValue)
{
    OutValue = {};
    const TSharedPtr<FJsonValue> Value = Array.ToJsonValue();
    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (!Value.IsValid() || !Value->TryGetArray(Items) || Items == nullptr ||
        !Items->IsValidIndex(Index) || !(*Items)[Index].IsValid())
    {
        return false;
    }
    OutValue = FOpenPocketBaseJsonValue::FromJsonValue((*Items)[Index]);
    return OutValue.IsValid();
}

bool UOpenPocketBaseJsonValueLibrary::TryGetJsonString(
    FOpenPocketBaseJsonValue Value,
    FString& OutString)
{
    OutString.Reset();
    const TSharedPtr<FJsonValue> JsonValue = Value.ToJsonValue();
    return JsonValue.IsValid() && JsonValue->Type == EJson::String &&
        JsonValue->TryGetString(OutString);
}

bool UOpenPocketBaseJsonValueLibrary::TryGetJsonNumber(
    FOpenPocketBaseJsonValue Value,
    double& OutNumber)
{
    OutNumber = 0.0;
    const TSharedPtr<FJsonValue> JsonValue = Value.ToJsonValue();
    return JsonValue.IsValid() && JsonValue->Type == EJson::Number &&
        JsonValue->TryGetNumber(OutNumber);
}

bool UOpenPocketBaseJsonValueLibrary::TryGetJsonBoolean(
    FOpenPocketBaseJsonValue Value,
    bool& bOutValue)
{
    bOutValue = false;
    const TSharedPtr<FJsonValue> JsonValue = Value.ToJsonValue();
    return JsonValue.IsValid() && JsonValue->Type == EJson::Boolean &&
        JsonValue->TryGetBool(bOutValue);
}

bool UOpenPocketBaseJsonValueLibrary::TryGetJsonArrayLength(
    FOpenPocketBaseJsonValue Array,
    int32& OutLength)
{
    OutLength = 0;
    const TSharedPtr<FJsonValue> Value = Array.ToJsonValue();
    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (!Value.IsValid() || !Value->TryGetArray(Items) || Items == nullptr)
    {
        return false;
    }
    OutLength = Items->Num();
    return true;
}
