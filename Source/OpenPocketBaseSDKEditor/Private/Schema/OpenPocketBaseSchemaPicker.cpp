// Copyright 2026 Ishtmeet Singh.

#include "OpenPocketBaseSchemaPicker.h"

#include "Misc/OutputDeviceNull.h"
#include "OpenPocketBaseProjectSettings.h"
#include "UObject/StructOnScope.h"

#define LOCTEXT_NAMESPACE "OpenPocketBaseSchemaPicker"

void FOpenPocketBaseSchemaPickerModel::ChooseProfileSchemas(
    const UOpenPocketBaseProjectSettings& Settings,
    const TArray<UOpenPocketBaseSchema*>& AvailableSchemas,
    TArray<UOpenPocketBaseSchema*>& OutSchemas)
{
    OutSchemas.Reset();
    const FOpenPocketBaseProjectProfile* Profile = Settings.Profiles.FindByPredicate(
        [&Settings](const FOpenPocketBaseProjectProfile& Candidate)
        {
            return Candidate.Name == Settings.DefaultProfile;
        });
    if (Profile != nullptr && !Profile->Schema.IsNull())
    {
        UOpenPocketBaseSchema* ProfileSchema = Profile->Schema.LoadSynchronous();
        if (ProfileSchema != nullptr)
        {
            OutSchemas.Add(ProfileSchema);
            return;
        }
    }

    for (UOpenPocketBaseSchema* Schema : AvailableSchemas)
    {
        if (Schema != nullptr)
        {
            OutSchemas.AddUnique(Schema);
        }
    }
}

namespace
{
FText CollectionTypeText(const EOpenPocketBaseCollectionType Type)
{
    switch (Type)
    {
    case EOpenPocketBaseCollectionType::Base:
        return LOCTEXT("BaseCollection", "Base collection");
    case EOpenPocketBaseCollectionType::Auth:
        return LOCTEXT("AuthCollection", "Auth collection");
    case EOpenPocketBaseCollectionType::View:
        return LOCTEXT("ViewCollection", "View collection");
    default:
        return LOCTEXT("UnknownCollection", "Unknown collection type");
    }
}

FText FieldTypeText(const FOpenPocketBaseFieldRef& Field)
{
    FText Type;
    switch (Field.Type)
    {
    case EOpenPocketBaseFieldType::Text:
        Type = LOCTEXT("TextField", "Text");
        break;
    case EOpenPocketBaseFieldType::Number:
        Type = LOCTEXT("NumberField", "Number");
        break;
    case EOpenPocketBaseFieldType::Boolean:
        Type = LOCTEXT("BooleanField", "Boolean");
        break;
    case EOpenPocketBaseFieldType::Email:
        Type = LOCTEXT("EmailField", "Email");
        break;
    case EOpenPocketBaseFieldType::Url:
        Type = LOCTEXT("UrlField", "URL");
        break;
    case EOpenPocketBaseFieldType::Editor:
        Type = LOCTEXT("EditorField", "Editor text");
        break;
    case EOpenPocketBaseFieldType::Date:
        Type = LOCTEXT("DateField", "Date");
        break;
    case EOpenPocketBaseFieldType::Autodate:
        Type = LOCTEXT("AutodateField", "Automatic date");
        break;
    case EOpenPocketBaseFieldType::Select:
        Type = LOCTEXT("SelectField", "Select");
        break;
    case EOpenPocketBaseFieldType::File:
        Type = LOCTEXT("FileField", "File");
        break;
    case EOpenPocketBaseFieldType::Relation:
        Type = LOCTEXT("RelationField", "Relation");
        break;
    case EOpenPocketBaseFieldType::Json:
        Type = LOCTEXT("JsonField", "JSON");
        break;
    case EOpenPocketBaseFieldType::Password:
        Type = LOCTEXT("PasswordField", "Password");
        break;
    case EOpenPocketBaseFieldType::GeoPoint:
        Type = LOCTEXT("GeoPointField", "Geo point");
        break;
    default:
        Type = LOCTEXT("UnknownField", "Unknown type");
        break;
    }

    return Field.bMultiple
        ? FText::Format(LOCTEXT("MultipleFieldType", "{0} array"), Type)
        : Type;
}

UOpenPocketBaseSchema* LoadSchema(const TSoftObjectPtr<UOpenPocketBaseSchema>& Schema)
{
    return Schema.IsNull() ? nullptr : Schema.LoadSynchronous();
}

const FOpenPocketBaseFieldRef* FieldMemory(const UScriptStruct* Struct, const uint8* Memory)
{
    return FOpenPocketBaseSchemaPickerModel::SupportsFieldStruct(Struct)
        ? reinterpret_cast<const FOpenPocketBaseFieldRef*>(Memory)
        : nullptr;
}

FOpenPocketBaseFieldRef* FieldMemory(const UScriptStruct* Struct, uint8* Memory)
{
    return FOpenPocketBaseSchemaPickerModel::SupportsFieldStruct(Struct)
        ? reinterpret_cast<FOpenPocketBaseFieldRef*>(Memory)
        : nullptr;
}
}

bool FOpenPocketBaseSchemaPickerModel::SupportsCollectionStruct(const UScriptStruct* Struct)
{
    return Struct != nullptr &&
        (Struct == FOpenPocketBaseCollectionRef::StaticStruct() ||
         Struct->IsChildOf(FOpenPocketBaseCollectionRef::StaticStruct()));
}

bool FOpenPocketBaseSchemaPickerModel::SupportsFieldStruct(const UScriptStruct* Struct)
{
    return Struct != nullptr &&
        (Struct == FOpenPocketBaseFieldRef::StaticStruct() ||
         Struct->IsChildOf(FOpenPocketBaseFieldRef::StaticStruct()));
}

bool FOpenPocketBaseSchemaPickerModel::AcceptsCollection(
    const UScriptStruct* ReferenceStruct,
    const FOpenPocketBaseCollectionRef& Collection)
{
    if (ReferenceStruct == FOpenPocketBaseCollectionRef::StaticStruct())
    {
        return Collection.IsSet();
    }
    if (ReferenceStruct == FOpenPocketBaseAuthCollectionRef::StaticStruct())
    {
        return FOpenPocketBaseAuthCollectionRef::Accepts(Collection);
    }
    if (ReferenceStruct == FOpenPocketBaseWritableCollectionRef::StaticStruct())
    {
        return FOpenPocketBaseWritableCollectionRef::Accepts(Collection);
    }
    return false;
}

bool FOpenPocketBaseSchemaPickerModel::AcceptsField(
    const UScriptStruct* ReferenceStruct,
    const FOpenPocketBaseFieldRef& Field)
{
    if (ReferenceStruct == FOpenPocketBaseFieldRef::StaticStruct() ||
        ReferenceStruct == FOpenPocketBaseAnyFieldRef::StaticStruct())
    {
        return Field.IsSet();
    }
    if (ReferenceStruct == FOpenPocketBaseStringFieldRef::StaticStruct())
    {
        return FOpenPocketBaseStringFieldRef::Accepts(Field);
    }
    if (ReferenceStruct == FOpenPocketBaseTextFieldRef::StaticStruct())
    {
        return FOpenPocketBaseTextFieldRef::Accepts(Field);
    }
    if (ReferenceStruct == FOpenPocketBaseNumberFieldRef::StaticStruct())
    {
        return FOpenPocketBaseNumberFieldRef::Accepts(Field);
    }
    if (ReferenceStruct == FOpenPocketBaseBooleanFieldRef::StaticStruct())
    {
        return FOpenPocketBaseBooleanFieldRef::Accepts(Field);
    }
    if (ReferenceStruct == FOpenPocketBaseDateFieldRef::StaticStruct())
    {
        return FOpenPocketBaseDateFieldRef::Accepts(Field);
    }
    if (ReferenceStruct == FOpenPocketBaseStringArrayFieldRef::StaticStruct())
    {
        return FOpenPocketBaseStringArrayFieldRef::Accepts(Field);
    }
    if (ReferenceStruct == FOpenPocketBaseJsonFieldRef::StaticStruct())
    {
        return FOpenPocketBaseJsonFieldRef::Accepts(Field);
    }
    if (ReferenceStruct == FOpenPocketBaseGeoPointFieldRef::StaticStruct())
    {
        return FOpenPocketBaseGeoPointFieldRef::Accepts(Field);
    }
    if (ReferenceStruct == FOpenPocketBaseSingleSelectFieldRef::StaticStruct())
    {
        return FOpenPocketBaseSingleSelectFieldRef::Accepts(Field);
    }
    if (ReferenceStruct == FOpenPocketBaseMultipleSelectFieldRef::StaticStruct())
    {
        return FOpenPocketBaseMultipleSelectFieldRef::Accepts(Field);
    }
    if (ReferenceStruct == FOpenPocketBaseRelationFieldRef::StaticStruct())
    {
        return FOpenPocketBaseRelationFieldRef::Accepts(Field);
    }
    if (ReferenceStruct == FOpenPocketBaseSingleRelationFieldRef::StaticStruct())
    {
        return FOpenPocketBaseSingleRelationFieldRef::Accepts(Field);
    }
    if (ReferenceStruct == FOpenPocketBaseMultipleRelationFieldRef::StaticStruct())
    {
        return FOpenPocketBaseMultipleRelationFieldRef::Accepts(Field);
    }
    if (ReferenceStruct == FOpenPocketBaseFileFieldRef::StaticStruct())
    {
        return FOpenPocketBaseFileFieldRef::Accepts(Field);
    }
    return false;
}

void FOpenPocketBaseSchemaPickerModel::BuildCollectionChoices(
    const TArray<UOpenPocketBaseSchema*>& Schemas,
    const UScriptStruct* ReferenceStruct,
    const bool bIncludeSystemCollections,
    const EOpenPocketBaseCollectionRequirement Requirement,
    TArray<FOpenPocketBaseSchemaPickerChoice>& OutChoices)
{
    OutChoices.Reset();
    for (UOpenPocketBaseSchema* Schema : Schemas)
    {
        if (Schema == nullptr)
        {
            continue;
        }

        for (const FOpenPocketBaseSchemaCollection& Collection : Schema->Collections)
        {
            if (Collection.bSystem && !bIncludeSystemCollections)
            {
                continue;
            }

            FOpenPocketBaseSchemaPickerChoice Choice;
            if (!Schema->MakeCollectionRef(Collection.Id, Choice.Collection))
            {
                continue;
            }
            if (!AcceptsCollection(ReferenceStruct, Choice.Collection))
            {
                continue;
            }
            if ((Requirement == EOpenPocketBaseCollectionRequirement::Writable &&
                 !FOpenPocketBaseWritableCollectionRef::Accepts(Choice.Collection)) ||
                (Requirement == EOpenPocketBaseCollectionRequirement::Auth &&
                 !FOpenPocketBaseAuthCollectionRef::Accepts(Choice.Collection)))
            {
                continue;
            }
            Choice.Label = FText::FromString(Collection.Name);
            Choice.Detail = FText::Format(
                LOCTEXT("CollectionDetail", "{0} | {1}"),
                CollectionTypeText(Collection.Type),
                FText::FromString(Schema->GetName()));
            Choice.SearchText = FString::Printf(
                TEXT("%s %s %s"),
                *Collection.Name,
                *CollectionTypeText(Collection.Type).ToString(),
                *Schema->GetName());
            OutChoices.Add(MoveTemp(Choice));
        }
    }

    OutChoices.Sort(
        [](const FOpenPocketBaseSchemaPickerChoice& Left, const FOpenPocketBaseSchemaPickerChoice& Right)
        {
            return Left.Collection.Name < Right.Collection.Name;
        });
}

void FOpenPocketBaseSchemaPickerModel::BuildFieldChoices(
    const TArray<UOpenPocketBaseSchema*>& Schemas,
    const FOpenPocketBaseFieldPickerFilter& Filter,
    TArray<FOpenPocketBaseSchemaPickerChoice>& OutChoices)
{
    OutChoices.Reset();
    for (UOpenPocketBaseSchema* Schema : Schemas)
    {
        if (Schema == nullptr)
        {
            continue;
        }

        for (const FOpenPocketBaseSchemaCollection& Collection : Schema->Collections)
        {
            if (Filter.Collection != nullptr)
            {
                if (Filter.Collection->SchemaId != Schema->SchemaId ||
                    Filter.Collection->CollectionId != Collection.Id)
                {
                    continue;
                }
            }
            else if (Collection.bSystem)
            {
                continue;
            }

            FOpenPocketBaseCollectionRef CollectionRef;
            if (!Schema->MakeCollectionRef(Collection.Id, CollectionRef))
            {
                continue;
            }

            for (const FOpenPocketBaseSchemaField& SchemaField : Collection.Fields)
            {
                FOpenPocketBaseFieldRef Field;
                if (!Schema->MakeFieldRef(CollectionRef, SchemaField.Id, Field) ||
                    !AcceptsField(Filter.ReferenceStruct, Field) ||
                    (Filter.bWritableOnly && Field.bReadOnly) ||
                    (!Filter.bIncludeHidden && SchemaField.bHidden))
                {
                    continue;
                }

                FOpenPocketBaseSchemaPickerChoice Choice;
                Choice.Collection = CollectionRef;
                Choice.Field = MoveTemp(Field);
                Choice.Label = FText::FromString(SchemaField.Name);
                Choice.Detail = SchemaField.Storage == EOpenPocketBaseFieldStorage::Data
                    ? FText::Format(
                          LOCTEXT("FieldDetail", "{0} | {1}"),
                          FText::FromString(Collection.Name),
                          FieldTypeText(Choice.Field))
                    : FText::Format(
                          LOCTEXT("MetadataFieldDetail", "{0} | {1} | Record metadata"),
                          FText::FromString(Collection.Name),
                          FieldTypeText(Choice.Field));
                Choice.SearchText = FString::Printf(
                    TEXT("%s.%s %s %s"),
                    *Collection.Name,
                    *SchemaField.Name,
                    *FieldTypeText(Choice.Field).ToString(),
                    *Schema->GetName());
                OutChoices.Add(MoveTemp(Choice));
            }
        }
    }

    OutChoices.Sort(
        [](const FOpenPocketBaseSchemaPickerChoice& Left, const FOpenPocketBaseSchemaPickerChoice& Right)
        {
            if (Left.Collection.Name != Right.Collection.Name)
            {
                return Left.Collection.Name < Right.Collection.Name;
            }
            return Left.Field.Name < Right.Field.Name;
        });
}

bool FOpenPocketBaseSchemaPickerModel::ParseCollectionDefault(
    const FString& DefaultValue,
    FOpenPocketBaseCollectionRef& OutRef)
{
    OutRef = {};
    if (DefaultValue.IsEmpty())
    {
        return false;
    }

    FOutputDeviceNull Output;
    return FOpenPocketBaseCollectionRef::StaticStruct()->ImportText(
        *DefaultValue,
        &OutRef,
        nullptr,
        PPF_SerializedAsImportText,
        &Output,
        TEXT("Collection")) != nullptr;
}

bool FOpenPocketBaseSchemaPickerModel::ParseFieldDefault(
    const UScriptStruct* ReferenceStruct,
    const FString& DefaultValue,
    FOpenPocketBaseFieldRef& OutRef)
{
    OutRef = {};
    if (!SupportsFieldStruct(ReferenceStruct) || DefaultValue.IsEmpty())
    {
        return false;
    }

    FStructOnScope Value(ReferenceStruct);
    FOutputDeviceNull Output;
    if (const_cast<UScriptStruct*>(ReferenceStruct)->ImportText(
            *DefaultValue,
            Value.GetStructMemory(),
            nullptr,
            PPF_SerializedAsImportText,
            &Output,
            TEXT("Field")) == nullptr)
    {
        return false;
    }

    const FOpenPocketBaseFieldRef* Parsed = FieldMemory(
        ReferenceStruct,
        Value.GetStructMemory());
    if (Parsed == nullptr)
    {
        return false;
    }

    OutRef = *Parsed;
    return true;
}

FString FOpenPocketBaseSchemaPickerModel::ExportCollectionDefault(
    const FOpenPocketBaseCollectionRef& Ref)
{
    FString Result;
    FOpenPocketBaseCollectionRef::StaticStruct()->ExportText(
        Result,
        &Ref,
        &Ref,
        nullptr,
        PPF_SerializedAsImportText,
        nullptr);
    return Result;
}

FString FOpenPocketBaseSchemaPickerModel::ExportFieldDefault(
    const UScriptStruct* ReferenceStruct,
    const FOpenPocketBaseFieldRef& Ref)
{
    if (!SupportsFieldStruct(ReferenceStruct) || !AcceptsField(ReferenceStruct, Ref))
    {
        return FString();
    }

    FStructOnScope Value(ReferenceStruct);
    FOpenPocketBaseFieldRef* TypedMemory = FieldMemory(
        ReferenceStruct,
        Value.GetStructMemory());
    if (TypedMemory == nullptr)
    {
        return FString();
    }
    *TypedMemory = Ref;

    FString Result;
    const_cast<UScriptStruct*>(ReferenceStruct)->ExportText(
        Result,
        Value.GetStructMemory(),
        Value.GetStructMemory(),
        nullptr,
        PPF_SerializedAsImportText,
        nullptr);
    return Result;
}

EOpenPocketBaseSchemaReferenceStatus FOpenPocketBaseSchemaPickerModel::ValidateCollection(
    const UScriptStruct* ReferenceStruct,
    const FOpenPocketBaseCollectionRef& Ref,
    const EOpenPocketBaseCollectionRequirement Requirement,
    FText& OutMessage)
{
    OutMessage = FText::GetEmpty();
    if (!Ref.IsSet())
    {
        OutMessage = LOCTEXT("ChooseCollection", "Choose a PocketBase collection.");
        return EOpenPocketBaseSchemaReferenceStatus::Empty;
    }

    UOpenPocketBaseSchema* Schema = LoadSchema(Ref.Schema);
    if (Schema == nullptr)
    {
        OutMessage = LOCTEXT("MissingSchema", "The PocketBase schema asset is missing.");
        return EOpenPocketBaseSchemaReferenceStatus::MissingSchema;
    }
    if (Schema->SchemaId != Ref.SchemaId)
    {
        OutMessage = LOCTEXT("StaleSchema", "The schema asset was replaced. Choose the collection again.");
        return EOpenPocketBaseSchemaReferenceStatus::StaleSchema;
    }

    const FOpenPocketBaseSchemaCollection* Collection = nullptr;
    if (!Schema->ResolveCollection(Ref, Collection))
    {
        OutMessage = FText::Format(
            LOCTEXT("MissingCollection", "Collection '{0}' no longer exists in the imported schema."),
            FText::FromString(Ref.Name));
        return EOpenPocketBaseSchemaReferenceStatus::MissingCollection;
    }
    FOpenPocketBaseCollectionRef CurrentRef;
    if (!Schema->MakeCollectionRef(Collection->Id, CurrentRef) ||
        !AcceptsCollection(ReferenceStruct, CurrentRef))
    {
        OutMessage = FText::Format(
            LOCTEXT("WrongCollectionType", "Collection '{0}' does not have the type required by this pin."),
            FText::FromString(Collection->Name));
        return EOpenPocketBaseSchemaReferenceStatus::WrongCollectionType;
    }
    if (Requirement == EOpenPocketBaseCollectionRequirement::Writable &&
        !FOpenPocketBaseWritableCollectionRef::Accepts(CurrentRef))
    {
        OutMessage = FText::Format(
            LOCTEXT("ReadOnlyCollection", "Collection '{0}' is read-only and cannot be used by the connected operation."),
            FText::FromString(Collection->Name));
        return EOpenPocketBaseSchemaReferenceStatus::WrongCollectionType;
    }
    if (Requirement == EOpenPocketBaseCollectionRequirement::Auth &&
        !FOpenPocketBaseAuthCollectionRef::Accepts(CurrentRef))
    {
        OutMessage = FText::Format(
            LOCTEXT("NonAuthCollection", "Collection '{0}' is not an auth collection."),
            FText::FromString(Collection->Name));
        return EOpenPocketBaseSchemaReferenceStatus::WrongCollectionType;
    }
    return EOpenPocketBaseSchemaReferenceStatus::Valid;
}

EOpenPocketBaseSchemaReferenceStatus FOpenPocketBaseSchemaPickerModel::ValidateField(
    const UScriptStruct* ReferenceStruct,
    const FOpenPocketBaseFieldRef& Ref,
    const bool bWritableOnly,
    FText& OutMessage)
{
    OutMessage = FText::GetEmpty();
    if (!Ref.IsSet())
    {
        OutMessage = LOCTEXT("ChooseField", "Choose a PocketBase field.");
        return EOpenPocketBaseSchemaReferenceStatus::Empty;
    }

    UOpenPocketBaseSchema* Schema = LoadSchema(Ref.Schema);
    if (Schema == nullptr)
    {
        OutMessage = LOCTEXT("MissingFieldSchema", "The PocketBase schema asset is missing.");
        return EOpenPocketBaseSchemaReferenceStatus::MissingSchema;
    }
    if (Schema->SchemaId != Ref.SchemaId)
    {
        OutMessage = LOCTEXT("StaleFieldSchema", "The schema asset was replaced. Choose the field again.");
        return EOpenPocketBaseSchemaReferenceStatus::StaleSchema;
    }
    if (Schema->FindCollection(Ref.CollectionId) == nullptr)
    {
        OutMessage = LOCTEXT("MissingFieldCollection", "The field's collection no longer exists in the imported schema.");
        return EOpenPocketBaseSchemaReferenceStatus::MissingCollection;
    }

    const FOpenPocketBaseSchemaField* Field = nullptr;
    if (!Schema->ResolveField(Ref, Field))
    {
        OutMessage = FText::Format(
            LOCTEXT("MissingField", "Field '{0}' no longer exists in the imported schema."),
            FText::FromString(Ref.Name));
        return EOpenPocketBaseSchemaReferenceStatus::MissingField;
    }
    FOpenPocketBaseCollectionRef CurrentCollection;
    FOpenPocketBaseFieldRef CurrentRef;
    if (!Schema->MakeCollectionRef(Ref.CollectionId, CurrentCollection) ||
        !Schema->MakeFieldRef(CurrentCollection, Field->Id, CurrentRef) ||
        !AcceptsField(ReferenceStruct, CurrentRef))
    {
        OutMessage = FText::Format(
            LOCTEXT("WrongFieldType", "Field '{0}' no longer has the type required by this pin."),
            FText::FromString(Field->Name));
        return EOpenPocketBaseSchemaReferenceStatus::WrongFieldType;
    }
    if (bWritableOnly && CurrentRef.bReadOnly)
    {
        OutMessage = FText::Format(
            LOCTEXT("ReadOnlyField", "Field '{0}' is read-only and cannot be written."),
            FText::FromString(Field->Name));
        return EOpenPocketBaseSchemaReferenceStatus::ReadOnlyField;
    }
    return EOpenPocketBaseSchemaReferenceStatus::Valid;
}

#undef LOCTEXT_NAMESPACE
