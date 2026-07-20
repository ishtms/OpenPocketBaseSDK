#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "OpenPocketBaseRecordLibrary.h"
#include "OpenPocketBaseSchema.h"

namespace
{
struct FRecordFieldFixture
{
    UOpenPocketBaseSchema* Schema = nullptr;
    FOpenPocketBaseCollectionRef Tasks;
    FOpenPocketBaseCollectionRef Other;
    FOpenPocketBaseStringFieldRef Title;
    FOpenPocketBaseStringFieldRef Description;
    FOpenPocketBaseStringFieldRef Missing;
    FOpenPocketBaseStringFieldRef WrongType;
    FOpenPocketBaseNumberFieldRef Priority;
    FOpenPocketBaseBooleanFieldRef Done;
    FOpenPocketBaseStringFieldRef OtherTitle;

    FRecordFieldFixture()
    {
        Schema = NewObject<UOpenPocketBaseSchema>();
        Schema->SchemaId = FGuid(21, 34, 55, 89);

        FOpenPocketBaseSchemaCollection TasksCollection;
        TasksCollection.Id = TEXT("tasks_id");
        TasksCollection.Name = TEXT("sdk_tasks");
        TasksCollection.Fields = {
            MakeField(TEXT("title_id"), TEXT("title"), EOpenPocketBaseFieldType::Text),
            MakeField(TEXT("description_id"), TEXT("description"), EOpenPocketBaseFieldType::Text),
            MakeField(TEXT("missing_id"), TEXT("missing"), EOpenPocketBaseFieldType::Text),
            MakeField(TEXT("wrong_type_id"), TEXT("wrongType"), EOpenPocketBaseFieldType::Text),
            MakeField(TEXT("priority_id"), TEXT("priority"), EOpenPocketBaseFieldType::Number),
            MakeField(TEXT("done_id"), TEXT("done"), EOpenPocketBaseFieldType::Boolean)};

        FOpenPocketBaseSchemaCollection OtherCollection;
        OtherCollection.Id = TEXT("other_id");
        OtherCollection.Name = TEXT("other");
        OtherCollection.Fields = {
            MakeField(TEXT("other_title_id"), TEXT("title"), EOpenPocketBaseFieldType::Text)};

        Schema->Collections = {TasksCollection, OtherCollection};
        Schema->MakeCollectionRef(TasksCollection.Id, Tasks);
        Schema->MakeCollectionRef(OtherCollection.Id, Other);
        Schema->MakeTypedFieldRef(Tasks, TEXT("title_id"), Title);
        Schema->MakeTypedFieldRef(Tasks, TEXT("description_id"), Description);
        Schema->MakeTypedFieldRef(Tasks, TEXT("missing_id"), Missing);
        Schema->MakeTypedFieldRef(Tasks, TEXT("wrong_type_id"), WrongType);
        Schema->MakeTypedFieldRef(Tasks, TEXT("priority_id"), Priority);
        Schema->MakeTypedFieldRef(Tasks, TEXT("done_id"), Done);
        Schema->MakeTypedFieldRef(Other, TEXT("other_title_id"), OtherTitle);
    }

private:
    static FOpenPocketBaseSchemaField MakeField(
        const FString& Id,
        const FString& Name,
        EOpenPocketBaseFieldType Type)
    {
        FOpenPocketBaseSchemaField Field;
        Field.Id = Id;
        Field.Name = Name;
        Field.Type = Type;
        return Field;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRecordLibraryTest,
    "OpenPocketBase.Blueprint.Records.FieldStatesRemainDistinct",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRecordLibraryTest::RunTest(const FString& Parameters)
{
    const FRecordFieldFixture Fields;
    FOpenPocketBaseRecord Record;
    Record.CollectionId = Fields.Tasks.CollectionId;
    Record.Data.JsonObject = MakeShared<FJsonObject>();
    Record.Data.JsonObject->SetStringField(TEXT("title"), TEXT("Ship SDK"));
    Record.Data.JsonObject->SetField(TEXT("description"), MakeShared<FJsonValueNull>());
    Record.Data.JsonObject->SetNumberField(TEXT("wrongType"), 3);
    Record.Data.JsonObject->SetNumberField(TEXT("priority"), 3);

    FString TextValue;
    TestEqual(
        TEXT("A string field is found"),
        UOpenPocketBaseRecordLibrary::GetStringFieldState(Record, Fields.Title, TextValue),
        EOpenPocketBaseFieldState::Found);
    TestEqual(TEXT("The string value is preserved"), TextValue, FString(TEXT("Ship SDK")));

    TestEqual(
        TEXT("An absent field remains distinguishable"),
        UOpenPocketBaseRecordLibrary::GetStringFieldState(Record, Fields.Missing, TextValue),
        EOpenPocketBaseFieldState::Missing);
    TestEqual(
        TEXT("A field from another collection is rejected"),
        UOpenPocketBaseRecordLibrary::GetStringFieldState(Record, Fields.OtherTitle, TextValue),
        EOpenPocketBaseFieldState::WrongCollection);
    TestEqual(
        TEXT("A null field remains distinguishable"),
        UOpenPocketBaseRecordLibrary::GetStringFieldState(Record, Fields.Description, TextValue),
        EOpenPocketBaseFieldState::Null);
    TestEqual(
        TEXT("A wrong type remains distinguishable"),
        UOpenPocketBaseRecordLibrary::GetStringFieldState(Record, Fields.WrongType, TextValue),
        EOpenPocketBaseFieldState::WrongType);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRecordValueBuilderTest,
    "OpenPocketBase.Blueprint.Records.BuildsImmutableRequestValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRecordValueBuilderTest::RunTest(const FString& Parameters)
{
    const FRecordFieldFixture Fields;
    const FOpenPocketBaseRecordBody Empty = UOpenPocketBaseRecordLibrary::NewRecordBody();
    const FOpenPocketBaseRecordBody WithTitle = UOpenPocketBaseRecordLibrary::WithStringField(
        Empty,
        Fields.Title,
        TEXT("Ship SDK"),
        EOpenPocketBaseFieldModifier::Replace);
    const FOpenPocketBaseRecordBody WithDone = UOpenPocketBaseRecordLibrary::WithBooleanField(
        WithTitle,
        Fields.Done,
        false,
        EOpenPocketBaseFieldModifier::Replace);

    TestFalse(TEXT("The original body remains unchanged"), Empty.Data.JsonObject->HasField(TEXT("title")));
    TestEqual(
        TEXT("Pure body nodes preserve earlier fields"),
        WithDone.Data.JsonObject->GetStringField(TEXT("title")),
        FString(TEXT("Ship SDK")));
    TestFalse(
        TEXT("Pure body nodes add the new field"),
        WithDone.Data.JsonObject->GetBoolField(TEXT("done")));

    FOpenPocketBaseRecordBody NativeBody;
    NativeBody
        .SetStringField(Fields.Title, TEXT("Native"))
        .SetNumberField(Fields.Priority, 5.0)
        .SetBooleanField(Fields.Done, true);
    TestTrue(
        TEXT("Native body setters chain"),
        NativeBody.Data.JsonObject->GetBoolField(TEXT("done")));

    FOpenPocketBaseRecordBody NullBody;
    NullBody.SetNullField(Fields.Title);
    TestTrue(
        TEXT("Specific field references can be set to null"),
        NullBody.Data.JsonObject->HasTypedField<EJson::Null>(TEXT("title")));

    FOpenPocketBaseListOptions Options;
    Options
        .AtPage(2)
        .PageSize(50)
        .Where(FOpenPocketBaseFilter::Boolean(
            Fields.Done,
            EOpenPocketBaseBooleanComparison::Equals,
            false))
        .SkipTotals();
    TestEqual(TEXT("Native list options chain the page"), Options.Page, 2);
    TestEqual(TEXT("Native list options chain the page size"), Options.PerPage, 50);
    TestEqual(
        TEXT("Native list options chain a typed filter"),
        Options.Filter.ToString(),
        FString(TEXT("done = false")));
    TestTrue(TEXT("Native list options chain total skipping"), Options.bSkipTotal);

    const FOpenPocketBaseRecordBody InvalidBody = UOpenPocketBaseRecordLibrary::WithStringField(
        WithDone,
        Fields.OtherTitle,
        TEXT("Wrong collection"));
    TestFalse(TEXT("A record body cannot mix collections"), InvalidBody.IsValid());
    TestFalse(TEXT("Body collection mismatches explain the error"), InvalidBody.ErrorMessage.IsEmpty());
    return true;
}

#endif
