#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseSchema.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSchemaReferencesTest,
    "OpenPocketBase.Schema.CreatesStableTypedReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSchemaReferencesTest::RunTest(const FString& Parameters)
{
    UOpenPocketBaseSchema* Schema = NewObject<UOpenPocketBaseSchema>();
    Schema->SchemaId = FGuid(1, 2, 3, 4);
    Schema->PocketBaseVersion = TEXT("0.39.11");

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

    FOpenPocketBaseSchemaCollection Users;
    Users.Id = TEXT("users_id");
    Users.Name = TEXT("sdk_users");
    Users.Type = EOpenPocketBaseCollectionType::Auth;

    Schema->Collections = {Tasks, Users};

    FOpenPocketBaseCollectionRef TasksRef;
    TestTrue(TEXT("Collection references can be created by name"), Schema->MakeCollectionRef(TEXT("sdk_tasks"), TasksRef));
    TestEqual(TEXT("Collection references retain the stable ID"), TasksRef.CollectionId, FString(TEXT("tasks_id")));
    TestEqual(TEXT("Collection references retain the collection type"), TasksRef.Type, EOpenPocketBaseCollectionType::Base);

    FOpenPocketBaseFieldRef DoneRef;
    TestTrue(TEXT("Field references can be created by stable ID"), Schema->MakeFieldRef(TasksRef, TEXT("done_id"), DoneRef));
    TestEqual(TEXT("Field references retain the wire name"), DoneRef.Name, FString(TEXT("done")));
    TestEqual(TEXT("Field references expose the value type"), DoneRef.GetValueType(), EOpenPocketBaseFieldValueType::Boolean);
    TestTrue(TEXT("The field belongs to its collection"), DoneRef.BelongsTo(TasksRef));

    FOpenPocketBaseBooleanFieldRef TypedDoneRef;
    TestTrue(TEXT("Boolean fields create Boolean references"), Schema->MakeTypedFieldRef(TasksRef, TEXT("done"), TypedDoneRef));
    FOpenPocketBaseNumberFieldRef WrongTypedRef;
    TestFalse(TEXT("Boolean fields cannot create number references"), Schema->MakeTypedFieldRef(TasksRef, TEXT("done"), WrongTypedRef));

    FOpenPocketBaseCollectionRef UsersRef;
    TestTrue(TEXT("Auth collection references are available"), Schema->MakeCollectionRef(TEXT("sdk_users"), UsersRef));
    FOpenPocketBaseAuthCollectionRef AuthUsersRef;
    TestTrue(
        TEXT("Auth collection pins accept auth collections"),
        Schema->MakeTypedCollectionRef(TEXT("sdk_users"), AuthUsersRef));
    FOpenPocketBaseAuthCollectionRef WrongAuthRef;
    TestFalse(
        TEXT("Auth collection pins reject base collections"),
        Schema->MakeTypedCollectionRef(TEXT("sdk_tasks"), WrongAuthRef));
    FOpenPocketBaseWritableCollectionRef WritableTasksRef;
    TestTrue(
        TEXT("Writable collection pins accept base collections"),
        Schema->MakeTypedCollectionRef(TEXT("sdk_tasks"), WritableTasksRef));
    TestFalse(TEXT("Cross-collection field use is rejected"), DoneRef.BelongsTo(UsersRef));

    Schema->Collections[0].Fields[1].Name = TEXT("completed");
    const FOpenPocketBaseSchemaField* RenamedField = nullptr;
    TestTrue(TEXT("Stable field IDs survive field renames"), Schema->ResolveField(DoneRef, RenamedField));
    TestEqual(TEXT("The current field name comes from the schema"), RenamedField->Name, FString(TEXT("completed")));
    return true;
}

#endif
