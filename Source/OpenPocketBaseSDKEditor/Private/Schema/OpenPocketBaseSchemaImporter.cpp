#include "OpenPocketBaseSchemaImporter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/SecureHash.h"
#include "Misc/FileHelper.h"
#include "OpenPocketBaseSchema.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "OpenPocketBaseSchemaImporter"

namespace
{
EOpenPocketBaseCollectionType ParseCollectionType(const FString& Value)
{
    if (Value == TEXT("base"))
    {
        return EOpenPocketBaseCollectionType::Base;
    }
    if (Value == TEXT("auth"))
    {
        return EOpenPocketBaseCollectionType::Auth;
    }
    if (Value == TEXT("view"))
    {
        return EOpenPocketBaseCollectionType::View;
    }
    return EOpenPocketBaseCollectionType::Unknown;
}

EOpenPocketBaseFieldType ParseFieldType(const FString& Value)
{
    if (Value == TEXT("text"))
    {
        return EOpenPocketBaseFieldType::Text;
    }
    if (Value == TEXT("number"))
    {
        return EOpenPocketBaseFieldType::Number;
    }
    if (Value == TEXT("bool"))
    {
        return EOpenPocketBaseFieldType::Boolean;
    }
    if (Value == TEXT("email"))
    {
        return EOpenPocketBaseFieldType::Email;
    }
    if (Value == TEXT("url"))
    {
        return EOpenPocketBaseFieldType::Url;
    }
    if (Value == TEXT("editor"))
    {
        return EOpenPocketBaseFieldType::Editor;
    }
    if (Value == TEXT("date"))
    {
        return EOpenPocketBaseFieldType::Date;
    }
    if (Value == TEXT("autodate"))
    {
        return EOpenPocketBaseFieldType::Autodate;
    }
    if (Value == TEXT("select"))
    {
        return EOpenPocketBaseFieldType::Select;
    }
    if (Value == TEXT("file"))
    {
        return EOpenPocketBaseFieldType::File;
    }
    if (Value == TEXT("relation"))
    {
        return EOpenPocketBaseFieldType::Relation;
    }
    if (Value == TEXT("json"))
    {
        return EOpenPocketBaseFieldType::Json;
    }
    if (Value == TEXT("password"))
    {
        return EOpenPocketBaseFieldType::Password;
    }
    if (Value == TEXT("geoPoint"))
    {
        return EOpenPocketBaseFieldType::GeoPoint;
    }
    return EOpenPocketBaseFieldType::Unknown;
}

bool ReadRequiredString(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Property,
    FString& OutValue,
    const FString& Context,
    FText& OutError)
{
    if (Object.IsValid() &&
        Object->TryGetStringField(Property, OutValue) &&
        !OutValue.IsEmpty())
    {
        return true;
    }

    OutError = FText::Format(
        LOCTEXT("MissingRequiredProperty", "{0} is missing a non-empty '{1}' value."),
        FText::FromString(Context),
        FText::FromString(Property));
    return false;
}

void AddResponseMetadataField(
    FOpenPocketBaseSchemaCollection& Collection,
    const TCHAR* Id,
    const TCHAR* Name)
{
    if (Collection.FindField(Name) != nullptr)
    {
        return;
    }

    FOpenPocketBaseSchemaField Field;
    Field.Id = Id;
    Field.Name = Name;
    Field.Type = EOpenPocketBaseFieldType::Text;
    Field.bSystem = true;
    Field.bReadOnly = true;
    Field.Storage = FCString::Strcmp(Name, TEXT("collectionId")) == 0
        ? EOpenPocketBaseFieldStorage::CollectionId
        : EOpenPocketBaseFieldStorage::CollectionName;
    Collection.Fields.Add(MoveTemp(Field));
}

void MarkRecordMetadata(FOpenPocketBaseSchemaField& Field)
{
    if (Field.Name == TEXT("id"))
    {
        Field.Storage = EOpenPocketBaseFieldStorage::RecordId;
    }
    else if (Field.Name == TEXT("created"))
    {
        Field.Storage = EOpenPocketBaseFieldStorage::Created;
    }
    else if (Field.Name == TEXT("updated"))
    {
        Field.Storage = EOpenPocketBaseFieldStorage::Updated;
    }
}

bool ParseField(
    const TSharedPtr<FJsonObject>& Object,
    const FString& CollectionName,
    FOpenPocketBaseSchemaField& OutField,
    FText& OutError)
{
    const FString Context = FString::Printf(TEXT("Collection '%s' field"), *CollectionName);
    FString WireType;
    if (!ReadRequiredString(Object, TEXT("id"), OutField.Id, Context, OutError) ||
        !ReadRequiredString(Object, TEXT("name"), OutField.Name, Context, OutError) ||
        !ReadRequiredString(Object, TEXT("type"), WireType, Context, OutError))
    {
        return false;
    }

    OutField.Type = ParseFieldType(WireType);
    Object->TryGetBoolField(TEXT("required"), OutField.bRequired);
    Object->TryGetBoolField(TEXT("system"), OutField.bSystem);
    Object->TryGetBoolField(TEXT("hidden"), OutField.bHidden);
    Object->TryGetStringField(TEXT("collectionId"), OutField.RelatedCollectionId);

    OutField.bHasMin = Object->TryGetNumberField(TEXT("min"), OutField.Min);
    OutField.bHasMax = Object->TryGetNumberField(TEXT("max"), OutField.Max);
    Object->TryGetStringField(TEXT("pattern"), OutField.Pattern);

    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (Object->TryGetArrayField(TEXT("values"), Values) && Values != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString Choice;
            if (Value.IsValid() && Value->TryGetString(Choice))
            {
                OutField.Choices.Add(MoveTemp(Choice));
            }
        }
    }
    const TArray<TSharedPtr<FJsonValue>>* MimeTypes = nullptr;
    if (Object->TryGetArrayField(TEXT("mimeTypes"), MimeTypes) && MimeTypes != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *MimeTypes)
        {
            FString MimeType;
            if (Value.IsValid() && Value->TryGetString(MimeType))
            {
                OutField.MimeTypes.Add(MoveTemp(MimeType));
            }
        }
    }

    double MaxSize = 0.0;
    if (Object->TryGetNumberField(TEXT("maxSize"), MaxSize))
    {
        OutField.MaxSizeBytes = static_cast<int64>(MaxSize);
    }
    double MinSelect = 0.0;
    Object->TryGetNumberField(TEXT("minSelect"), MinSelect);
    OutField.MinSelect = static_cast<int32>(MinSelect);

    double MaxSelect = 1.0;
    Object->TryGetNumberField(TEXT("maxSelect"), MaxSelect);
    OutField.MaxSelect = static_cast<int32>(MaxSelect);
    OutField.bMultiple =
        (OutField.Type == EOpenPocketBaseFieldType::Select ||
         OutField.Type == EOpenPocketBaseFieldType::File ||
         OutField.Type == EOpenPocketBaseFieldType::Relation) &&
        MaxSelect > 1.0;
    OutField.bReadOnly = OutField.Type == EOpenPocketBaseFieldType::Autodate;
    return true;
}

bool ParseCollection(
    const TSharedPtr<FJsonObject>& Object,
    FOpenPocketBaseSchemaCollection& OutCollection,
    FText& OutError)
{
    FString CollectionType;
    if (!ReadRequiredString(Object, TEXT("id"), OutCollection.Id, TEXT("Collection"), OutError) ||
        !ReadRequiredString(Object, TEXT("name"), OutCollection.Name, TEXT("Collection"), OutError) ||
        !ReadRequiredString(Object, TEXT("type"), CollectionType, TEXT("Collection"), OutError))
    {
        return false;
    }

    OutCollection.Type = ParseCollectionType(CollectionType);
    Object->TryGetBoolField(TEXT("system"), OutCollection.bSystem);

    const TArray<TSharedPtr<FJsonValue>>* Fields = nullptr;
    if (!Object->TryGetArrayField(TEXT("fields"), Fields) || Fields == nullptr)
    {
        OutError = FText::Format(
            LOCTEXT("MissingFields", "Collection '{0}' is missing its fields array."),
            FText::FromString(OutCollection.Name));
        return false;
    }

    TSet<FString> FieldIds;
    TSet<FString> FieldNames;
    for (const TSharedPtr<FJsonValue>& FieldValue : *Fields)
    {
        const TSharedPtr<FJsonObject>* FieldObject = nullptr;
        if (!FieldValue.IsValid() || !FieldValue->TryGetObject(FieldObject) || FieldObject == nullptr)
        {
            OutError = FText::Format(
                LOCTEXT("InvalidField", "Collection '{0}' contains a field that is not an object."),
                FText::FromString(OutCollection.Name));
            return false;
        }

        FOpenPocketBaseSchemaField Field;
        if (!ParseField(*FieldObject, OutCollection.Name, Field, OutError))
        {
            return false;
        }
        MarkRecordMetadata(Field);
        if (FieldIds.Contains(Field.Id) || FieldNames.Contains(Field.Name))
        {
            OutError = FText::Format(
                LOCTEXT("DuplicateField", "Collection '{0}' contains duplicate field ID or name '{1}'."),
                FText::FromString(OutCollection.Name),
                FText::FromString(Field.Name));
            return false;
        }

        FieldIds.Add(Field.Id);
        FieldNames.Add(Field.Name);
        OutCollection.Fields.Add(MoveTemp(Field));
    }

    AddResponseMetadataField(OutCollection, TEXT("@collectionId"), TEXT("collectionId"));
    AddResponseMetadataField(OutCollection, TEXT("@collectionName"), TEXT("collectionName"));
    return true;
}

FString BuildFingerprint(const FString& Version, const TArray<FOpenPocketBaseSchemaCollection>& Collections)
{
    TArray<const FOpenPocketBaseSchemaCollection*> SortedCollections;
    SortedCollections.Reserve(Collections.Num());
    for (const FOpenPocketBaseSchemaCollection& Collection : Collections)
    {
        SortedCollections.Add(&Collection);
    }
    SortedCollections.Sort(
        [](const FOpenPocketBaseSchemaCollection& Left, const FOpenPocketBaseSchemaCollection& Right)
        {
            return Left.Id < Right.Id;
        });

    FString Normalized = Version;
    for (const FOpenPocketBaseSchemaCollection* Collection : SortedCollections)
    {
        Normalized += FString::Printf(
            TEXT("|c:%s:%s:%d:%d"),
            *Collection->Id,
            *Collection->Name,
            static_cast<int32>(Collection->Type),
            Collection->bSystem ? 1 : 0);

        TArray<const FOpenPocketBaseSchemaField*> SortedFields;
        SortedFields.Reserve(Collection->Fields.Num());
        for (const FOpenPocketBaseSchemaField& Field : Collection->Fields)
        {
            SortedFields.Add(&Field);
        }
        SortedFields.Sort(
            [](const FOpenPocketBaseSchemaField& Left, const FOpenPocketBaseSchemaField& Right)
            {
                return Left.Id < Right.Id;
            });

        for (const FOpenPocketBaseSchemaField* Field : SortedFields)
        {
            Normalized += FString::Printf(
                TEXT("|f:%s:%s:%d:%d:%d:%d:%d:%d:%d:%s:%d:%.17g:%d:%.17g:%d:%d:%lld"),
                *Field->Id,
                *Field->Name,
                static_cast<int32>(Field->Type),
                Field->bMultiple ? 1 : 0,
                Field->bRequired ? 1 : 0,
                Field->bSystem ? 1 : 0,
                Field->bHidden ? 1 : 0,
                Field->bReadOnly ? 1 : 0,
                static_cast<int32>(Field->Storage),
                *Field->RelatedCollectionId,
                Field->bHasMin ? 1 : 0,
                Field->Min,
                Field->bHasMax ? 1 : 0,
                Field->Max,
                Field->MinSelect,
                Field->MaxSelect,
                Field->MaxSizeBytes);
            Normalized += FString::Printf(
                TEXT(":p:%d:%s:c:%d"),
                Field->Pattern.Len(),
                *Field->Pattern,
                Field->Choices.Num());
            for (const FString& Choice : Field->Choices)
            {
                Normalized += FString::Printf(TEXT(":%d:%s"), Choice.Len(), *Choice);
            }
            Normalized += FString::Printf(TEXT(":m:%d"), Field->MimeTypes.Num());
            for (const FString& MimeType : Field->MimeTypes)
            {
                Normalized += FString::Printf(TEXT(":%d:%s"), MimeType.Len(), *MimeType);
            }
        }
    }

    FTCHARToUTF8 Utf8(*Normalized);
    return FSHA1::HashBuffer(Utf8.Get(), Utf8.Length()).ToString();
}

bool ReadCollectionsRoot(
    const TSharedPtr<FJsonValue>& Root,
    TArray<TSharedPtr<FJsonValue>>& OutCollections,
    FString& OutVersion,
    FText& OutError)
{
    if (!Root.IsValid())
    {
        OutError = LOCTEXT("EmptyJson", "The schema JSON is empty.");
        return false;
    }

    if (Root->Type == EJson::Array)
    {
        OutCollections = Root->AsArray();
        return true;
    }

    if (Root->Type != EJson::Object)
    {
        OutError = LOCTEXT("InvalidRoot", "PocketBase schema JSON must be an array or object.");
        return false;
    }

    const TSharedPtr<FJsonObject> Object = Root->AsObject();
    Object->TryGetStringField(TEXT("pocketBaseVersion"), OutVersion);
    const TArray<TSharedPtr<FJsonValue>>* Collections = nullptr;
    if ((!Object->TryGetArrayField(TEXT("items"), Collections) || Collections == nullptr) &&
        (!Object->TryGetArrayField(TEXT("collections"), Collections) || Collections == nullptr))
    {
        OutError = LOCTEXT(
            "MissingCollections",
            "PocketBase schema JSON must contain an 'items' or 'collections' array.");
        return false;
    }

    OutCollections = *Collections;
    return true;
}
}

bool FOpenPocketBaseSchemaImporter::ImportJson(
    const FString& Json,
    const FString& Source,
    UOpenPocketBaseSchema& OutSchema,
    FText& OutError)
{
    OutError = FText::GetEmpty();
    TSharedPtr<FJsonValue> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root))
    {
        OutError = FText::Format(
            LOCTEXT("JsonParseFailed", "PocketBase schema JSON could not be parsed: {0}"),
            FText::FromString(Reader->GetErrorMessage()));
        return false;
    }

    FString Version;
    TArray<TSharedPtr<FJsonValue>> CollectionValues;
    if (!ReadCollectionsRoot(Root, CollectionValues, Version, OutError))
    {
        return false;
    }

    TArray<FOpenPocketBaseSchemaCollection> Collections;
    Collections.Reserve(CollectionValues.Num());
    TSet<FString> CollectionIds;
    TSet<FString> CollectionNames;
    for (const TSharedPtr<FJsonValue>& CollectionValue : CollectionValues)
    {
        const TSharedPtr<FJsonObject>* CollectionObject = nullptr;
        if (!CollectionValue.IsValid() ||
            !CollectionValue->TryGetObject(CollectionObject) ||
            CollectionObject == nullptr)
        {
            OutError = LOCTEXT("InvalidCollection", "The schema contains a collection that is not an object.");
            return false;
        }

        FOpenPocketBaseSchemaCollection Collection;
        if (!ParseCollection(*CollectionObject, Collection, OutError))
        {
            return false;
        }
        if (CollectionIds.Contains(Collection.Id) || CollectionNames.Contains(Collection.Name))
        {
            OutError = FText::Format(
                LOCTEXT("DuplicateCollection", "The schema contains duplicate collection ID or name '{0}'."),
                FText::FromString(Collection.Name));
            return false;
        }

        CollectionIds.Add(Collection.Id);
        CollectionNames.Add(Collection.Name);
        Collections.Add(MoveTemp(Collection));
    }

    const FString Fingerprint = BuildFingerprint(Version, Collections);
    if (Fingerprint.IsEmpty())
    {
        OutError = LOCTEXT("FingerprintFailed", "The imported schema fingerprint could not be created.");
        return false;
    }

    OutSchema.Modify();
    if (!OutSchema.SchemaId.IsValid())
    {
        OutSchema.SchemaId = FGuid::NewGuid();
    }
    OutSchema.PocketBaseVersion = MoveTemp(Version);
    OutSchema.Source = Source;
    OutSchema.Fingerprint = Fingerprint;
    OutSchema.Collections = MoveTemp(Collections);
    return true;
}

UOpenPocketBaseSchemaFactory::UOpenPocketBaseSchemaFactory()
{
    SupportedClass = UOpenPocketBaseSchema::StaticClass();
    Formats.Add(TEXT("json;PocketBase schema"));
    bCreateNew = false;
    bEditAfterNew = true;
    bEditorImport = true;
    bText = true;
    // Valid PocketBase JSON has to win before Unreal opens its generic DataTable options dialog.
    ImportPriority = DefaultImportPriority + 10;
}

bool UOpenPocketBaseSchemaFactory::FactoryCanImport(const FString& Filename)
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *Filename))
    {
        return false;
    }

    UOpenPocketBaseSchema* Probe = NewObject<UOpenPocketBaseSchema>(GetTransientPackage());
    FText Error;
    return FOpenPocketBaseSchemaImporter::ImportJson(Json, Filename, *Probe, Error);
}

FText UOpenPocketBaseSchemaFactory::GetDisplayName() const
{
    return LOCTEXT("SchemaFactoryDisplayName", "PocketBase Schema");
}

bool UOpenPocketBaseSchemaFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
    const UOpenPocketBaseSchema* Schema = Cast<UOpenPocketBaseSchema>(Obj);
    if (Schema == nullptr || Schema->Source.IsEmpty())
    {
        return false;
    }

    OutFilenames.Add(Schema->Source);
    return true;
}

void UOpenPocketBaseSchemaFactory::SetReimportPaths(
    UObject* Obj,
    const TArray<FString>& NewReimportPaths)
{
    UOpenPocketBaseSchema* Schema = Cast<UOpenPocketBaseSchema>(Obj);
    if (Schema == nullptr || NewReimportPaths.IsEmpty())
    {
        return;
    }

    Schema->Modify();
    Schema->Source = NewReimportPaths[0];
    Schema->MarkPackageDirty();
}

EReimportResult::Type UOpenPocketBaseSchemaFactory::Reimport(UObject* Obj)
{
    UOpenPocketBaseSchema* Schema = Cast<UOpenPocketBaseSchema>(Obj);
    if (Schema == nullptr || Schema->Source.IsEmpty())
    {
        return EReimportResult::Failed;
    }

    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *Schema->Source))
    {
        return EReimportResult::Failed;
    }

    FText Error;
    if (!FOpenPocketBaseSchemaImporter::ImportJson(Json, Schema->Source, *Schema, Error))
    {
        return EReimportResult::Failed;
    }

    Schema->PostEditChange();
    Schema->MarkPackageDirty();
    return EReimportResult::Succeeded;
}

int32 UOpenPocketBaseSchemaFactory::GetPriority() const
{
    return ImportPriority;
}

UObject* UOpenPocketBaseSchemaFactory::FactoryCreateText(
    UClass* InClass,
    UObject* InParent,
    FName InName,
    EObjectFlags Flags,
    UObject* Context,
    const TCHAR* Type,
    const TCHAR*& Buffer,
    const TCHAR* BufferEnd,
    FFeedbackContext* Warn)
{
    UOpenPocketBaseSchema* Schema = NewObject<UOpenPocketBaseSchema>(InParent, InClass, InName, Flags);
    const FString Json(BufferEnd - Buffer, Buffer);
    FText Error;
    if (!FOpenPocketBaseSchemaImporter::ImportJson(Json, GetCurrentFilename(), *Schema, Error))
    {
        if (Warn != nullptr)
        {
            Warn->Logf(ELogVerbosity::Error, TEXT("%s"), *Error.ToString());
        }
        return nullptr;
    }

    return Schema;
}

#undef LOCTEXT_NAMESPACE
