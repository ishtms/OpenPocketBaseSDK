#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseRecord.h"
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

    FOpenPocketBaseFieldRef CurrentDone;
    TestTrue(TEXT("Field references resolve their current schema value"), DoneRef.ResolveCurrent(CurrentDone));
    TestEqual(TEXT("Resolved references use the current wire name"), CurrentDone.Name, FString(TEXT("completed")));

    FOpenPocketBaseRecordBody Body;
    Body.SetBooleanField(TypedDoneRef, true);
    TestTrue(
        TEXT("Record bodies use the current wire name after a schema rename"),
        Body.Data.JsonObject.IsValid() && Body.Data.JsonObject->HasField(TEXT("completed")));
    TestFalse(
        TEXT("Record bodies do not send the stale wire name"),
        Body.Data.JsonObject.IsValid() && Body.Data.JsonObject->HasField(TEXT("done")));

    TasksRef.Name = TEXT("old_tasks");
    FOpenPocketBaseCollectionRef CurrentTasks;
    TestTrue(TEXT("Collection references resolve their current schema value"), TasksRef.ResolveCurrent(CurrentTasks));
    TestEqual(TEXT("Resolved collections use the current wire name"), CurrentTasks.Name, FString(TEXT("sdk_tasks")));

    Schema->SchemaId = FGuid(5, 8, 13, 21);
    FOpenPocketBaseCollectionRef RejectedCollection = TasksRef;
    TestFalse(TEXT("Replaced schemas reject collection references"), TasksRef.ResolveCurrent(RejectedCollection));
    TestFalse(TEXT("Rejected collection outputs are cleared"), RejectedCollection.IsSet());
    FOpenPocketBaseFieldRef RejectedField = DoneRef;
    TestFalse(TEXT("Replaced schemas reject field references"), DoneRef.ResolveCurrent(RejectedField));
    TestFalse(TEXT("Rejected field outputs are cleared"), RejectedField.IsSet());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseDefaultTextLimitTest,
    "OpenPocketBase.Schema.RecordBodiesRespectDefaultTextLimits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseDefaultTextLimitTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseTextFieldRef Field;
    Field.SchemaId = FGuid(1, 2, 3, 4);
    Field.CollectionId = TEXT("users");
    Field.FieldId = TEXT("name_id");
    Field.Name = TEXT("name");
    Field.Type = EOpenPocketBaseFieldType::Text;
    Field.bHasMax = true;
    Field.Max = 0;

    FOpenPocketBaseRecordBody TextBody;
    TextBody.SetStringField(Field, TEXT("hello"));
    TestTrue(TEXT("An exported zero maximum accepts ordinary text"), TextBody.IsValid());
    FOpenPocketBaseRecordBody LongTextBody;
    LongTextBody.SetStringField(Field, FString::ChrN(5001, TEXT('a')));
    TestFalse(TEXT("The default text maximum is still enforced"), LongTextBody.IsValid());

    Field.Name = TEXT("password");
    Field.Type = EOpenPocketBaseFieldType::Password;
    FOpenPocketBaseRecordBody PasswordBody;
    PasswordBody.SetStringField(Field, FString::ChrN(71, TEXT('a')));
    TestTrue(TEXT("An exported zero maximum accepts a password"), PasswordBody.IsValid());
    FOpenPocketBaseRecordBody LongPasswordBody;
    LongPasswordBody.SetStringField(Field, FString::ChrN(72, TEXT('a')));
    TestFalse(TEXT("The default password maximum is still enforced"), LongPasswordBody.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseNumberDeltaTest,
    "OpenPocketBase.Schema.NumberModifiersDoNotValidateDeltasAsFinalValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseNumberDeltaTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseNumberFieldRef Field;
    Field.SchemaId = FGuid(1, 2, 3, 4);
    Field.CollectionId = TEXT("tasks");
    Field.FieldId = TEXT("score_id");
    Field.Name = TEXT("score");
    Field.Type = EOpenPocketBaseFieldType::Number;
    Field.bHasMin = true;
    Field.Min = 100;
    Field.bHasMax = true;
    Field.Max = 1000;

    FOpenPocketBaseRecordBody Replacement;
    Replacement.SetNumberField(Field, 5);
    TestFalse(TEXT("Replacement values still respect the minimum"), Replacement.IsValid());
    FOpenPocketBaseRecordBody Increment;
    Increment.SetNumberField(Field, 5, EOpenPocketBaseFieldModifier::Append);
    TestTrue(TEXT("An increment is validated by the server against the stored value"), Increment.IsValid());
    TestEqual(TEXT("The increment retains its delta"), Increment.Data.JsonObject->GetNumberField(TEXT("score+")), 5.0);
    FOpenPocketBaseRecordBody Decrement;
    Decrement.SetNumberField(Field, 5, EOpenPocketBaseFieldModifier::Remove);
    TestTrue(TEXT("A decrement is not mistaken for a replacement"), Decrement.IsValid());
    TestEqual(TEXT("The decrement retains its delta"), Decrement.Data.JsonObject->GetNumberField(TEXT("score-")), 5.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseFieldValueParityTest,
    "OpenPocketBase.Schema.RecordBodiesAcceptPocketBaseEmptyAndUnicodeValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseFieldValueParityTest::RunTest(const FString& Parameters)
{
    const auto Initialize = [](FOpenPocketBaseFieldRef& Field, const TCHAR* Name, EOpenPocketBaseFieldType Type)
    {
        Field.SchemaId = FGuid(1, 2, 3, 4);
        Field.CollectionId = TEXT("tasks");
        Field.FieldId = Name;
        Field.Name = Name;
        Field.Type = Type;
    };
    FOpenPocketBaseTextFieldRef Text;
    Initialize(Text, TEXT("label"), EOpenPocketBaseFieldType::Text);
    Text.bHasMin = true;
    Text.Min = 3;
    Text.bHasMax = true;
    Text.Max = 10;
    FOpenPocketBaseRecordBody EmptyText;
    EmptyText.SetStringField(Text, TEXT(""));
    TestTrue(TEXT("Empty text is left to the server's required-field validation"), EmptyText.IsValid());

    Text.Min = 0;
    Text.Max = 1;
    FOpenPocketBaseRecordBody Unicode;
    Unicode.SetStringField(Text, TEXT("\U0001F600"));
    TestTrue(TEXT("A supplementary character counts as one character"), Unicode.IsValid());
    FOpenPocketBaseRecordBody LongUnicode;
    LongUnicode.SetStringField(Text, TEXT("\U0001F600\U0001F600"));
    TestFalse(TEXT("Two supplementary characters exceed a one-character maximum"), LongUnicode.IsValid());

    Text.Type = EOpenPocketBaseFieldType::Password;
    Text.Max = 1;
    FOpenPocketBaseRecordBody Password;
    Password.SetStringField(Text, TEXT("\U0001F600"));
    TestTrue(TEXT("Password limits also count Unicode characters"), Password.IsValid());
    FOpenPocketBaseRecordBody LongPassword;
    LongPassword.SetStringField(Text, TEXT("\U0001F600a"));
    TestFalse(TEXT("The password character maximum is enforced"), LongPassword.IsValid());

    FOpenPocketBaseNumberFieldRef Number;
    Initialize(Number, TEXT("score"), EOpenPocketBaseFieldType::Number);
    Number.bHasMin = true;
    Number.Min = 100;
    FOpenPocketBaseRecordBody Zero;
    Zero.SetNumberField(Number, 0);
    TestTrue(TEXT("Zero can reset an optional number with a positive minimum"), Zero.IsValid());

    FOpenPocketBaseSingleSelectFieldRef Choice;
    Initialize(Choice, TEXT("choice"), EOpenPocketBaseFieldType::Select);
    Choice.Choices = {TEXT("red"), TEXT("green")};
    FOpenPocketBaseRecordBody ClearChoice;
    ClearChoice.SetSingleSelectField(Choice, TEXT(""));
    TestTrue(TEXT("An optional single select can be cleared"), ClearChoice.IsValid());
    FOpenPocketBaseRecordBody InvalidChoice;
    InvalidChoice.SetSingleSelectField(Choice, TEXT("blue"));
    TestFalse(TEXT("Unknown nonempty select values remain invalid"), InvalidChoice.IsValid());

    FOpenPocketBaseSingleRelationFieldRef Relation;
    Initialize(Relation, TEXT("parent"), EOpenPocketBaseFieldType::Relation);
    Relation.RelatedCollectionId = TEXT("parents");
    FOpenPocketBaseRecordBody ClearRelation;
    ClearRelation.SetSingleRelationField(Relation, TEXT(""));
    TestTrue(TEXT("An optional single relation can be cleared"), ClearRelation.IsValid());

    FOpenPocketBaseMultipleSelectFieldRef Choices;
    Initialize(Choices, TEXT("choices"), EOpenPocketBaseFieldType::Select);
    Choices.bMultiple = true;
    Choices.MinSelect = 2;
    Choices.MaxSelect = 3;
    FOpenPocketBaseRecordBody ClearChoices;
    ClearChoices.SetMultipleSelectField(Choices, {});
    TestTrue(TEXT("An optional multiple select can be cleared below Min Select"), ClearChoices.IsValid());

    FOpenPocketBaseMultipleRelationFieldRef Relations;
    Initialize(Relations, TEXT("parents"), EOpenPocketBaseFieldType::Relation);
    Relations.RelatedCollectionId = TEXT("parents");
    Relations.bMultiple = true;
    Relations.MinSelect = 2;
    Relations.MaxSelect = 3;
    FOpenPocketBaseRecordBody ClearRelations;
    ClearRelations.SetMultipleRelationField(Relations, {});
    TestTrue(TEXT("An optional multiple relation can be cleared below Min Select"), ClearRelations.IsValid());
    return true;
}

#endif
