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

#endif
