#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseSchema.h"
#include "OpenPocketBaseSchemaCodeGenerator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSchemaCodeGeneratorTest,
    "OpenPocketBase.Schema.GeneratesTypedCppAccessors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSchemaCodeGeneratorTest::RunTest(const FString& Parameters)
{
    UOpenPocketBaseSchema* Schema = NewObject<UOpenPocketBaseSchema>();
    Schema->SchemaId = FGuid(1, 2, 3, 4);
    Schema->Fingerprint = TEXT("schema-fingerprint");

    FOpenPocketBaseSchemaCollection Tasks;
    Tasks.Id = TEXT("tasks_id");
    Tasks.Name = TEXT("sdk_tasks");
    Tasks.Type = EOpenPocketBaseCollectionType::Base;

    FOpenPocketBaseSchemaField Title;
    Title.Id = TEXT("title_id");
    Title.Name = TEXT("title");
    Title.Type = EOpenPocketBaseFieldType::Text;
    Title.bHasMin = true;
    Title.Min = 3.0;
    Title.Pattern = TEXT("^[A-Z]");
    Tasks.Fields.Add(Title);

    FOpenPocketBaseSchemaField Done;
    Done.Id = TEXT("done_id");
    Done.Name = TEXT("done");
    Done.Type = EOpenPocketBaseFieldType::Boolean;
    Tasks.Fields.Add(Done);

    FOpenPocketBaseSchemaField Attachments;
    Attachments.Id = TEXT("attachments_id");
    Attachments.Name = TEXT("attachments");
    Attachments.Type = EOpenPocketBaseFieldType::File;
    Attachments.bMultiple = true;
    Tasks.Fields.Add(Attachments);

    FOpenPocketBaseSchemaField Owner;
    Owner.Id = TEXT("owner_id");
    Owner.Name = TEXT("owner");
    Owner.Type = EOpenPocketBaseFieldType::Relation;
    Owner.RelatedCollectionId = TEXT("users_id");
    Tasks.Fields.Add(Owner);

    FOpenPocketBaseSchemaField Reviewers;
    Reviewers.Id = TEXT("reviewers_id");
    Reviewers.Name = TEXT("reviewers");
    Reviewers.Type = EOpenPocketBaseFieldType::Relation;
    Reviewers.bMultiple = true;
    Reviewers.MinSelect = 1;
    Reviewers.MaxSelect = 3;
    Reviewers.RelatedCollectionId = TEXT("users_id");
    Tasks.Fields.Add(Reviewers);

    FOpenPocketBaseSchemaField Status;
    Status.Id = TEXT("status_id");
    Status.Name = TEXT("status");
    Status.Type = EOpenPocketBaseFieldType::Select;
    Status.Choices = {TEXT("active"), TEXT("paused")};
    Tasks.Fields.Add(Status);

    FOpenPocketBaseSchemaField Labels;
    Labels.Id = TEXT("labels_id");
    Labels.Name = TEXT("labels");
    Labels.Type = EOpenPocketBaseFieldType::Select;
    Labels.bMultiple = true;
    Labels.Choices = {TEXT("sdk"), TEXT("unreal")};
    Tasks.Fields.Add(Labels);

    FOpenPocketBaseSchemaCollection Users;
    Users.Id = TEXT("users_id");
    Users.Name = TEXT("sdk_users");
    Users.Type = EOpenPocketBaseCollectionType::Auth;

    FOpenPocketBaseSchemaCollection Summary;
    Summary.Id = TEXT("summary_id");
    Summary.Name = TEXT("task_summary");
    Summary.Type = EOpenPocketBaseCollectionType::View;

    Schema->Collections = {Tasks, Users, Summary};

    FString Header;
    FText Error;
    TestTrue(
        TEXT("A valid schema generates a header"),
        FOpenPocketBaseSchemaCodeGenerator::GenerateHeader(
            *Schema,
            TEXT("TestGame::PocketBase"),
            Header,
            Error));
    TestTrue(TEXT("Generation errors stay empty"), Error.IsEmpty());
    TestTrue(TEXT("The requested namespace is used"), Header.Contains(TEXT("namespace TestGame::PocketBase")));
    TestTrue(TEXT("Collection names become C++ identifiers"), Header.Contains(TEXT("namespace SdkTasks")));
    TestTrue(TEXT("Base collections return writable references"), Header.Contains(TEXT("inline FOpenPocketBaseWritableCollectionRef Ref()")));
    TestTrue(TEXT("Base collections expose writable services"), Header.Contains(TEXT("return Client.WritableCollection(Ref());")));
    TestTrue(TEXT("Auth collections expose auth services"), Header.Contains(TEXT("return Client.AuthCollection(Ref());")));
    TestTrue(TEXT("Views expose read-only services"), Header.Contains(TEXT("return Client.Collection(Ref());")));
    TestTrue(TEXT("Text fields use the semantic text reference"), Header.Contains(TEXT("inline FOpenPocketBaseTextFieldRef Title()")));
    TestTrue(TEXT("Boolean fields are typed"), Header.Contains(TEXT("inline FOpenPocketBaseBooleanFieldRef Done()")));
    TestTrue(TEXT("File fields expose upload references"), Header.Contains(TEXT("inline FOpenPocketBaseFileFieldRef Attachments()")));
    TestTrue(TEXT("File values expose filename arrays"), Header.Contains(TEXT("inline FOpenPocketBaseStringArrayFieldRef AttachmentsValue()")));
    TestTrue(TEXT("Single relations use single-relation references"), Header.Contains(TEXT("inline FOpenPocketBaseSingleRelationFieldRef Owner()")));
    TestTrue(TEXT("Multiple relations use multiple-relation references"), Header.Contains(TEXT("inline FOpenPocketBaseMultipleRelationFieldRef Reviewers()")));
    TestTrue(TEXT("Relation values expose record IDs"), Header.Contains(TEXT("inline FOpenPocketBaseStringFieldRef OwnerValue()")));
    TestTrue(TEXT("Single selects use single-select references"), Header.Contains(TEXT("inline FOpenPocketBaseSingleSelectFieldRef Status()")));
    TestTrue(TEXT("Multiple selects use multiple-select references"), Header.Contains(TEXT("inline FOpenPocketBaseMultipleSelectFieldRef Labels()")));
    TestTrue(TEXT("Generated accessors retain string constraints"), Header.Contains(TEXT("Result.Pattern = TEXT(\"^[A-Z]\")")));
    TestTrue(TEXT("Generated accessors retain select choices"), Header.Contains(TEXT("Result.Choices = {TEXT(\"active\"), TEXT(\"paused\")}")));
    TestTrue(TEXT("Generated accessors retain relation limits"), Header.Contains(TEXT("Result.MaxSelect = 3")));
    TestTrue(TEXT("The schema fingerprint is embedded"), Header.Contains(TEXT("schema-fingerprint")));
    TestTrue(TEXT("Generated code can compare an asset fingerprint"), Header.Contains(TEXT("IsCurrentSchema")));

    Schema->Collections.Swap(0, 2);
    Schema->Collections[2].Fields.Swap(0, 3);
    FString SecondHeader;
    TestTrue(
        TEXT("The same schema generates again"),
        FOpenPocketBaseSchemaCodeGenerator::GenerateHeader(
            *Schema,
            TEXT("TestGame::PocketBase"),
            SecondHeader,
            Error));
    TestEqual(TEXT("Generation is independent of schema array order"), SecondHeader, Header);

    Schema->SchemaId.Invalidate();
    TestFalse(
        TEXT("Schemas without a stable ID are rejected"),
        FOpenPocketBaseSchemaCodeGenerator::GenerateHeader(
            *Schema,
            TEXT("TestGame::PocketBase"),
            Header,
            Error));
    TestFalse(TEXT("Invalid schemas return a useful error"), Error.IsEmpty());
    return true;
}

#endif
