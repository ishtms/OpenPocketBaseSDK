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
    TestTrue(TEXT("String fields are typed"), Header.Contains(TEXT("inline FOpenPocketBaseStringFieldRef Title()")));
    TestTrue(TEXT("Boolean fields are typed"), Header.Contains(TEXT("inline FOpenPocketBaseBooleanFieldRef Done()")));
    TestTrue(TEXT("File fields expose upload references"), Header.Contains(TEXT("inline FOpenPocketBaseFileFieldRef Attachments()")));
    TestTrue(TEXT("File values expose filename arrays"), Header.Contains(TEXT("inline FOpenPocketBaseStringArrayFieldRef AttachmentsValue()")));
    TestTrue(TEXT("Relations expose expansion references"), Header.Contains(TEXT("inline FOpenPocketBaseRelationFieldRef Owner()")));
    TestTrue(TEXT("Relation values expose record IDs"), Header.Contains(TEXT("inline FOpenPocketBaseStringFieldRef OwnerValue()")));
    TestTrue(TEXT("The schema fingerprint is embedded"), Header.Contains(TEXT("schema-fingerprint")));

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
