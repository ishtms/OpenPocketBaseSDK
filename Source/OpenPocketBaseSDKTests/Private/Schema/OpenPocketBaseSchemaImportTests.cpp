#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "EditorReimportHandler.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "OpenPocketBaseSchema.h"
#include "OpenPocketBaseSchemaDiagnostics.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSchemaDiagnosticsTest,
    "OpenPocketBase.Schema.DetailsSummarizeDiffAndValidate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSchemaDiagnosticsTest::RunTest(const FString& Parameters)
{
    UOpenPocketBaseSchema* Current = NewObject<UOpenPocketBaseSchema>();
    Current->SchemaId = FGuid(21, 34, 55, 89);
    Current->Fingerprint = TEXT("current");

    FOpenPocketBaseSchemaCollection Tasks;
    Tasks.Id = TEXT("tasks_id");
    Tasks.Name = TEXT("sdk_tasks");
    Tasks.Type = EOpenPocketBaseCollectionType::Base;
    FOpenPocketBaseSchemaField Title;
    Title.Id = TEXT("title_id");
    Title.Name = TEXT("title");
    Title.Type = EOpenPocketBaseFieldType::Text;
    Title.bRequired = true;
    FOpenPocketBaseSchemaField Done;
    Done.Id = TEXT("done_id");
    Done.Name = TEXT("done");
    Done.Type = EOpenPocketBaseFieldType::Boolean;
    Tasks.Fields = {Title, Done};

    FOpenPocketBaseSchemaCollection Users;
    Users.Id = TEXT("users_id");
    Users.Name = TEXT("sdk_users");
    Users.Type = EOpenPocketBaseCollectionType::Auth;
    FOpenPocketBaseSchemaField Email;
    Email.Id = TEXT("email_id");
    Email.Name = TEXT("email");
    Email.Type = EOpenPocketBaseFieldType::Email;
    Users.Fields = {Email};

    FOpenPocketBaseSchemaCollection Report;
    Report.Id = TEXT("report_id");
    Report.Name = TEXT("task_report");
    Report.Type = EOpenPocketBaseCollectionType::View;
    Current->Collections = {Tasks, Users, Report};

    const FOpenPocketBaseSchemaSummary Summary =
        FOpenPocketBaseSchemaDiagnostics::Summarize(*Current);
    TestEqual(TEXT("The summary counts every collection"), Summary.CollectionCount, 3);
    TestEqual(TEXT("The summary counts base collections"), Summary.BaseCollectionCount, 1);
    TestEqual(TEXT("The summary counts auth collections"), Summary.AuthCollectionCount, 1);
    TestEqual(TEXT("The summary counts view collections"), Summary.ViewCollectionCount, 1);
    TestEqual(TEXT("The summary counts fields"), Summary.FieldCount, 3);
    TestEqual(TEXT("The summary counts required fields"), Summary.RequiredFieldCount, 1);
    TestTrue(TEXT("The summary explains the schema at a glance"), Summary.ToText().Contains(TEXT("3 collections")));

    UOpenPocketBaseSchema* Candidate = DuplicateObject<UOpenPocketBaseSchema>(
        Current,
        GetTransientPackage());
    Candidate->Collections[0].Name = TEXT("tasks");
    Candidate->Collections[0].Fields[0].Name = TEXT("headline");
    Candidate->Collections[0].Fields[0].bRequired = false;
    Candidate->Collections[0].Fields.RemoveAt(1);
    FOpenPocketBaseSchemaField Score;
    Score.Id = TEXT("score_id");
    Score.Name = TEXT("score");
    Score.Type = EOpenPocketBaseFieldType::Number;
    Candidate->Collections[0].Fields.Add(Score);

    const FOpenPocketBaseSchemaDiff Diff =
        FOpenPocketBaseSchemaDiagnostics::Compare(*Current, *Candidate);
    TestEqual(TEXT("Collection renames are detected by stable ID"), Diff.RenamedCollections.Num(), 1);
    TestEqual(TEXT("Field renames are detected by stable ID"), Diff.RenamedFields.Num(), 1);
    TestEqual(TEXT("Field constraint changes are detected"), Diff.ChangedFields.Num(), 1);
    TestEqual(TEXT("Removed fields are detected"), Diff.RemovedFields.Num(), 1);
    TestEqual(TEXT("Added fields are detected"), Diff.AddedFields.Num(), 1);
    TestTrue(TEXT("The diff produces readable preview text"), Diff.ToText().Contains(TEXT("headline")));

    UOpenPocketBaseSchema* Broken = NewObject<UOpenPocketBaseSchema>();
    Broken->Collections.Add(Tasks);
    Broken->Collections.Add(Tasks);
    const FOpenPocketBaseSchemaValidationReport Validation =
        FOpenPocketBaseSchemaDiagnostics::Validate(*Broken);
    TestTrue(TEXT("Invalid schema assets report errors"), Validation.HasErrors());
    TestTrue(TEXT("Validation text explains duplicate IDs"), Validation.ToText().Contains(TEXT("Duplicate collection ID")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSchemaSourcePreviewTest,
    "OpenPocketBase.Schema.DetailsPreviewSourceWithoutMutation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSchemaSourcePreviewTest::RunTest(const FString& Parameters)
{
    const FString SourcePath = FPaths::CreateTempFilename(
        *FPaths::ProjectIntermediateDir(), TEXT("OpenPocketBasePreview"), TEXT(".json"));
    const FString Json = TEXT(R"json([{
        "id": "tasks_id",
        "name": "renamed_tasks",
        "type": "base",
        "fields": [{ "id": "title_id", "name": "title", "type": "text" }]
    }])json");
    if (!TestTrue(TEXT("The preview fixture is written"), FFileHelper::SaveStringToFile(Json, *SourcePath)))
    {
        return false;
    }

    UOpenPocketBaseSchema* Current = NewObject<UOpenPocketBaseSchema>();
    Current->SchemaId = FGuid(34, 55, 89, 144);
    Current->Source = SourcePath;
    FOpenPocketBaseSchemaCollection Tasks;
    Tasks.Id = TEXT("tasks_id");
    Tasks.Name = TEXT("sdk_tasks");
    Tasks.Type = EOpenPocketBaseCollectionType::Base;
    Current->Collections.Add(Tasks);

    UOpenPocketBaseSchema* Preview = nullptr;
    FText Error;
    TestTrue(
        TEXT("The source can be loaded as a non-mutating preview"),
        FOpenPocketBaseSchemaDiagnostics::LoadSourcePreview(
            *Current,
            Preview,
            Error));
    TestNotNull(TEXT("A preview schema is returned"), Preview);
    if (Preview != nullptr)
    {
        TestEqual(TEXT("Preview preserves the schema identity"), Preview->SchemaId, Current->SchemaId);
        TestEqual(TEXT("Preview contains source changes"), Preview->Collections[0].Name, FString(TEXT("renamed_tasks")));
    }
    TestEqual(TEXT("Preview does not mutate the current asset"), Current->Collections[0].Name, FString(TEXT("sdk_tasks")));
    IFileManager::Get().Delete(*SourcePath);
    return true;
}

#endif
