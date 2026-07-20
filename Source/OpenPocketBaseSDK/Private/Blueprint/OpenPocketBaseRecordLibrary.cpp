#include "OpenPocketBaseRecordLibrary.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "OpenPocketBaseDate.h"
#include "Serialization/OpenPocketBaseJson.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
TSharedPtr<FJsonValue> FindValue(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseFieldRef& Field)
{
    FOpenPocketBaseFieldRef Current;
    if (!Field.ResolveCurrent(Current) || Record.CollectionId != Current.CollectionId)
    {
        return nullptr;
    }

    switch (Current.Storage)
    {
    case EOpenPocketBaseFieldStorage::RecordId:
        return MakeShared<FJsonValueString>(Record.Id);
    case EOpenPocketBaseFieldStorage::CollectionId:
        return MakeShared<FJsonValueString>(Record.CollectionId);
    case EOpenPocketBaseFieldStorage::CollectionName:
        return MakeShared<FJsonValueString>(Record.CollectionName);
    case EOpenPocketBaseFieldStorage::Created:
        return MakeShared<FJsonValueString>(OpenPocketBase::Date::Format(Record.Created));
    case EOpenPocketBaseFieldStorage::Updated:
        return MakeShared<FJsonValueString>(OpenPocketBase::Date::Format(Record.Updated));
    default:
        break;
    }

    if (!Record.Data.JsonObject.IsValid())
    {
        return nullptr;
    }
    return Record.Data.JsonObject->TryGetField(Current.Name);
}

EOpenPocketBaseFieldState GetBaseState(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseFieldRef& Field,
    const TSharedPtr<FJsonValue>& Value)
{
    FOpenPocketBaseFieldRef Current;
    if (!Field.ResolveCurrent(Current) || Record.CollectionId != Current.CollectionId)
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

EOpenPocketBaseFieldState ParseExpandedValue(
    const TSharedPtr<FJsonValue>& Value,
    TArray<FOpenPocketBaseRecord>& OutRecords)
{
    OutRecords.Reset();
    if (!Value.IsValid())
    {
        return EOpenPocketBaseFieldState::Missing;
    }
    if (Value->IsNull())
    {
        return EOpenPocketBaseFieldState::Null;
    }

    const TSharedPtr<FJsonObject>* Object = nullptr;
    if (Value->TryGetObject(Object) && Object != nullptr && Object->IsValid())
    {
        FOpenPocketBaseRecord Record;
        if (!OpenPocketBase::Json::TryParseRecordObject(Object->ToSharedRef(), Record))
        {
            return EOpenPocketBaseFieldState::WrongType;
        }
        OutRecords.Add(MoveTemp(Record));
        return EOpenPocketBaseFieldState::Found;
    }

    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Value->TryGetArray(Values) || Values == nullptr)
    {
        return EOpenPocketBaseFieldState::WrongType;
    }
    for (const TSharedPtr<FJsonValue>& Item : *Values)
    {
        const TSharedPtr<FJsonObject>* ItemObject = nullptr;
        if (!Item.IsValid() || !Item->TryGetObject(ItemObject) || ItemObject == nullptr ||
            !ItemObject->IsValid())
        {
            OutRecords.Reset();
            return EOpenPocketBaseFieldState::WrongType;
        }
        FOpenPocketBaseRecord Record;
        if (!OpenPocketBase::Json::TryParseRecordObject(ItemObject->ToSharedRef(), Record))
        {
            OutRecords.Reset();
            return EOpenPocketBaseFieldState::WrongType;
        }
        OutRecords.Add(MoveTemp(Record));
    }
    return EOpenPocketBaseFieldState::Found;
}
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::NewRecordBody(
    const FOpenPocketBaseCollection Collection)
{
    FOpenPocketBaseRecordBody Body;
    if (!Collection.Reference.IsSet())
    {
        if (!Collection.IsValid())
        {
            Body.bValid = false;
            Body.ErrorMessage = TEXT("Choose a valid collection before building its record body.");
        }
        return Body;
    }

    FOpenPocketBaseCollectionRef Current;
    if (!Collection.Reference.ResolveCurrent(Current) ||
        !FOpenPocketBaseWritableCollectionRef::Accepts(Current))
    {
        Body.bValid = false;
        Body.ErrorMessage = TEXT("Choose a writable collection before building its record body.");
        return Body;
    }
    Body.SchemaId = Current.SchemaId;
    Body.CollectionId = Current.CollectionId;
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithStringField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseTextFieldRef Field,
    const FString& Value)
{
    Body.SetStringField(Field, Value);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithNumberField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseNumberFieldRef Field,
    const double Value)
{
    Body.SetNumberField(Field, Value);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithBooleanField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseBooleanFieldRef Field,
    const bool bValue)
{
    Body.SetBooleanField(Field, bValue);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithNullField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseAnyFieldRef Field)
{
    Body.SetNullField(Field);
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

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithDateField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseDateFieldRef Field,
    const FDateTime Value)
{
    Body.SetDateField(Field, Value);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithJsonField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseJsonFieldRef Field,
    const FJsonObjectWrapper& Value)
{
    Body.SetJsonField(Field, Value);
    return Body;
}

FOpenPocketBaseGeoPoint UOpenPocketBaseRecordLibrary::MakeGeoPoint(
    const double Latitude,
    const double Longitude)
{
    FOpenPocketBaseGeoPoint Value;
    Value.Latitude = Latitude;
    Value.Longitude = Longitude;
    return Value;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithGeoPointField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseGeoPointFieldRef Field,
    const FOpenPocketBaseGeoPoint Value)
{
    Body.SetGeoPointField(Field, Value);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithSingleSelectField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseSingleSelectFieldRef Field,
    const FString& Value)
{
    Body.SetSingleSelectField(Field, Value);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithMultipleSelectField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseMultipleSelectFieldRef Field,
    const TArray<FString>& Values,
    const EOpenPocketBaseFieldModifier Modifier)
{
    Body.SetMultipleSelectField(Field, Values, Modifier);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithRelationRecord(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseSingleRelationFieldRef Field,
    const FOpenPocketBaseRecord& Record)
{
    Body.SetSingleRelationRecord(Field, Record);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithRelationRecords(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseMultipleRelationFieldRef Field,
    const TArray<FOpenPocketBaseRecord>& Records,
    const EOpenPocketBaseFieldModifier Modifier)
{
    Body.SetMultipleRelationRecords(Field, Records, Modifier);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithSingleRelationField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseSingleRelationFieldRef Field,
    const FString& RecordId)
{
    Body.SetSingleRelationField(Field, RecordId);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithMultipleRelationField(
    FOpenPocketBaseRecordBody Body,
    const FOpenPocketBaseMultipleRelationFieldRef Field,
    const TArray<FString>& RecordIds,
    const EOpenPocketBaseFieldModifier Modifier)
{
    Body.SetMultipleRelationField(Field, RecordIds, Modifier);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithDynamicStringField(
    FOpenPocketBaseRecordBody Body,
    const FString& FieldName,
    const FString& Value)
{
    Body.SetDynamicStringField(FieldName, Value);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithDynamicNumberField(
    FOpenPocketBaseRecordBody Body,
    const FString& FieldName,
    const double Value)
{
    Body.SetDynamicNumberField(FieldName, Value);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithDynamicBooleanField(
    FOpenPocketBaseRecordBody Body,
    const FString& FieldName,
    const bool bValue)
{
    Body.SetDynamicBooleanField(FieldName, bValue);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithDynamicNullField(
    FOpenPocketBaseRecordBody Body,
    const FString& FieldName)
{
    Body.SetDynamicNullField(FieldName);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithDynamicStringArrayField(
    FOpenPocketBaseRecordBody Body,
    const FString& FieldName,
    const TArray<FString>& Value)
{
    Body.SetDynamicStringArrayField(FieldName, Value);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithDynamicDateField(
    FOpenPocketBaseRecordBody Body,
    const FString& FieldName,
    const FDateTime Value)
{
    Body.SetDynamicDateField(FieldName, Value);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithDynamicJsonField(
    FOpenPocketBaseRecordBody Body,
    const FString& FieldName,
    const FJsonObjectWrapper& Value)
{
    Body.SetDynamicJsonField(FieldName, Value);
    return Body;
}

FOpenPocketBaseRecordBody UOpenPocketBaseRecordLibrary::WithDynamicGeoPointField(
    FOpenPocketBaseRecordBody Body,
    const FString& FieldName,
    const FOpenPocketBaseGeoPoint Value)
{
    Body.SetDynamicGeoPointField(FieldName, Value);
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
    return FindValue(Record, Field).IsValid();
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

bool UOpenPocketBaseRecordLibrary::TryGetGeoPointField(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseGeoPointFieldRef Field,
    FOpenPocketBaseGeoPoint& OutValue)
{
    OutValue = {};
    const TSharedPtr<FJsonValue> Value = FindValue(Record, Field);
    const TSharedPtr<FJsonObject>* Object = nullptr;
    return Value.IsValid() && Value->TryGetObject(Object) && Object != nullptr && Object->IsValid() &&
        (*Object)->TryGetNumberField(TEXT("lat"), OutValue.Latitude) &&
        (*Object)->TryGetNumberField(TEXT("lon"), OutValue.Longitude) && OutValue.IsValid();
}

bool UOpenPocketBaseRecordLibrary::TryGetSingleSelectField(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseSingleSelectFieldRef Field,
    FString& OutValue)
{
    OutValue.Reset();
    const TSharedPtr<FJsonValue> Value = FindValue(Record, Field);
    return Value.IsValid() && Value->TryGetString(OutValue);
}

bool UOpenPocketBaseRecordLibrary::TryGetMultipleSelectField(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseMultipleSelectFieldRef Field,
    TArray<FString>& OutValues)
{
    FOpenPocketBaseStringArrayFieldRef ArrayField;
    static_cast<FOpenPocketBaseFieldRef&>(ArrayField) = Field;
    return TryGetStringArrayField(Record, MoveTemp(ArrayField), OutValues);
}

bool UOpenPocketBaseRecordLibrary::TryGetSingleRelationId(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseSingleRelationFieldRef Field,
    FString& OutRecordId)
{
    OutRecordId.Reset();
    const TSharedPtr<FJsonValue> Value = FindValue(Record, Field);
    return Value.IsValid() && Value->TryGetString(OutRecordId);
}

bool UOpenPocketBaseRecordLibrary::TryGetMultipleRelationIds(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseMultipleRelationFieldRef Field,
    TArray<FString>& OutRecordIds)
{
    FOpenPocketBaseStringArrayFieldRef ArrayField;
    static_cast<FOpenPocketBaseFieldRef&>(ArrayField) = Field;
    return TryGetStringArrayField(Record, MoveTemp(ArrayField), OutRecordIds);
}

EOpenPocketBaseFieldState UOpenPocketBaseRecordLibrary::GetExpandedRecordState(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseSingleRelationFieldRef Relation,
    FOpenPocketBaseRecord& OutRecord)
{
    OutRecord = {};
    TArray<FOpenPocketBaseRecord> Records;
    FOpenPocketBaseRelationFieldRef AnyRelation;
    static_cast<FOpenPocketBaseFieldRef&>(AnyRelation) = Relation;
    const EOpenPocketBaseFieldState State = GetExpandedRecordsState(Record, AnyRelation, Records);
    if (State != EOpenPocketBaseFieldState::Found || Records.Num() != 1)
    {
        return State == EOpenPocketBaseFieldState::Found
            ? EOpenPocketBaseFieldState::WrongType
            : State;
    }
    OutRecord = MoveTemp(Records[0]);
    return EOpenPocketBaseFieldState::Found;
}

EOpenPocketBaseFieldState UOpenPocketBaseRecordLibrary::GetExpandedRecordsState(
    const FOpenPocketBaseRecord& Record,
    const FOpenPocketBaseRelationFieldRef Relation,
    TArray<FOpenPocketBaseRecord>& OutRecords)
{
    OutRecords.Reset();
    FOpenPocketBaseRelationFieldRef Current;
    if (!Relation.ResolveCurrentAs(Current) || Record.CollectionId != Current.CollectionId)
    {
        return EOpenPocketBaseFieldState::WrongCollection;
    }
    if (!Record.Expanded.JsonObject.IsValid())
    {
        return EOpenPocketBaseFieldState::Missing;
    }
    return ParseExpandedValue(Record.Expanded.JsonObject->TryGetField(Current.Name), OutRecords);
}

EOpenPocketBaseFieldState UOpenPocketBaseRecordLibrary::FollowExpansionPath(
    const FOpenPocketBaseRecord& Record,
    FOpenPocketBaseExpand Path,
    TArray<FOpenPocketBaseRecord>& OutRecords)
{
    OutRecords.Reset();
    if (!Path.IsSet())
    {
        return EOpenPocketBaseFieldState::WrongType;
    }

    TArray<FOpenPocketBaseRecord> CurrentRecords{Record};
    for (const FOpenPocketBaseRelationFieldRef& Relation : Path.Path)
    {
        TArray<FOpenPocketBaseRecord> NextRecords;
        for (const FOpenPocketBaseRecord& CurrentRecord : CurrentRecords)
        {
            TArray<FOpenPocketBaseRecord> ExpandedRecords;
            const EOpenPocketBaseFieldState State =
                GetExpandedRecordsState(CurrentRecord, Relation, ExpandedRecords);
            if (State != EOpenPocketBaseFieldState::Found)
            {
                OutRecords.Reset();
                return State;
            }
            NextRecords.Append(MoveTemp(ExpandedRecords));
        }
        CurrentRecords = MoveTemp(NextRecords);
    }
    OutRecords = MoveTemp(CurrentRecords);
    return EOpenPocketBaseFieldState::Found;
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
