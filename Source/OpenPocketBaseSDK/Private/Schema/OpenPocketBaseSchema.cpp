#include "OpenPocketBaseSchema.h"

namespace
{
EOpenPocketBaseFieldValueType GetFieldValueType(
    const EOpenPocketBaseFieldType Type,
    const bool bMultiple)
{
    if (bMultiple)
    {
        switch (Type)
        {
        case EOpenPocketBaseFieldType::Select:
        case EOpenPocketBaseFieldType::File:
        case EOpenPocketBaseFieldType::Relation:
            return EOpenPocketBaseFieldValueType::StringArray;
        default:
            break;
        }
    }

    switch (Type)
    {
    case EOpenPocketBaseFieldType::Text:
    case EOpenPocketBaseFieldType::Email:
    case EOpenPocketBaseFieldType::Url:
    case EOpenPocketBaseFieldType::Editor:
    case EOpenPocketBaseFieldType::Select:
    case EOpenPocketBaseFieldType::File:
    case EOpenPocketBaseFieldType::Relation:
    case EOpenPocketBaseFieldType::Password:
        return EOpenPocketBaseFieldValueType::String;
    case EOpenPocketBaseFieldType::Number:
        return EOpenPocketBaseFieldValueType::Number;
    case EOpenPocketBaseFieldType::Boolean:
        return EOpenPocketBaseFieldValueType::Boolean;
    case EOpenPocketBaseFieldType::Date:
    case EOpenPocketBaseFieldType::Autodate:
        return EOpenPocketBaseFieldValueType::DateTime;
    case EOpenPocketBaseFieldType::Json:
        return EOpenPocketBaseFieldValueType::Json;
    case EOpenPocketBaseFieldType::GeoPoint:
        return EOpenPocketBaseFieldValueType::GeoPoint;
    default:
        return EOpenPocketBaseFieldValueType::Unknown;
    }
}
}

EOpenPocketBaseFieldValueType FOpenPocketBaseSchemaField::GetValueType() const
{
    return GetFieldValueType(Type, bMultiple);
}

const FOpenPocketBaseSchemaField* FOpenPocketBaseSchemaCollection::FindField(
    const FString& IdOrName) const
{
    if (IdOrName.IsEmpty())
    {
        return nullptr;
    }

    const FOpenPocketBaseSchemaField* ById = Fields.FindByPredicate(
        [&IdOrName](const FOpenPocketBaseSchemaField& Field)
        {
            return Field.Id == IdOrName;
        });
    return ById != nullptr
        ? ById
        : Fields.FindByPredicate(
              [&IdOrName](const FOpenPocketBaseSchemaField& Field)
              {
                  return Field.Name == IdOrName;
              });
}

bool FOpenPocketBaseCollectionRef::IsSet() const
{
    return SchemaId.IsValid() && !CollectionId.IsEmpty() && !Name.IsEmpty();
}

bool FOpenPocketBaseFieldRef::IsSet() const
{
    return SchemaId.IsValid() && !CollectionId.IsEmpty() && !FieldId.IsEmpty() && !Name.IsEmpty();
}

EOpenPocketBaseFieldValueType FOpenPocketBaseFieldRef::GetValueType() const
{
    return GetFieldValueType(Type, bMultiple);
}

bool FOpenPocketBaseFieldRef::BelongsTo(const FOpenPocketBaseCollectionRef& Collection) const
{
    return IsSet() && Collection.IsSet() && SchemaId == Collection.SchemaId &&
        CollectionId == Collection.CollectionId;
}

bool FOpenPocketBaseAnyFieldRef::Accepts(const FOpenPocketBaseFieldRef& Field)
{
    return Field.IsSet();
}

bool FOpenPocketBaseStringFieldRef::Accepts(const FOpenPocketBaseFieldRef& Field)
{
    return Field.IsSet() && Field.GetValueType() == EOpenPocketBaseFieldValueType::String;
}

bool FOpenPocketBaseNumberFieldRef::Accepts(const FOpenPocketBaseFieldRef& Field)
{
    return Field.IsSet() && Field.GetValueType() == EOpenPocketBaseFieldValueType::Number;
}

bool FOpenPocketBaseBooleanFieldRef::Accepts(const FOpenPocketBaseFieldRef& Field)
{
    return Field.IsSet() && Field.GetValueType() == EOpenPocketBaseFieldValueType::Boolean;
}

bool FOpenPocketBaseDateFieldRef::Accepts(const FOpenPocketBaseFieldRef& Field)
{
    return Field.IsSet() && Field.GetValueType() == EOpenPocketBaseFieldValueType::DateTime;
}

bool FOpenPocketBaseStringArrayFieldRef::Accepts(const FOpenPocketBaseFieldRef& Field)
{
    return Field.IsSet() && Field.GetValueType() == EOpenPocketBaseFieldValueType::StringArray;
}

bool FOpenPocketBaseJsonFieldRef::Accepts(const FOpenPocketBaseFieldRef& Field)
{
    const EOpenPocketBaseFieldValueType ValueType = Field.GetValueType();
    return Field.IsSet() &&
        (ValueType == EOpenPocketBaseFieldValueType::Json ||
         ValueType == EOpenPocketBaseFieldValueType::GeoPoint);
}

bool FOpenPocketBaseRelationFieldRef::Accepts(const FOpenPocketBaseFieldRef& Field)
{
    return Field.IsSet() && Field.Type == EOpenPocketBaseFieldType::Relation;
}

bool FOpenPocketBaseFileFieldRef::Accepts(const FOpenPocketBaseFieldRef& Field)
{
    return Field.IsSet() && Field.Type == EOpenPocketBaseFieldType::File;
}

const FOpenPocketBaseSchemaCollection* UOpenPocketBaseSchema::FindCollection(
    const FString& IdOrName) const
{
    if (IdOrName.IsEmpty())
    {
        return nullptr;
    }

    const FOpenPocketBaseSchemaCollection* ById = Collections.FindByPredicate(
        [&IdOrName](const FOpenPocketBaseSchemaCollection& Collection)
        {
            return Collection.Id == IdOrName;
        });
    return ById != nullptr
        ? ById
        : Collections.FindByPredicate(
              [&IdOrName](const FOpenPocketBaseSchemaCollection& Collection)
              {
                  return Collection.Name == IdOrName;
              });
}

bool UOpenPocketBaseSchema::MakeCollectionRef(
    const FString& IdOrName,
    FOpenPocketBaseCollectionRef& OutRef) const
{
    OutRef = {};
    const FOpenPocketBaseSchemaCollection* Collection = FindCollection(IdOrName);
    if (!SchemaId.IsValid() || Collection == nullptr)
    {
        return false;
    }

    OutRef.Schema = TSoftObjectPtr<UOpenPocketBaseSchema>(FSoftObjectPath(this));
    OutRef.SchemaId = SchemaId;
    OutRef.CollectionId = Collection->Id;
    OutRef.Name = Collection->Name;
    OutRef.Type = Collection->Type;
    return true;
}

bool UOpenPocketBaseSchema::MakeFieldRef(
    const FOpenPocketBaseCollectionRef& Collection,
    const FString& IdOrName,
    FOpenPocketBaseFieldRef& OutRef) const
{
    OutRef = {};
    const FOpenPocketBaseSchemaCollection* ResolvedCollection = nullptr;
    if (!ResolveCollection(Collection, ResolvedCollection))
    {
        return false;
    }

    const FOpenPocketBaseSchemaField* Field = ResolvedCollection->FindField(IdOrName);
    if (Field == nullptr)
    {
        return false;
    }

    OutRef.Schema = TSoftObjectPtr<UOpenPocketBaseSchema>(FSoftObjectPath(this));
    OutRef.SchemaId = SchemaId;
    OutRef.CollectionId = ResolvedCollection->Id;
    OutRef.FieldId = Field->Id;
    OutRef.Name = Field->Name;
    OutRef.Type = Field->Type;
    OutRef.bMultiple = Field->bMultiple;
    OutRef.bReadOnly = Field->bReadOnly;
    OutRef.RelatedCollectionId = Field->RelatedCollectionId;
    return true;
}

bool UOpenPocketBaseSchema::ResolveCollection(
    const FOpenPocketBaseCollectionRef& Ref,
    const FOpenPocketBaseSchemaCollection*& OutCollection) const
{
    OutCollection = nullptr;
    if (!SchemaId.IsValid() || Ref.SchemaId != SchemaId)
    {
        return false;
    }

    OutCollection = FindCollection(Ref.CollectionId);
    return OutCollection != nullptr;
}

bool UOpenPocketBaseSchema::ResolveField(
    const FOpenPocketBaseFieldRef& Ref,
    const FOpenPocketBaseSchemaField*& OutField) const
{
    OutField = nullptr;
    if (!SchemaId.IsValid() || Ref.SchemaId != SchemaId)
    {
        return false;
    }

    const FOpenPocketBaseSchemaCollection* Collection = FindCollection(Ref.CollectionId);
    if (Collection == nullptr)
    {
        return false;
    }

    OutField = Collection->FindField(Ref.FieldId);
    return OutField != nullptr;
}
