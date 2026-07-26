#include "OpenPocketBaseRecord.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "OpenPocketBaseDate.h"

namespace
{
TSharedPtr<FJsonValue> CopyJsonValue(const TSharedPtr<FJsonValue>& Value);

TSharedRef<FJsonObject> CopyJsonObject(const TSharedPtr<FJsonObject>& Source)
{
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    if (!Source.IsValid())
    {
        return Result;
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Source->Values)
    {
        Result->SetField(Pair.Key, CopyJsonValue(Pair.Value));
    }
    return Result;
}

TSharedPtr<FJsonValue> CopyJsonValue(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid() || Value->Type == EJson::Null)
    {
        return MakeShared<FJsonValueNull>();
    }

    switch (Value->Type)
    {
    case EJson::String:
    {
        FString Result;
        if (Value->TryGetString(Result))
        {
            return MakeShared<FJsonValueString>(MoveTemp(Result));
        }
        return MakeShared<FJsonValueNull>();
    }
    case EJson::Number:
    {
        double Result = 0.0;
        if (Value->TryGetNumber(Result))
        {
            return MakeShared<FJsonValueNumber>(Result);
        }
        return MakeShared<FJsonValueNull>();
    }
    case EJson::Boolean:
    {
        bool bResult = false;
        if (Value->TryGetBool(bResult))
        {
            return MakeShared<FJsonValueBoolean>(bResult);
        }
        return MakeShared<FJsonValueNull>();
    }
    case EJson::Object:
    {
        const TSharedPtr<FJsonObject>* Result = nullptr;
        if (Value->TryGetObject(Result) && Result != nullptr)
        {
            return MakeShared<FJsonValueObject>(CopyJsonObject(*Result));
        }
        return MakeShared<FJsonValueNull>();
    }
    case EJson::Array:
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        TArray<TSharedPtr<FJsonValue>> Result;
        if (Value->TryGetArray(Values) && Values != nullptr)
        {
            Result.Reserve(Values->Num());
            for (const TSharedPtr<FJsonValue>& Item : *Values)
            {
                Result.Add(CopyJsonValue(Item));
            }
        }
        return MakeShared<FJsonValueArray>(MoveTemp(Result));
    }
    default:
        return MakeShared<FJsonValueNull>();
    }
}

TSharedRef<FJsonObject> GetOrCreateBodyObject(FOpenPocketBaseRecordBody& Body)
{
    if (!Body.Data.JsonObject.IsValid())
    {
        Body.Data.JsonObject = MakeShared<FJsonObject>();
    }
    Body.Data.JsonString.Reset();
    return Body.Data.JsonObject.ToSharedRef();
}

TArray<TSharedPtr<FJsonValue>> StringValues(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    Result.Reserve(Values.Num());
    for (const FString& Value : Values)
    {
        Result.Add(MakeShared<FJsonValueString>(Value));
    }
    return Result;
}

TSharedRef<FJsonObject> GeoPointValue(const FOpenPocketBaseGeoPoint& Value)
{
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("lat"), Value.Latitude);
    Result->SetNumberField(TEXT("lon"), Value.Longitude);
    return Result;
}

FOpenPocketBaseRecordBody& InvalidateBody(
    FOpenPocketBaseRecordBody& Body,
    FString Message)
{
    Body.bValid = false;
    Body.ErrorMessage = MoveTemp(Message);
    return Body;
}

bool IsValidDynamicFieldName(const FString& Name)
{
    if (Name.IsEmpty() || Name.Len() > 255 ||
        (!FChar::IsAlpha(Name[0]) && Name[0] != TEXT('_')))
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

bool ValidateDynamicField(
    FOpenPocketBaseRecordBody& Body,
    const FString& FieldName)
{
    if (!Body.bValid)
    {
        return false;
    }
    if (IsValidDynamicFieldName(FieldName))
    {
        return true;
    }
    InvalidateBody(
        Body,
        FString::Printf(
            TEXT("Dynamic Record Field '%s' is invalid. Use a PocketBase field name up to 255 characters containing only letters, numbers, and underscores."),
            *FieldName));
    return false;
}
}

bool FOpenPocketBaseGeoPoint::IsValid() const
{
    return FMath::IsFinite(Latitude) && FMath::IsFinite(Longitude) &&
        Latitude >= -90.0 && Latitude <= 90.0 &&
        Longitude >= -180.0 && Longitude <= 180.0;
}

FOpenPocketBaseRecordBody::FOpenPocketBaseRecordBody()
{
    Data.JsonObject = MakeShared<FJsonObject>();
}

FOpenPocketBaseRecordBody::FOpenPocketBaseRecordBody(const FOpenPocketBaseRecordBody& Other)
{
    *this = Other;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::operator=(
    const FOpenPocketBaseRecordBody& Other)
{
    if (this == &Other)
    {
        return *this;
    }

    Data.JsonString = Other.Data.JsonString;
    if (Other.Data.JsonObject.IsValid())
    {
        Data.JsonObject = CopyJsonObject(Other.Data.JsonObject);
    }
    else
    {
        Data.JsonObject.Reset();
    }
    bValid = Other.bValid;
    ErrorMessage = Other.ErrorMessage;
    SchemaId = Other.SchemaId;
    CollectionId = Other.CollectionId;
    return *this;
}

bool FOpenPocketBaseRecordBody::AcceptField(
    const FOpenPocketBaseFieldRef& Field,
    FOpenPocketBaseFieldRef& OutCurrentField)
{
    if (!bValid)
    {
        return false;
    }
    if (!Field.ResolveCurrent(OutCurrentField))
    {
        InvalidateBody(
            *this,
            TEXT("Record Body Field is missing or stale. Choose the field again from the current collection schema."));
        return false;
    }
    if (OutCurrentField.bReadOnly)
    {
        InvalidateBody(
            *this,
            FString::Printf(
                TEXT("Field '%s' is read-only and cannot be added to a Record Body. Choose a writable field."),
                *OutCurrentField.Name));
        return false;
    }
    if (!SchemaId.IsValid())
    {
        SchemaId = OutCurrentField.SchemaId;
        CollectionId = OutCurrentField.CollectionId;
        return true;
    }
    if (SchemaId != OutCurrentField.SchemaId || CollectionId != OutCurrentField.CollectionId)
    {
        InvalidateBody(
            *this,
            TEXT("This Record Body already contains fields from another collection. Start a new body from the intended Collection pin."));
        return false;
    }
    return true;
}

bool FOpenPocketBaseRecordBody::IsValid() const
{
    return bValid;
}

bool FOpenPocketBaseRecordBody::BelongsTo(const FOpenPocketBaseCollectionRef& Collection) const
{
    return !SchemaId.IsValid() ||
        (Collection.IsSet() && SchemaId == Collection.SchemaId && CollectionId == Collection.CollectionId);
}

FString FOpenPocketBaseRecordBody::MakeModifiedFieldName(
    const FString& FieldName,
    const EOpenPocketBaseFieldModifier Modifier)
{
    switch (Modifier)
    {
    case EOpenPocketBaseFieldModifier::Append:
        return FieldName + TEXT("+");
    case EOpenPocketBaseFieldModifier::Prepend:
        return TEXT("+") + FieldName;
    case EOpenPocketBaseFieldModifier::Remove:
        return FieldName + TEXT("-");
    default:
        return FieldName;
    }
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetStringField(
    const FOpenPocketBaseTextFieldRef& Field,
    const FString& Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    FOpenPocketBaseFieldRef Current;
    if (!AcceptField(Field, Current))
    {
        return *this;
    }
    if (!FOpenPocketBaseTextFieldRef::Accepts(Current))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Field '%s' is not a text, email, URL, editor, or password field. Use the field node matching its schema type."),
            *Current.Name));
    }
    if (Modifier != EOpenPocketBaseFieldModifier::Replace)
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("String field '%s' only supports the Replace modifier. Choose Replace before setting it."),
            *Current.Name));
    }
    if (Current.bHasMin && Value.Len() < Current.Min)
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("String field '%s' contains %d characters, but its minimum is %.0f."),
            *Current.Name,
            Value.Len(),
            Current.Min));
    }
    if (Current.bHasMax && Value.Len() > Current.Max)
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("String field '%s' contains %d characters, but its maximum is %.0f."),
            *Current.Name,
            Value.Len(),
            Current.Max));
    }
    GetOrCreateBodyObject(*this)->SetStringField(MakeModifiedFieldName(Current.Name, Modifier), Value);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetNumberField(
    const FOpenPocketBaseNumberFieldRef& Field,
    const double Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    FOpenPocketBaseFieldRef Current;
    if (!AcceptField(Field, Current))
    {
        return *this;
    }
    if (!FOpenPocketBaseNumberFieldRef::Accepts(Current))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Field '%s' is not a number field. Use With Number Field only with a number field from the schema."),
            *Current.Name));
    }
    if (!FMath::IsFinite(Value))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Number field '%s' requires a finite value. NaN and infinity cannot be sent to PocketBase."),
            *Current.Name));
    }
    if (Modifier == EOpenPocketBaseFieldModifier::Prepend)
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Number field '%s' does not support the Prepend modifier. Use Replace, Append, or Remove."),
            *Current.Name));
    }
    if (Current.bHasMin && Value < Current.Min)
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Number field '%s' has value %.17g, but its minimum is %.17g."),
            *Current.Name,
            Value,
            Current.Min));
    }
    if (Current.bHasMax && Value > Current.Max)
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Number field '%s' has value %.17g, but its maximum is %.17g."),
            *Current.Name,
            Value,
            Current.Max));
    }
    GetOrCreateBodyObject(*this)->SetNumberField(MakeModifiedFieldName(Current.Name, Modifier), Value);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetBooleanField(
    const FOpenPocketBaseBooleanFieldRef& Field,
    const bool bValue,
    const EOpenPocketBaseFieldModifier Modifier)
{
    FOpenPocketBaseFieldRef Current;
    if (!AcceptField(Field, Current))
    {
        return *this;
    }
    if (!FOpenPocketBaseBooleanFieldRef::Accepts(Current))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Field '%s' is not a Boolean field. Use With Boolean Field only with a Boolean field from the schema."),
            *Current.Name));
    }
    if (Modifier != EOpenPocketBaseFieldModifier::Replace)
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Boolean field '%s' only supports the Replace modifier."),
            *Current.Name));
    }
    GetOrCreateBodyObject(*this)->SetBoolField(MakeModifiedFieldName(Current.Name, Modifier), bValue);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetNullField(
    const FOpenPocketBaseFieldRef& Field,
    const EOpenPocketBaseFieldModifier Modifier)
{
    FOpenPocketBaseFieldRef Current;
    if (!AcceptField(Field, Current))
    {
        return *this;
    }
    if (Modifier != EOpenPocketBaseFieldModifier::Replace)
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Null field '%s' only supports the Replace modifier."),
            *Current.Name));
    }
    GetOrCreateBodyObject(*this)->SetField(
        MakeModifiedFieldName(Current.Name, Modifier),
        MakeShared<FJsonValueNull>());
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetStringArrayField(
    const FOpenPocketBaseStringArrayFieldRef& Field,
    const TArray<FString>& Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    FOpenPocketBaseFieldRef Current;
    if (!AcceptField(Field, Current))
    {
        return *this;
    }
    if (!FOpenPocketBaseStringArrayFieldRef::Accepts(Current))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Field '%s' is not a string-array field. Choose a compatible multiple-value field from the schema."),
            *Current.Name));
    }
    GetOrCreateBodyObject(*this)->SetArrayField(
        MakeModifiedFieldName(Current.Name, Modifier),
        StringValues(Value));
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetDateField(
    const FOpenPocketBaseDateFieldRef& Field,
    const FDateTime& Value)
{
    FOpenPocketBaseFieldRef Current;
    if (!AcceptField(Field, Current))
    {
        return *this;
    }
    if (!FOpenPocketBaseDateFieldRef::Accepts(Current))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Field '%s' is not a date field. Use With Date Field only with a date field from the schema."),
            *Current.Name));
    }
    GetOrCreateBodyObject(*this)->SetStringField(Current.Name, OpenPocketBase::Date::Format(Value));
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetJsonField(
    const FOpenPocketBaseJsonFieldRef& Field,
    const FOpenPocketBaseJsonValue& Value)
{
    FOpenPocketBaseFieldRef Current;
    const TSharedPtr<FJsonValue> JsonValue = Value.ToJsonValue();
    if (!AcceptField(Field, Current))
    {
        return *this;
    }
    if (!FOpenPocketBaseJsonFieldRef::Accepts(Current))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Field '%s' is not a JSON field. Use With JSON Field only with a JSON field from the schema."),
            *Current.Name));
    }
    if (!JsonValue.IsValid())
    {
        return InvalidateBody(
            *this,
            Value.ErrorMessage.IsEmpty()
                ? FString::Printf(
                      TEXT("JSON value for field '%s' is empty or invalid. Build a JSON value before setting the field."),
                      *Current.Name)
                : Value.ErrorMessage);
    }
    GetOrCreateBodyObject(*this)->SetField(Current.Name, JsonValue);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetGeoPointField(
    const FOpenPocketBaseGeoPointFieldRef& Field,
    const FOpenPocketBaseGeoPoint& Value)
{
    FOpenPocketBaseFieldRef Current;
    if (!AcceptField(Field, Current))
    {
        return *this;
    }
    if (!FOpenPocketBaseGeoPointFieldRef::Accepts(Current))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Field '%s' is not a geo point field. Use With Geo Point Field only with a geo field from the schema."),
            *Current.Name));
    }
    if (!Value.IsValid())
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Geo field '%s' requires finite coordinates with latitude from -90 to 90 and longitude from -180 to 180."),
            *Current.Name));
    }
    GetOrCreateBodyObject(*this)->SetObjectField(Current.Name, GeoPointValue(Value));
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetSingleSelectField(
    const FOpenPocketBaseSingleSelectFieldRef& Field,
    const FString& Value)
{
    FOpenPocketBaseFieldRef Current;
    if (!AcceptField(Field, Current))
    {
        return *this;
    }
    if (!FOpenPocketBaseSingleSelectFieldRef::Accepts(Current))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Field '%s' is not a single-select field. Choose a single-select field from the schema."),
            *Current.Name));
    }
    if (!Current.Choices.IsEmpty() && !Current.Choices.Contains(Value))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Value '%s' is not allowed by select field '%s'. Choose one of the field's schema choices."),
            *Value,
            *Current.Name));
    }
    GetOrCreateBodyObject(*this)->SetStringField(Current.Name, Value);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetMultipleSelectField(
    const FOpenPocketBaseMultipleSelectFieldRef& Field,
    const TArray<FString>& Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    FOpenPocketBaseFieldRef Current;
    if (!AcceptField(Field, Current))
    {
        return *this;
    }
    if (!FOpenPocketBaseMultipleSelectFieldRef::Accepts(Current))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Field '%s' is not a multiple-select field. Choose a multiple-select field from the schema."),
            *Current.Name));
    }
    const bool bInvalidChoice = Value.ContainsByPredicate(
        [&Current](const FString& Choice)
        {
            return !Current.Choices.IsEmpty() && !Current.Choices.Contains(Choice);
        });
    if (bInvalidChoice ||
        (Modifier == EOpenPocketBaseFieldModifier::Replace &&
         (Value.Num() < Current.MinSelect ||
          (Current.MaxSelect > 0 && Value.Num() > Current.MaxSelect))))
    {
        if (bInvalidChoice)
        {
            const FString* InvalidChoice = Value.FindByPredicate(
                [&Current](const FString& Choice)
                {
                    return !Current.Choices.Contains(Choice);
                });
            return InvalidateBody(*this, FString::Printf(
                TEXT("Value '%s' is not allowed by select field '%s'. Choose only values listed in the field's schema choices."),
                InvalidChoice != nullptr ? **InvalidChoice : TEXT(""),
                *Current.Name));
        }
        return Current.MaxSelect > 0
            ? InvalidateBody(*this, FString::Printf(
                  TEXT("Select field '%s' received %d values, but it requires from %d to %d values."),
                  *Current.Name,
                  Value.Num(),
                  Current.MinSelect,
                  Current.MaxSelect))
            : InvalidateBody(*this, FString::Printf(
                  TEXT("Select field '%s' received %d values, but it requires at least %d values."),
                  *Current.Name,
                  Value.Num(),
                  Current.MinSelect));
    }
    GetOrCreateBodyObject(*this)->SetArrayField(
        MakeModifiedFieldName(Current.Name, Modifier), StringValues(Value));
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetSingleRelationField(
    const FOpenPocketBaseSingleRelationFieldRef& Field,
    const FString& RecordId)
{
    FOpenPocketBaseFieldRef Current;
    if (!AcceptField(Field, Current))
    {
        return *this;
    }
    if (!FOpenPocketBaseSingleRelationFieldRef::Accepts(Current))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Field '%s' is not a single-relation field. Choose a single-relation field from the schema."),
            *Current.Name));
    }
    if (RecordId.IsEmpty())
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Relation field '%s' requires a non-empty Record ID returned by PocketBase."),
            *Current.Name));
    }
    GetOrCreateBodyObject(*this)->SetStringField(Current.Name, RecordId);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetMultipleRelationField(
    const FOpenPocketBaseMultipleRelationFieldRef& Field,
    const TArray<FString>& RecordIds,
    const EOpenPocketBaseFieldModifier Modifier)
{
    FOpenPocketBaseFieldRef Current;
    if (!AcceptField(Field, Current))
    {
        return *this;
    }
    if (!FOpenPocketBaseMultipleRelationFieldRef::Accepts(Current))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Field '%s' is not a multiple-relation field. Choose a multiple-relation field from the schema."),
            *Current.Name));
    }
    if (RecordIds.ContainsByPredicate([](const FString& Id) { return Id.IsEmpty(); }))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Relation field '%s' contains an empty Record ID. Use IDs returned by PocketBase."),
            *Current.Name));
    }
    if (Modifier == EOpenPocketBaseFieldModifier::Replace &&
        (RecordIds.Num() < Current.MinSelect ||
         (Current.MaxSelect > 0 && RecordIds.Num() > Current.MaxSelect)))
    {
        return Current.MaxSelect > 0
            ? InvalidateBody(*this, FString::Printf(
                  TEXT("Relation field '%s' received %d Record IDs, but it requires from %d to %d."),
                  *Current.Name,
                  RecordIds.Num(),
                  Current.MinSelect,
                  Current.MaxSelect))
            : InvalidateBody(*this, FString::Printf(
                  TEXT("Relation field '%s' received %d Record IDs, but it requires at least %d."),
                  *Current.Name,
                  RecordIds.Num(),
                  Current.MinSelect));
    }
    GetOrCreateBodyObject(*this)->SetArrayField(
        MakeModifiedFieldName(Current.Name, Modifier), StringValues(RecordIds));
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetSingleRelationRecord(
    const FOpenPocketBaseSingleRelationFieldRef& Field,
    const FOpenPocketBaseRecord& Record)
{
    FOpenPocketBaseFieldRef Current;
    if (!AcceptField(Field, Current))
    {
        return *this;
    }
    if (!FOpenPocketBaseSingleRelationFieldRef::Accepts(Current))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Field '%s' is not a single-relation field. Choose a single-relation field from the schema."),
            *Current.Name));
    }
    if (Record.Id.IsEmpty())
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Relation field '%s' received a record with no ID. Fetch or create the related record before connecting it."),
            *Current.Name));
    }
    if (Record.CollectionId != Current.RelatedCollectionId)
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Record '%s' belongs to another collection and cannot be assigned to relation field '%s'. Choose a record from that field's related collection."),
            *Record.Id,
            *Current.Name));
    }
    GetOrCreateBodyObject(*this)->SetStringField(Current.Name, Record.Id);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetMultipleRelationRecords(
    const FOpenPocketBaseMultipleRelationFieldRef& Field,
    const TArray<FOpenPocketBaseRecord>& Records,
    const EOpenPocketBaseFieldModifier Modifier)
{
    FOpenPocketBaseFieldRef Current;
    const FOpenPocketBaseRecord* InvalidRecord = nullptr;
    if (Field.ResolveCurrent(Current))
    {
        InvalidRecord = Records.FindByPredicate(
            [&Current](const FOpenPocketBaseRecord& Record)
            {
                return Record.Id.IsEmpty() || Record.CollectionId != Current.RelatedCollectionId;
            });
    }
    if (!AcceptField(Field, Current))
    {
        return *this;
    }
    if (!FOpenPocketBaseMultipleRelationFieldRef::Accepts(Current))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Field '%s' is not a multiple-relation field. Choose a multiple-relation field from the schema."),
            *Current.Name));
    }
    if (InvalidRecord != nullptr)
    {
        return InvalidateBody(*this, InvalidRecord->Id.IsEmpty()
            ? FString::Printf(
                  TEXT("Relation field '%s' received a record with no ID. Fetch or create every related record before connecting them."),
                  *Current.Name)
            : FString::Printf(
                  TEXT("Record '%s' belongs to another collection and cannot be assigned to relation field '%s'. Choose records from that field's related collection."),
                  *InvalidRecord->Id,
                  *Current.Name));
    }

    TArray<FString> RecordIds;
    RecordIds.Reserve(Records.Num());
    for (const FOpenPocketBaseRecord& Record : Records)
    {
        RecordIds.Add(Record.Id);
    }
    return SetMultipleRelationField(Field, RecordIds, Modifier);
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetDynamicStringField(
    const FString& FieldName,
    const FString& Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    if (!ValidateDynamicField(*this, FieldName))
    {
        return *this;
    }
    GetOrCreateBodyObject(*this)->SetStringField(MakeModifiedFieldName(FieldName, Modifier), Value);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetDynamicNumberField(
    const FString& FieldName,
    const double Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    if (!ValidateDynamicField(*this, FieldName))
    {
        return *this;
    }
    if (!FMath::IsFinite(Value))
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Dynamic number field '%s' requires a finite value. NaN and infinity cannot be sent to PocketBase."),
            *FieldName));
    }
    GetOrCreateBodyObject(*this)->SetNumberField(MakeModifiedFieldName(FieldName, Modifier), Value);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetDynamicBooleanField(
    const FString& FieldName,
    const bool bValue,
    const EOpenPocketBaseFieldModifier Modifier)
{
    if (!ValidateDynamicField(*this, FieldName))
    {
        return *this;
    }
    GetOrCreateBodyObject(*this)->SetBoolField(MakeModifiedFieldName(FieldName, Modifier), bValue);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetDynamicNullField(
    const FString& FieldName,
    const EOpenPocketBaseFieldModifier Modifier)
{
    if (!ValidateDynamicField(*this, FieldName))
    {
        return *this;
    }
    GetOrCreateBodyObject(*this)->SetField(
        MakeModifiedFieldName(FieldName, Modifier),
        MakeShared<FJsonValueNull>());
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetDynamicStringArrayField(
    const FString& FieldName,
    const TArray<FString>& Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    if (!ValidateDynamicField(*this, FieldName))
    {
        return *this;
    }
    GetOrCreateBodyObject(*this)->SetArrayField(
        MakeModifiedFieldName(FieldName, Modifier),
        StringValues(Value));
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetDynamicDateField(
    const FString& FieldName,
    const FDateTime& Value)
{
    if (!ValidateDynamicField(*this, FieldName))
    {
        return *this;
    }
    GetOrCreateBodyObject(*this)->SetStringField(FieldName, OpenPocketBase::Date::Format(Value));
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetDynamicJsonField(
    const FString& FieldName,
    const FOpenPocketBaseJsonValue& Value)
{
    if (!ValidateDynamicField(*this, FieldName))
    {
        return *this;
    }
    const TSharedPtr<FJsonValue> JsonValue = Value.ToJsonValue();
    if (!JsonValue.IsValid())
    {
        bValid = false;
        ErrorMessage = Value.ErrorMessage.IsEmpty()
            ? FString::Printf(
                  TEXT("Dynamic JSON field '%s' requires a valid JSON value. Build the value before setting the field."),
                  *FieldName)
            : Value.ErrorMessage;
        return *this;
    }
    GetOrCreateBodyObject(*this)->SetField(FieldName, JsonValue);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetDynamicGeoPointField(
    const FString& FieldName,
    const FOpenPocketBaseGeoPoint& Value)
{
    if (!ValidateDynamicField(*this, FieldName))
    {
        return *this;
    }
    if (!Value.IsValid())
    {
        return InvalidateBody(*this, FString::Printf(
            TEXT("Dynamic geo field '%s' requires finite coordinates with latitude from -90 to 90 and longitude from -180 to 180."),
            *FieldName));
    }
    GetOrCreateBodyObject(*this)->SetObjectField(FieldName, GeoPointValue(Value));
    return *this;
}

FOpenPocketBaseRecordOptions& FOpenPocketBaseRecordOptions::WithExpand(
    TArray<FOpenPocketBaseExpand> InExpand)
{
    Expand = MoveTemp(InExpand);
    return *this;
}

FOpenPocketBaseRecordOptions& FOpenPocketBaseRecordOptions::WithFields(
    TArray<FOpenPocketBaseFieldSelection> InFields)
{
    Fields = MoveTemp(InFields);
    return *this;
}

FOpenPocketBaseRecordOptions& FOpenPocketBaseRecordOptions::Including(
    FOpenPocketBaseExpand InExpand)
{
    Expand.Add(MoveTemp(InExpand));
    return *this;
}

FOpenPocketBaseRecordOptions& FOpenPocketBaseRecordOptions::Selecting(
    FOpenPocketBaseFieldSelection InField)
{
    Fields.Add(MoveTemp(InField));
    return *this;
}

FOpenPocketBaseRecordOptions& FOpenPocketBaseRecordOptions::WithRequestOptions(
    FOpenPocketBaseRequestOptions InOptions)
{
    RequestOptions = MoveTemp(InOptions);
    return *this;
}

bool FOpenPocketBaseRecordOptions::BelongsTo(
    const FOpenPocketBaseCollectionRef& Collection) const
{
    if (!IsValid())
    {
        return false;
    }
    for (const FOpenPocketBaseExpand& Value : Expand)
    {
        if (!Value.BelongsTo(Collection))
        {
            return false;
        }
    }
    for (const FOpenPocketBaseFieldSelection& Value : Fields)
    {
        if (!Value.BelongsTo(Collection))
        {
            return false;
        }
    }
    return true;
}

bool FOpenPocketBaseRecordOptions::IsValid() const
{
    for (const FOpenPocketBaseExpand& Value : Expand)
    {
        if (!Value.IsSet())
        {
            return false;
        }
    }
    for (const FOpenPocketBaseFieldSelection& Value : Fields)
    {
        if (!Value.IsSet())
        {
            return false;
        }
    }
    return true;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::AtPage(const int32 InPage)
{
    Page = FMath::Max(1, InPage);
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::PageSize(const int32 InPerPage)
{
    PerPage = FMath::Max(1, InPerPage);
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::Where(FOpenPocketBaseFilter InFilter)
{
    Filter = MoveTemp(InFilter);
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::WithSort(
    TArray<FOpenPocketBaseSort> InSort)
{
    Sort = MoveTemp(InSort);
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::WithExpand(
    TArray<FOpenPocketBaseExpand> InExpand)
{
    Expand = MoveTemp(InExpand);
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::WithFields(
    TArray<FOpenPocketBaseFieldSelection> InFields)
{
    Fields = MoveTemp(InFields);
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::OrderedBy(FOpenPocketBaseSort InSort)
{
    Sort.Add(MoveTemp(InSort));
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::Including(
    FOpenPocketBaseExpand InExpand)
{
    Expand.Add(MoveTemp(InExpand));
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::Selecting(
    FOpenPocketBaseFieldSelection InField)
{
    Fields.Add(MoveTemp(InField));
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::SkipTotals(const bool bInSkipTotal)
{
    bSkipTotal = bInSkipTotal;
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::WithRequestOptions(
    FOpenPocketBaseRequestOptions InOptions)
{
    RequestOptions = MoveTemp(InOptions);
    return *this;
}

bool FOpenPocketBaseListOptions::BelongsTo(
    const FOpenPocketBaseCollectionRef& Collection) const
{
    if (!IsValid() || !Filter.BelongsTo(Collection))
    {
        return false;
    }
    for (const FOpenPocketBaseSort& Value : Sort)
    {
        if (!Value.BelongsTo(Collection))
        {
            return false;
        }
    }
    for (const FOpenPocketBaseExpand& Value : Expand)
    {
        if (!Value.BelongsTo(Collection))
        {
            return false;
        }
    }
    for (const FOpenPocketBaseFieldSelection& Value : Fields)
    {
        if (!Value.BelongsTo(Collection))
        {
            return false;
        }
    }
    return true;
}

bool FOpenPocketBaseListOptions::IsValid() const
{
    if (!Filter.IsValid())
    {
        return false;
    }
    for (const FOpenPocketBaseSort& Value : Sort)
    {
        if (!Value.IsSet())
        {
            return false;
        }
    }
    for (const FOpenPocketBaseExpand& Value : Expand)
    {
        if (!Value.IsSet())
        {
            return false;
        }
    }
    for (const FOpenPocketBaseFieldSelection& Value : Fields)
    {
        if (!Value.IsSet())
        {
            return false;
        }
    }
    return true;
}

FOpenPocketBaseFullListOptions& FOpenPocketBaseFullListOptions::WithListOptions(
    FOpenPocketBaseListOptions InOptions)
{
    ListOptions = MoveTemp(InOptions);
    return *this;
}

FOpenPocketBaseFullListOptions& FOpenPocketBaseFullListOptions::LimitItems(const int32 InMaxItems)
{
    MaxItems = FMath::Max(0, InMaxItems);
    return *this;
}

FOpenPocketBaseFullListOptions& FOpenPocketBaseFullListOptions::LimitPages(const int32 InMaxPages)
{
    MaxPages = FMath::Max(0, InMaxPages);
    return *this;
}
