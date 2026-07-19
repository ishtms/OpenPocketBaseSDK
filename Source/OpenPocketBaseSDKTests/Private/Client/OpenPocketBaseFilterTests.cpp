#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseFilter.h"
#include "OpenPocketBaseSchema.h"

#include <limits>
#include <type_traits>

static_assert(std::is_same_v<
    decltype(&FOpenPocketBaseFilter::DynamicRaw),
    FOpenPocketBaseFilter (*)(FString)>);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseTypedFilterBindingTest,
    "OpenPocketBase.Client.Filters.BindsTypedValuesSafely",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseTypedFilterBindingTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseDynamicFilterParams Params;
    TestTrue(TEXT("String parameters are accepted"), Params.AddString(TEXT("text"), TEXT("x\"") TEXT(" || true || title = \"y")));
    TestTrue(TEXT("Number parameters are accepted"), Params.AddNumber(TEXT("score"), 12.5));
    TestTrue(TEXT("Boolean parameters are accepted"), Params.AddBoolean(TEXT("done"), false));
    TestTrue(
        TEXT("Date parameters are accepted"),
        Params.AddDate(TEXT("created"), FDateTime(2026, 8, 22, 10, 11, 12, 345)));
    TestTrue(TEXT("Null parameters are accepted"), Params.AddNull(TEXT("owner")));
    TestTrue(
        TEXT("String arrays are accepted"),
        Params.AddStringArray(TEXT("labels"), {TEXT("urgent"), TEXT("quoted\"value")}));
    TestTrue(TEXT("Number arrays are accepted"), Params.AddNumberArray(TEXT("scores"), {1.0, 2.5}));
    TestTrue(TEXT("Boolean arrays are accepted"), Params.AddBooleanArray(TEXT("states"), {true, false}));

    const FString Expression =
        TEXT("title = {:text} && score >= {:score} && done = {:done} && created >= {:created} && ")
        TEXT("owner = {:owner} && labels ?= {:labels} && score ?= {:scores} && done ?= {:states}");
    FOpenPocketBaseFilter Bound;
    FOpenPocketBaseError Error;
    TestTrue(TEXT("A complete typed filter binds"), FOpenPocketBaseFilter::TryBindDynamic(Expression, Params, Bound, Error));
    TestEqual(
        TEXT("Typed values use the PocketBase JavaScript SDK wire encoding"),
        Bound.ToString(),
        FString(
            TEXT("title = \"x\\\" || true || title = \\\"y\" && score >= 12.5 && done = false && ")
            TEXT("created >= \"2026-08-22 10:11:12.345Z\" && owner = null && ")
            TEXT("labels ?= \"[\\\"urgent\\\",\\\"quoted\\\\\\\"value\\\"]\" && ")
            TEXT("score ?= \"[1,2.5]\" && done ?= \"[true,false]\"")));

    FOpenPocketBaseDynamicFilterParams RecursiveParams;
    RecursiveParams.AddString(TEXT("value"), TEXT("{:admin}"));
    TestTrue(
        TEXT("Encoded values are not rescanned as placeholders"),
        FOpenPocketBaseFilter::TryBindDynamic(TEXT("name = {:value}"), RecursiveParams, Bound, Error));
    TestEqual(
        TEXT("Placeholder-like input stays quoted"),
        Bound.ToString(),
        FString(TEXT("name = \"{:admin}\"")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseFilterValidationTest,
    "OpenPocketBase.Client.Filters.RejectsBindingMistakes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseFilterValidationTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseDynamicFilterParams Params;
    TestFalse(
        TEXT("Non-finite numbers are rejected"),
        Params.AddNumber(TEXT("score"), std::numeric_limits<double>::infinity()));
    TestFalse(TEXT("Invalid placeholder names are rejected"), Params.AddString(TEXT("bad-name"), TEXT("value")));
    TestTrue(TEXT("Valid placeholder names are accepted"), Params.AddString(TEXT("status"), TEXT("open")));

    FOpenPocketBaseFilter Bound;
    FOpenPocketBaseError Error;
    TestFalse(
        TEXT("Unknown placeholders are rejected"),
        FOpenPocketBaseFilter::TryBindDynamic(TEXT("owner = {:owner}"), Params, Bound, Error));
    TestEqual(TEXT("Unknown placeholders report invalid input"), Error.Kind, EOpenPocketBaseErrorKind::InvalidArgument);

    TestFalse(
        TEXT("Unused parameters are rejected"),
        FOpenPocketBaseFilter::TryBindDynamic(TEXT("status = 'open'"), Params, Bound, Error));
    TestFalse(
        TEXT("Unclosed placeholders are rejected"),
        FOpenPocketBaseFilter::TryBindDynamic(TEXT("status = {:status"), Params, Bound, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseComposableFilterTest,
    "OpenPocketBase.Client.Filters.ComposesTypedValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseComposableFilterTest::RunTest(const FString& Parameters)
{
    UOpenPocketBaseSchema* Schema = NewObject<UOpenPocketBaseSchema>();
    Schema->SchemaId = FGuid(3, 5, 8, 13);

    FOpenPocketBaseSchemaCollection Tasks;
    Tasks.Id = TEXT("tasks_id");
    Tasks.Name = TEXT("sdk_tasks");

    FOpenPocketBaseSchemaField Done;
    Done.Id = TEXT("done_id");
    Done.Name = TEXT("done");
    Done.Type = EOpenPocketBaseFieldType::Boolean;
    Tasks.Fields.Add(Done);

    FOpenPocketBaseSchemaField Title;
    Title.Id = TEXT("title_id");
    Title.Name = TEXT("title");
    Title.Type = EOpenPocketBaseFieldType::Text;
    Tasks.Fields.Add(Title);

    FOpenPocketBaseSchemaField Updated;
    Updated.Id = TEXT("updated_id");
    Updated.Name = TEXT("updated");
    Updated.Type = EOpenPocketBaseFieldType::Date;
    Tasks.Fields.Add(Updated);

    FOpenPocketBaseSchemaCollection Other;
    Other.Id = TEXT("other_id");
    Other.Name = TEXT("other");
    FOpenPocketBaseSchemaField OtherTitle = Title;
    OtherTitle.Id = TEXT("other_title_id");
    Other.Fields.Add(OtherTitle);
    Schema->Collections = {Tasks, Other};

    FOpenPocketBaseCollectionRef TasksRef;
    Schema->MakeCollectionRef(Tasks.Id, TasksRef);
    FOpenPocketBaseBooleanFieldRef DoneRef;
    Schema->MakeTypedFieldRef(TasksRef, Done.Id, DoneRef);
    FOpenPocketBaseStringFieldRef TitleRef;
    Schema->MakeTypedFieldRef(TasksRef, Title.Id, TitleRef);
    FOpenPocketBaseDateFieldRef UpdatedRef;
    Schema->MakeTypedFieldRef(TasksRef, Updated.Id, UpdatedRef);

    const FOpenPocketBaseFilter Filter = FOpenPocketBaseFilter::Boolean(
        DoneRef,
        EOpenPocketBaseBooleanComparison::Equals,
        false).And(FOpenPocketBaseFilter::String(
            TitleRef,
            EOpenPocketBaseStringComparison::Contains,
            TEXT("x\"") TEXT(" || true")));

    TestTrue(TEXT("Typed filters are valid"), Filter.IsValid());
    TestTrue(TEXT("Typed filters retain collection ownership"), Filter.BelongsTo(TasksRef));
    TestEqual(
        TEXT("Typed filters escape values and preserve grouping"),
        Filter.ToString(),
        FString(TEXT("(done = false) && (title ~ \"x\\\" || true\")")));

    const FOpenPocketBaseFilter Empty;
    TestEqual(
        TEXT("An empty optional filter does not add a group"),
        Empty.Or(Filter).ToString(),
        Filter.ToString());

    FOpenPocketBaseCollectionRef OtherRef;
    Schema->MakeCollectionRef(Other.Id, OtherRef);
    FOpenPocketBaseStringFieldRef OtherTitleRef;
    Schema->MakeTypedFieldRef(OtherRef, OtherTitle.Id, OtherTitleRef);
    const FOpenPocketBaseFilter Invalid = Filter.And(FOpenPocketBaseFilter::String(
        OtherTitleRef,
        EOpenPocketBaseStringComparison::Equals,
        TEXT("value")));
    TestFalse(TEXT("Filters from different collections cannot be combined"), Invalid.IsValid());
    TestFalse(TEXT("Collection mismatches explain the error"), Invalid.ErrorMessage.IsEmpty());

    const FOpenPocketBaseFilter Date = FOpenPocketBaseFilter::Date(
        UpdatedRef,
        EOpenPocketBaseDateComparison::OnOrAfter,
        FDateTime(2026, 8, 23, 10, 30, 0));
    TestEqual(
        TEXT("Date filters use readable comparisons and PocketBase timestamps"),
        Date.ToString(),
        FString(TEXT("updated >= \"2026-08-23 10:30:00.000Z\"")));
    return true;
}

#endif
