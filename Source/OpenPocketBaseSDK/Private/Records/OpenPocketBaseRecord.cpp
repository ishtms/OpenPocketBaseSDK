#include "OpenPocketBaseRecord.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "OpenPocketBaseDate.h"

namespace
{
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
        Data.JsonObject = MakeShared<FJsonObject>();
        FJsonObject::Duplicate(Other.Data.JsonObject, Data.JsonObject);
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
        bValid = false;
        ErrorMessage = TEXT("Choose a valid collection field for the record body.");
        return false;
    }
    if (OutCurrentField.bReadOnly)
    {
        bValid = false;
        ErrorMessage = FString::Printf(TEXT("Field '%s' is read-only."), *OutCurrentField.Name);
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
        bValid = false;
        ErrorMessage = TEXT("A record body cannot contain fields from different collections.");
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
    if (!AcceptField(Field, Current) || !FOpenPocketBaseTextFieldRef::Accepts(Current) ||
        Modifier != EOpenPocketBaseFieldModifier::Replace ||
        (Current.bHasMin && Value.Len() < Current.Min) ||
        (Current.bHasMax && Value.Len() > Current.Max))
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = TEXT("Choose a writable string field for the record body.");
        }
        return *this;
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
    if (!AcceptField(Field, Current) || !FOpenPocketBaseNumberFieldRef::Accepts(Current) ||
        Modifier == EOpenPocketBaseFieldModifier::Prepend ||
        (Current.bHasMin && Value < Current.Min) ||
        (Current.bHasMax && Value > Current.Max))
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = TEXT("Choose a writable number field for the record body.");
        }
        return *this;
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
    if (!AcceptField(Field, Current) || !FOpenPocketBaseBooleanFieldRef::Accepts(Current) ||
        Modifier != EOpenPocketBaseFieldModifier::Replace)
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = TEXT("Choose a writable boolean field for the record body.");
        }
        return *this;
    }
    GetOrCreateBodyObject(*this)->SetBoolField(MakeModifiedFieldName(Current.Name, Modifier), bValue);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetNullField(
    const FOpenPocketBaseFieldRef& Field,
    const EOpenPocketBaseFieldModifier Modifier)
{
    FOpenPocketBaseFieldRef Current;
    if (!AcceptField(Field, Current) || Modifier != EOpenPocketBaseFieldModifier::Replace)
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = TEXT("Choose a writable field for the record body.");
        }
        return *this;
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
    if (!AcceptField(Field, Current) || !FOpenPocketBaseStringArrayFieldRef::Accepts(Current))
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = TEXT("Choose a writable string-array field for the record body.");
        }
        return *this;
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
    if (!AcceptField(Field, Current) || !FOpenPocketBaseDateFieldRef::Accepts(Current))
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = TEXT("Choose a writable date field for the record body.");
        }
        return *this;
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
    if (!AcceptField(Field, Current) || !FOpenPocketBaseJsonFieldRef::Accepts(Current) ||
        !JsonValue.IsValid())
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = Value.ErrorMessage.IsEmpty()
                ? TEXT("Choose a writable JSON field and a valid JSON value.")
                : Value.ErrorMessage;
        }
        return *this;
    }
    GetOrCreateBodyObject(*this)->SetField(Current.Name, JsonValue);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetGeoPointField(
    const FOpenPocketBaseGeoPointFieldRef& Field,
    const FOpenPocketBaseGeoPoint& Value)
{
    FOpenPocketBaseFieldRef Current;
    if (!AcceptField(Field, Current) || !FOpenPocketBaseGeoPointFieldRef::Accepts(Current) ||
        !Value.IsValid())
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = TEXT("Choose a writable geo point field and valid coordinates.");
        }
        return *this;
    }
    GetOrCreateBodyObject(*this)->SetObjectField(Current.Name, GeoPointValue(Value));
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetSingleSelectField(
    const FOpenPocketBaseSingleSelectFieldRef& Field,
    const FString& Value)
{
    FOpenPocketBaseFieldRef Current;
    if (!AcceptField(Field, Current) || !FOpenPocketBaseSingleSelectFieldRef::Accepts(Current) ||
        (!Current.Choices.IsEmpty() && !Current.Choices.Contains(Value)))
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = Current.IsSet() && !Current.Choices.IsEmpty()
                ? FString::Printf(
                      TEXT("Value '%s' is not allowed by select field '%s'."),
                      *Value,
                      *Current.Name)
                : TEXT("Choose a writable single-select field.");
        }
        return *this;
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
    if (!AcceptField(Field, Current) || !FOpenPocketBaseMultipleSelectFieldRef::Accepts(Current))
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = TEXT("Choose a writable multiple-select field.");
        }
        return *this;
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
        if (bValid)
        {
            bValid = false;
            if (bInvalidChoice)
            {
                const FString* InvalidChoice = Value.FindByPredicate(
                    [&Current](const FString& Choice)
                    {
                        return !Current.Choices.Contains(Choice);
                    });
                ErrorMessage = FString::Printf(
                    TEXT("Value '%s' is not allowed by select field '%s'."),
                    InvalidChoice != nullptr ? **InvalidChoice : TEXT(""),
                    *Current.Name);
            }
            else
            {
                ErrorMessage = FString::Printf(
                    TEXT("Select field '%s' requires between %d and %d values."),
                    *Current.Name,
                    Current.MinSelect,
                    Current.MaxSelect);
            }
        }
        return *this;
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
    if (!AcceptField(Field, Current) || !FOpenPocketBaseSingleRelationFieldRef::Accepts(Current))
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = TEXT("Choose a writable single-relation field.");
        }
        return *this;
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
    if (!AcceptField(Field, Current) || !FOpenPocketBaseMultipleRelationFieldRef::Accepts(Current) ||
        (Modifier == EOpenPocketBaseFieldModifier::Replace &&
         (RecordIds.Num() < Current.MinSelect ||
          (Current.MaxSelect > 0 && RecordIds.Num() > Current.MaxSelect))))
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = TEXT("Choose a writable multiple-relation field.");
        }
        return *this;
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
    if (!AcceptField(Field, Current) || !FOpenPocketBaseSingleRelationFieldRef::Accepts(Current) ||
        Record.Id.IsEmpty() || Record.CollectionId != Current.RelatedCollectionId)
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = Current.IsSet()
                ? FString::Printf(
                      TEXT("Record '%s' does not belong to relation field '%s'."),
                      *Record.Id,
                      *Current.Name)
                : TEXT("Choose a writable single-relation field and a related record.");
        }
        return *this;
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
    if (!AcceptField(Field, Current) || !FOpenPocketBaseMultipleRelationFieldRef::Accepts(Current) ||
        InvalidRecord != nullptr)
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = InvalidRecord != nullptr
                ? FString::Printf(
                      TEXT("Record '%s' does not belong to relation field '%s'."),
                      *InvalidRecord->Id,
                      *Current.Name)
                : TEXT("Choose a writable multiple-relation field and related records.");
        }
        return *this;
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
    GetOrCreateBodyObject(*this)->SetStringField(MakeModifiedFieldName(FieldName, Modifier), Value);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetDynamicNumberField(
    const FString& FieldName,
    const double Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    GetOrCreateBodyObject(*this)->SetNumberField(MakeModifiedFieldName(FieldName, Modifier), Value);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetDynamicBooleanField(
    const FString& FieldName,
    const bool bValue,
    const EOpenPocketBaseFieldModifier Modifier)
{
    GetOrCreateBodyObject(*this)->SetBoolField(MakeModifiedFieldName(FieldName, Modifier), bValue);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetDynamicNullField(
    const FString& FieldName,
    const EOpenPocketBaseFieldModifier Modifier)
{
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
    GetOrCreateBodyObject(*this)->SetArrayField(
        MakeModifiedFieldName(FieldName, Modifier),
        StringValues(Value));
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetDynamicDateField(
    const FString& FieldName,
    const FDateTime& Value)
{
    GetOrCreateBodyObject(*this)->SetStringField(FieldName, OpenPocketBase::Date::Format(Value));
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetDynamicJsonField(
    const FString& FieldName,
    const FOpenPocketBaseJsonValue& Value)
{
    const TSharedPtr<FJsonValue> JsonValue = Value.ToJsonValue();
    if (!JsonValue.IsValid())
    {
        bValid = false;
        ErrorMessage = Value.ErrorMessage.IsEmpty()
            ? TEXT("A valid JSON value is required.")
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
    if (!Value.IsValid())
    {
        bValid = false;
        ErrorMessage = TEXT("Valid latitude and longitude values are required.");
        return *this;
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
