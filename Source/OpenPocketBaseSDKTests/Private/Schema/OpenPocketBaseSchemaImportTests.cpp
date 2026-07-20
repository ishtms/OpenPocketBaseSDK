#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "EditorReimportHandler.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "OpenPocketBaseSchema.h"
#include "OpenPocketBaseSchemaImporter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSchemaImportTest,
    "OpenPocketBase.Schema.ImportsPocketBaseJson",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSchemaImportTest::RunTest(const FString& Parameters)
{
    const FString Json = TEXT(R"json(
        {
            "pocketBaseVersion": "0.39.11",
            "items": [
                {
                    "id": "tasks_id",
                    "name": "sdk_tasks",
                    "type": "base",
                    "system": false,
                    "fields": [
                        { "id": "title_id", "name": "title", "type": "text", "required": true },
                        { "id": "done_id", "name": "done", "type": "bool" },
                        { "id": "files_id", "name": "attachments", "type": "file", "maxSelect": 5 },
                        {
                            "id": "owner_id",
                            "name": "owner",
                            "type": "relation",
                            "collectionId": "users_id",
                            "maxSelect": 1
                        },
                        { "id": "created_id", "name": "created", "type": "autodate", "system": true }
                    ]
                },
                {
                    "id": "users_id",
                    "name": "sdk_users",
                    "type": "auth",
                    "system": false,
                    "fields": [
                        { "id": "email_id", "name": "email", "type": "email" },
                        { "id": "password_id", "name": "password", "type": "password", "hidden": true }
                    ]
                }
            ]
        }
    )json");

    UOpenPocketBaseSchema* Schema = NewObject<UOpenPocketBaseSchema>();
    const FGuid ExistingSchemaId(1, 2, 3, 4);
    Schema->SchemaId = ExistingSchemaId;
    FText Error;

    TestTrue(
        TEXT("PocketBase collection responses import"),
        FOpenPocketBaseSchemaImporter::ImportJson(
            Json,
            TEXT("C:/schemas/pb_schema.json"),
            *Schema,
            Error));
    TestTrue(TEXT("Import errors stay empty"), Error.IsEmpty());
    TestEqual(TEXT("Reimport preserves the schema identity"), Schema->SchemaId, ExistingSchemaId);
    TestEqual(TEXT("The PocketBase version is captured"), Schema->PocketBaseVersion, FString(TEXT("0.39.11")));
    TestEqual(TEXT("The source is captured"), Schema->Source, FString(TEXT("C:/schemas/pb_schema.json")));
    TestFalse(TEXT("A deterministic fingerprint is created"), Schema->Fingerprint.IsEmpty());
    TestEqual(TEXT("All collections import"), Schema->Collections.Num(), 2);

    const FOpenPocketBaseSchemaCollection* Tasks = Schema->FindCollection(TEXT("tasks_id"));
    TestNotNull(TEXT("Collections resolve by their stable ID"), Tasks);
    if (Tasks == nullptr)
    {
        return false;
    }

    TestEqual(TEXT("Base collection type imports"), Tasks->Type, EOpenPocketBaseCollectionType::Base);
    const FOpenPocketBaseSchemaField* Attachments = Tasks->FindField(TEXT("attachments"));
    TestNotNull(TEXT("File fields import"), Attachments);
    if (Attachments != nullptr)
    {
        TestEqual(TEXT("File type imports"), Attachments->Type, EOpenPocketBaseFieldType::File);
        TestTrue(TEXT("Multi-file fields retain multiplicity"), Attachments->bMultiple);
    }

    const FOpenPocketBaseSchemaField* Owner = Tasks->FindField(TEXT("owner"));
    TestNotNull(TEXT("Relation fields import"), Owner);
    if (Owner != nullptr)
    {
        TestEqual(TEXT("Relation target imports"), Owner->RelatedCollectionId, FString(TEXT("users_id")));
        TestFalse(TEXT("Single relation fields stay scalar"), Owner->bMultiple);
    }

    const FOpenPocketBaseSchemaField* Created = Tasks->FindField(TEXT("created"));
    TestNotNull(TEXT("Autodate fields import"), Created);
    if (Created != nullptr)
    {
        TestTrue(TEXT("Generated date fields are read only"), Created->bReadOnly);
    }

    const FOpenPocketBaseSchemaField* CollectionName = Tasks->FindField(TEXT("collectionName"));
    TestNotNull(TEXT("Response metadata fields are available to field pickers"), CollectionName);
    if (CollectionName != nullptr)
    {
        TestTrue(TEXT("Response metadata is read only"), CollectionName->bReadOnly);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSchemaImportAtomicityTest,
    "OpenPocketBase.Schema.RejectsInvalidJsonWithoutChangingAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSchemaImportAtomicityTest::RunTest(const FString& Parameters)
{
    UOpenPocketBaseSchema* Schema = NewObject<UOpenPocketBaseSchema>();
    Schema->SchemaId = FGuid(5, 6, 7, 8);
    FOpenPocketBaseSchemaCollection Existing;
    Existing.Id = TEXT("existing_id");
    Existing.Name = TEXT("existing");
    Schema->Collections.Add(Existing);

    FText Error;
    TestFalse(
        TEXT("Collections without stable IDs are rejected"),
        FOpenPocketBaseSchemaImporter::ImportJson(
            TEXT(R"json([{ "name": "broken", "type": "base", "fields": [] }])json"),
            TEXT("broken.json"),
            *Schema,
            Error));
    TestFalse(TEXT("A useful import error is returned"), Error.IsEmpty());
    TestEqual(TEXT("Failed imports leave collections unchanged"), Schema->Collections.Num(), 1);
    TestEqual(TEXT("Failed imports preserve existing data"), Schema->Collections[0].Id, FString(TEXT("existing_id")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSchemaConstraintFingerprintTest,
    "OpenPocketBase.Schema.FingerprintIncludesFieldConstraints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSchemaConstraintFingerprintTest::RunTest(const FString& Parameters)
{
    UOpenPocketBaseSchema* Schema = NewObject<UOpenPocketBaseSchema>();
    FText Error;
    const FString FirstJson = TEXT(R"json([{
        "id": "tasks_id",
        "name": "sdk_tasks",
        "type": "base",
        "fields": [{
            "id": "status_id",
            "name": "status",
            "type": "select",
            "values": ["active"],
            "maxSelect": 1
        }]
    }])json");
    const FString SecondJson = TEXT(R"json([{
        "id": "tasks_id",
        "name": "sdk_tasks",
        "type": "base",
        "fields": [{
            "id": "status_id",
            "name": "status",
            "type": "select",
            "values": ["active", "paused"],
            "maxSelect": 1
        }]
    }])json");

    TestTrue(
        TEXT("The first constrained schema imports"),
        FOpenPocketBaseSchemaImporter::ImportJson(
            FirstJson,
            TEXT("schema.json"),
            *Schema,
            Error));
    const FString FirstFingerprint = Schema->Fingerprint;
    TestTrue(
        TEXT("The changed constrained schema imports"),
        FOpenPocketBaseSchemaImporter::ImportJson(
            SecondJson,
            TEXT("schema.json"),
            *Schema,
            Error));
    TestNotEqual(
        TEXT("Changing only select choices changes the schema fingerprint"),
        Schema->Fingerprint,
        FirstFingerprint);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSchemaFactoryReimportTest,
    "OpenPocketBase.Schema.ReimportsThroughUnreal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSchemaFactoryReimportTest::RunTest(const FString& Parameters)
{
    const FString SourcePath = FPaths::CreateTempFilename(
        *FPaths::ProjectIntermediateDir(), TEXT("OpenPocketBaseSchema"), TEXT(".json"));
    const FString Json = TEXT(R"json([
        {
            "id": "tasks_id",
            "name": "renamed_tasks",
            "type": "base",
            "fields": [
                { "id": "title_id", "name": "renamed_title", "type": "text" }
            ]
        }
    ])json");
    if (!TestTrue(TEXT("The reimport fixture is written"), FFileHelper::SaveStringToFile(Json, *SourcePath)))
    {
        return false;
    }

    UOpenPocketBaseSchema* Schema = NewObject<UOpenPocketBaseSchema>();
    Schema->SchemaId = FGuid(13, 21, 34, 55);
    Schema->Source = SourcePath;
    FOpenPocketBaseSchemaCollection Existing;
    Existing.Id = TEXT("tasks_id");
    Existing.Name = TEXT("sdk_tasks");
    Schema->Collections.Add(Existing);

    UOpenPocketBaseSchemaFactory* Factory = NewObject<UOpenPocketBaseSchemaFactory>();
    TArray<FString> Sources;
    TestTrue(TEXT("The Unreal reimport handler accepts schema assets"), Factory->CanReimport(Schema, Sources));
    TestEqual(TEXT("The source file is reported"), Sources.Num(), 1);
    TestEqual(TEXT("The reimport succeeds"), Factory->Reimport(Schema), EReimportResult::Succeeded);
    TestEqual(TEXT("Reimport preserves the schema ID"), Schema->SchemaId, FGuid(13, 21, 34, 55));
    TestEqual(TEXT("Reimport updates the collection"), Schema->Collections[0].Name, FString(TEXT("renamed_tasks")));
    TestEqual(TEXT("Reimport updates the fields"), Schema->Collections[0].Fields[0].Name, FString(TEXT("renamed_title")));

    IFileManager::Get().Delete(*SourcePath);
    return true;
}

#endif
