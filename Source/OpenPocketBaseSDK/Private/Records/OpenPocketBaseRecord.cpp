#include "OpenPocketBaseRecord.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

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

bool FOpenPocketBaseRecordBody::AcceptField(const FOpenPocketBaseFieldRef& Field)
{
    if (!bValid)
    {
        return false;
    }
    if (!Field.IsSet())
    {
        bValid = false;
        ErrorMessage = TEXT("Choose a valid collection field for the record body.");
        return false;
    }
    if (Field.bReadOnly)
    {
        bValid = false;
        ErrorMessage = FString::Printf(TEXT("Field '%s' is read-only."), *Field.Name);
        return false;
    }
    if (!SchemaId.IsValid())
    {
        SchemaId = Field.SchemaId;
        CollectionId = Field.CollectionId;
        return true;
    }
    if (SchemaId != Field.SchemaId || CollectionId != Field.CollectionId)
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
    const FOpenPocketBaseStringFieldRef& Field,
    const FString& Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    if (!FOpenPocketBaseStringFieldRef::Accepts(Field) || !AcceptField(Field))
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = TEXT("Choose a writable string field for the record body.");
        }
        return *this;
    }
    GetOrCreateBodyObject(*this)->SetStringField(MakeModifiedFieldName(Field.Name, Modifier), Value);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetNumberField(
    const FOpenPocketBaseNumberFieldRef& Field,
    const double Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    if (!FOpenPocketBaseNumberFieldRef::Accepts(Field) || !AcceptField(Field))
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = TEXT("Choose a writable number field for the record body.");
        }
        return *this;
    }
    GetOrCreateBodyObject(*this)->SetNumberField(MakeModifiedFieldName(Field.Name, Modifier), Value);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetBooleanField(
    const FOpenPocketBaseBooleanFieldRef& Field,
    const bool bValue,
    const EOpenPocketBaseFieldModifier Modifier)
{
    if (!FOpenPocketBaseBooleanFieldRef::Accepts(Field) || !AcceptField(Field))
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = TEXT("Choose a writable boolean field for the record body.");
        }
        return *this;
    }
    GetOrCreateBodyObject(*this)->SetBoolField(MakeModifiedFieldName(Field.Name, Modifier), bValue);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetNullField(
    const FOpenPocketBaseAnyFieldRef& Field,
    const EOpenPocketBaseFieldModifier Modifier)
{
    if (!FOpenPocketBaseAnyFieldRef::Accepts(Field) || !AcceptField(Field))
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = TEXT("Choose a writable field for the record body.");
        }
        return *this;
    }
    GetOrCreateBodyObject(*this)->SetField(
        MakeModifiedFieldName(Field.Name, Modifier),
        MakeShared<FJsonValueNull>());
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetStringArrayField(
    const FOpenPocketBaseStringArrayFieldRef& Field,
    const TArray<FString>& Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    if (!FOpenPocketBaseStringArrayFieldRef::Accepts(Field) || !AcceptField(Field))
    {
        if (bValid)
        {
            bValid = false;
            ErrorMessage = TEXT("Choose a writable string-array field for the record body.");
        }
        return *this;
    }
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    JsonValues.Reserve(Value.Num());
    for (const FString& Item : Value)
    {
        JsonValues.Add(MakeShared<FJsonValueString>(Item));
    }
    GetOrCreateBodyObject(*this)->SetArrayField(
        MakeModifiedFieldName(Field.Name, Modifier),
        MoveTemp(JsonValues));
    return *this;
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
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    JsonValues.Reserve(Value.Num());
    for (const FString& Item : Value)
    {
        JsonValues.Add(MakeShared<FJsonValueString>(Item));
    }
    GetOrCreateBodyObject(*this)->SetArrayField(
        MakeModifiedFieldName(FieldName, Modifier),
        MoveTemp(JsonValues));
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
