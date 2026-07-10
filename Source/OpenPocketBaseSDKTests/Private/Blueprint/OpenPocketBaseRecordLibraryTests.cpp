#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "OpenPocketBaseRecordLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRecordLibraryTest,
    "OpenPocketBase.Blueprint.Records.FieldStatesRemainDistinct",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRecordLibraryTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseRecord Record;
    Record.Data.JsonObject = MakeShared<FJsonObject>();
    Record.Data.JsonObject->SetStringField(TEXT("title"), TEXT("Ship SDK"));
    Record.Data.JsonObject->SetField(TEXT("description"), MakeShared<FJsonValueNull>());
    Record.Data.JsonObject->SetNumberField(TEXT("priority"), 3);

    FString TextValue;
    TestEqual(
        TEXT("A string field is found"),
        UOpenPocketBaseRecordLibrary::GetStringFieldState(Record, TEXT("title"), TextValue),
        EOpenPocketBaseFieldState::Found);
    TestEqual(TEXT("The string value is preserved"), TextValue, FString(TEXT("Ship SDK")));

    TestEqual(
        TEXT("An absent field remains distinguishable"),
        UOpenPocketBaseRecordLibrary::GetStringFieldState(Record, TEXT("missing"), TextValue),
        EOpenPocketBaseFieldState::Missing);
    TestEqual(
        TEXT("A null field remains distinguishable"),
        UOpenPocketBaseRecordLibrary::GetStringFieldState(Record, TEXT("description"), TextValue),
        EOpenPocketBaseFieldState::Null);
    TestEqual(
        TEXT("A wrong type remains distinguishable"),
        UOpenPocketBaseRecordLibrary::GetStringFieldState(Record, TEXT("priority"), TextValue),
        EOpenPocketBaseFieldState::WrongType);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRecordValueBuilderTest,
    "OpenPocketBase.Blueprint.Records.BuildsImmutableRequestValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRecordValueBuilderTest::RunTest(const FString& Parameters)
{
    const FOpenPocketBaseRecordBody Empty = UOpenPocketBaseRecordLibrary::NewRecordBody();
    const FOpenPocketBaseRecordBody WithTitle = UOpenPocketBaseRecordLibrary::WithStringField(
        Empty,
        TEXT("title"),
        TEXT("Ship SDK"),
        EOpenPocketBaseFieldModifier::Replace);
    const FOpenPocketBaseRecordBody WithDone = UOpenPocketBaseRecordLibrary::WithBooleanField(
        WithTitle,
        TEXT("done"),
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
        .SetStringField(TEXT("title"), TEXT("Native"))
        .SetNumberField(TEXT("score"), 5.0)
        .SetBooleanField(TEXT("done"), true);
    TestTrue(
        TEXT("Native body setters chain"),
        NativeBody.Data.JsonObject->GetBoolField(TEXT("done")));

    FOpenPocketBaseListOptions Options;
    Options
        .AtPage(2)
        .PageSize(50)
        .Where(FOpenPocketBaseFilter::Boolean(
            TEXT("done"),
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
    return true;
}

#endif
