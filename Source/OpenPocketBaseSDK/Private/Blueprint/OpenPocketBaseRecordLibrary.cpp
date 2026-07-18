#include "OpenPocketBaseRecordLibrary.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "OpenPocketBaseDate.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
TSharedPtr<FJsonValue> FindValue(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseFieldRef& Field)
{
    if (!Field.IsSet() || Record.CollectionId != Field.CollectionId || !Record.Data.JsonObject.IsValid())
    {
        return nullptr;
    }

    return Record.Data.JsonObject->TryGetField(Field.Name);
}

EOpenPocketBaseFieldState GetBaseState(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseFieldRef& Field,
    const TSharedPtr<FJsonValue>& Value)
{
    if (!Field.IsSet() || Record.CollectionId != Field.CollectionId)
    {
        return EOpenPocketBaseFieldState::WrongCollection;
    }
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

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::NewRecordBody()
{
    return {};
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithStringField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseStringFieldRef Field,
    const FString& Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    Body.SetStringField(Field, Value, Modifier);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithNumberField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseNumberFieldRef Field,
    const double Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    Body.SetNumberField(Field, Value, Modifier);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithBooleanField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseBooleanFieldRef Field,
    const bool bValue,
    const EOpenPocketBaseFieldModifier Modifier)
{
    Body.SetBooleanField(Field, bValue, Modifier);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithNullField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseAnyFieldRef Field,
    const EOpenPocketBaseFieldModifier Modifier)
{
    Body.SetNullField(Field, Modifier);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithStringArrayField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseStringArrayFieldRef Field,
    const TArray<FString>& Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    Body.SetStringArrayField(Field, Value, Modifier);
    return Body;
}

FOpenPocketBaseRecordOptions UOpenPocketBaseRecordLibrary::NewRecordOptions()
{
    return {};
}

FOpenPocketBaseRecordOptions UOpenPocketBaseRecordLibrary::RecordOptionsSelectField(
    FOpenPocketBaseRecordOptions Options,
    FOpenPocketBaseFieldSelection Field)
{
    Options.Selecting(MoveTemp(Field));
    return Options;
}

FOpenPocketBaseRecordOptions UOpenPocketBaseRecordLibrary::RecordOptionsIncludeExpansion(
    FOpenPocketBaseRecordOptions Options,
    FOpenPocketBaseExpand Expand)
{
    Options.Including(MoveTemp(Expand));
    return Options;
}

FOpenPocketBaseListOptions UOpenPocketBaseRecordLibrary::NewListOptions(
    const int32 Page,
    const int32 PerPage)
{
    FOpenPocketBaseListOptions Options;
    Options.AtPage(Page).PageSize(PerPage);
    return Options;
}

FOpenPocketBaseListOptions UOpenPocketBaseRecordLibrary::ListOptionsWhere(
    FOpenPocketBaseListOptions Options,
    FOpenPocketBaseFilter Filter)
{
    Options.Where(MoveTemp(Filter));
    return Options;
}

FOpenPocketBaseListOptions UOpenPocketBaseRecordLibrary::ListOptionsThenSortBy(
    FOpenPocketBaseListOptions Options,
    FOpenPocketBaseSort Sort)
{
    Options.OrderedBy(MoveTemp(Sort));
    return Options;
}

FOpenPocketBaseListOptions UOpenPocketBaseRecordLibrary::ListOptionsSelectField(
    FOpenPocketBaseListOptions Options,
    FOpenPocketBaseFieldSelection Field)
{
    Options.Selecting(MoveTemp(Field));
    return Options;
}

FOpenPocketBaseListOptions UOpenPocketBaseRecordLibrary::ListOptionsIncludeExpansion(
    FOpenPocketBaseListOptions Options,
    FOpenPocketBaseExpand Expand)
{
    Options.Including(MoveTemp(Expand));
    return Options;
}

bool UOpenPocketBaseRecordLibrary::HasField(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseAnyFieldRef Field)
{
    return Field.IsSet() && Record.CollectionId == Field.CollectionId &&
        Record.Data.JsonObject.IsValid() && Record.Data.JsonObject->HasField(Field.Name);
}

bool UOpenPocketBaseRecordLibrary::IsFieldNull(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseAnyFieldRef Field)
{
    const TSharedPtr<FJsonValue> Value = FindValue(Record, Field);
    return Value.IsValid() && Value->IsNull();
}

bool UOpenPocketBaseRecordLibrary::TryGetStringField(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseStringFieldRef Field,
    FString& OutValue)
{
    return GetStringFieldState(Record, Field, OutValue) == EOpenPocketBaseFieldState::Found;
}

EOpenPocketBaseFieldState UOpenPocketBaseRecordLibrary::GetStringFieldState(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseStringFieldRef Field,
    FString& OutValue)
{
    OutValue.Reset();
    const TSharedPtr<FJsonValue> Value = FindValue(Record, Field);
    const EOpenPocketBaseFieldState BaseState = GetBaseState(Record, Field, Value);
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
    const FOpenPocketBaseNumberFieldRef Field,
    int64& OutValue)
{
    return GetIntegerFieldState(Record, Field, OutValue) == EOpenPocketBaseFieldState::Found;
}

EOpenPocketBaseFieldState UOpenPocketBaseRecordLibrary::GetIntegerFieldState(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseNumberFieldRef Field,
    int64& OutValue)
{
    OutValue = 0;
    const TSharedPtr<FJsonValue> Value = FindValue(Record, Field);
    const EOpenPocketBaseFieldState BaseState = GetBaseState(Record, Field, Value);
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
    const FOpenPocketBaseNumberFieldRef Field,
    double& OutValue)
{
    OutValue = 0;
    const TSharedPtr<FJsonValue> Value = FindValue(Record, Field);
    return Value.IsValid() && Value->Type == EJson::Number && Value->TryGetNumber(OutValue);
}

bool UOpenPocketBaseRecordLibrary::TryGetBooleanField(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseBooleanFieldRef Field,
    bool& OutValue)
{
    OutValue = false;
    const TSharedPtr<FJsonValue> Value = FindValue(Record, Field);
    return Value.IsValid() && Value->Type == EJson::Boolean && Value->TryGetBool(OutValue);
}

bool UOpenPocketBaseRecordLibrary::TryGetDateField(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseDateFieldRef Field,
    FDateTime& OutValue)
{
    FString DateString;
    const TSharedPtr<FJsonValue> JsonValue = FindValue(Record, Field);
    if (!JsonValue.IsValid() || !JsonValue->TryGetString(DateString))
    {
        return false;
    }
    return OpenPocketBase::Date::TryParse(DateString, OutValue);
}

bool UOpenPocketBaseRecordLibrary::TryGetStringArrayField(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseStringArrayFieldRef Field,
    TArray<FString>& OutValue)
{
    OutValue.Reset();
    const TSharedPtr<FJsonValue> Value = FindValue(Record, Field);
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
    const FOpenPocketBaseJsonFieldRef Field,
    FJsonObjectWrapper& OutValue)
{
    OutValue = FJsonObjectWrapper();
    const TSharedPtr<FJsonValue> Value = FindValue(Record, Field);
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
