#include "OpenPocketBaseRecordLibrary.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "OpenPocketBaseDate.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
TSharedPtr<FJsonValue> FindValue(const FOpenPocketBaseRecord& Record, const FString& FieldName)
{
    if (!Record.Data.JsonObject.IsValid())
    {
        return nullptr;
    }

    return Record.Data.JsonObject->TryGetField(FieldName);
}

EOpenPocketBaseFieldState GetBaseState(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        return EOpenPocketBaseFieldState::Missing;
    }
    if (Value->IsNull())
    {
        return EOpenPocketBaseFieldState::Null;
    }
    return EOpenPocketBaseFieldState::Found;
}
}

bool UOpenPocketBaseRecordLibrary::HasField(
    const FOpenPocketBaseRecord& Record,
    const FString& FieldName)
{
    return Record.Data.JsonObject.IsValid() && Record.Data.JsonObject->HasField(FieldName);
}

bool UOpenPocketBaseRecordLibrary::IsFieldNull(
    const FOpenPocketBaseRecord& Record,
    const FString& FieldName)
{
    const TSharedPtr<FJsonValue> Value = FindValue(Record, FieldName);
    return Value.IsValid() && Value->IsNull();
}

bool UOpenPocketBaseRecordLibrary::TryGetStringField(
    const FOpenPocketBaseRecord& Record,
    const FString& FieldName,
    FString& OutValue)
{
    return GetStringFieldState(Record, FieldName, OutValue) == EOpenPocketBaseFieldState::Found;
}

EOpenPocketBaseFieldState UOpenPocketBaseRecordLibrary::GetStringFieldState(
    const FOpenPocketBaseRecord& Record,
    const FString& FieldName,
    FString& OutValue)
{
    OutValue.Reset();
    const TSharedPtr<FJsonValue> Value = FindValue(Record, FieldName);
    const EOpenPocketBaseFieldState BaseState = GetBaseState(Value);
    if (BaseState != EOpenPocketBaseFieldState::Found)
    {
        return BaseState;
    }
    return Value->Type == EJson::String && Value->TryGetString(OutValue)
        ? EOpenPocketBaseFieldState::Found
        : EOpenPocketBaseFieldState::WrongType;
}

bool UOpenPocketBaseRecordLibrary::TryGetIntegerField(
    const FOpenPocketBaseRecord& Record,
    const FString& FieldName,
    int64& OutValue)
{
    return GetIntegerFieldState(Record, FieldName, OutValue) == EOpenPocketBaseFieldState::Found;
}

EOpenPocketBaseFieldState UOpenPocketBaseRecordLibrary::GetIntegerFieldState(
    const FOpenPocketBaseRecord& Record,
    const FString& FieldName,
    int64& OutValue)
{
    OutValue = 0;
    const TSharedPtr<FJsonValue> Value = FindValue(Record, FieldName);
    const EOpenPocketBaseFieldState BaseState = GetBaseState(Value);
    if (BaseState != EOpenPocketBaseFieldState::Found)
    {
        return BaseState;
    }

    double Number = 0;
    if (Value->Type != EJson::Number ||
        !Value->TryGetNumber(Number) ||
        !FMath::IsNearlyEqual(Number, FMath::RoundToDouble(Number)))
    {
        return EOpenPocketBaseFieldState::WrongType;
    }
    OutValue = static_cast<int64>(Number);
    return EOpenPocketBaseFieldState::Found;
}

bool UOpenPocketBaseRecordLibrary::TryGetNumberField(
    const FOpenPocketBaseRecord& Record,
    const FString& FieldName,
    double& OutValue)
{
    OutValue = 0;
    const TSharedPtr<FJsonValue> Value = FindValue(Record, FieldName);
    return Value.IsValid() && Value->Type == EJson::Number && Value->TryGetNumber(OutValue);
}

bool UOpenPocketBaseRecordLibrary::TryGetBooleanField(
    const FOpenPocketBaseRecord& Record,
    const FString& FieldName,
    bool& OutValue)
{
    OutValue = false;
    const TSharedPtr<FJsonValue> Value = FindValue(Record, FieldName);
    return Value.IsValid() && Value->Type == EJson::Boolean && Value->TryGetBool(OutValue);
}

bool UOpenPocketBaseRecordLibrary::TryGetDateField(
    const FOpenPocketBaseRecord& Record,
    const FString& FieldName,
    FDateTime& OutValue)
{
    FString DateString;
    if (!TryGetStringField(Record, FieldName, DateString))
    {
        return false;
    }
    return OpenPocketBase::Date::TryParse(DateString, OutValue);
}

bool UOpenPocketBaseRecordLibrary::TryGetStringArrayField(
    const FOpenPocketBaseRecord& Record,
    const FString& FieldName,
    TArray<FString>& OutValue)
{
    OutValue.Reset();
    const TSharedPtr<FJsonValue> Value = FindValue(Record, FieldName);
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Value.IsValid() ||
        Value->Type != EJson::Array ||
        !Value->TryGetArray(Values) ||
        Values == nullptr)
    {
        return false;
    }

    OutValue.Reserve(Values->Num());
    for (const TSharedPtr<FJsonValue>& Item : *Values)
    {
        FString StringValue;
        if (!Item.IsValid() || !Item->TryGetString(StringValue))
        {
            OutValue.Reset();
            return false;
        }
        OutValue.Add(MoveTemp(StringValue));
    }
    return true;
}

bool UOpenPocketBaseRecordLibrary::TryGetObjectField(
    const FOpenPocketBaseRecord& Record,
    const FString& FieldName,
    FJsonObjectWrapper& OutValue)
{
    OutValue = FJsonObjectWrapper();
    const TSharedPtr<FJsonValue> Value = FindValue(Record, FieldName);
    const TSharedPtr<FJsonObject>* Object = nullptr;
    if (!Value.IsValid() ||
        Value->Type != EJson::Object ||
        !Value->TryGetObject(Object) ||
        Object == nullptr)
    {
        return false;
    }

    OutValue.JsonObject = *Object;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutValue.JsonString);
    FJsonSerializer::Serialize(Object->ToSharedRef(), Writer);
    return true;
}

bool UOpenPocketBaseRecordLibrary::TryParsePocketBaseDate(
    const FString& Value,
    FDateTime& OutDateTime)
{
    return OpenPocketBase::Date::TryParse(Value, OutDateTime);
}

FString UOpenPocketBaseRecordLibrary::FormatPocketBaseDate(const FDateTime& Value)
{
    return OpenPocketBase::Date::Format(Value);
}
