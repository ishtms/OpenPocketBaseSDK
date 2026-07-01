#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseDate.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseDateTest,
    "OpenPocketBase.Client.Records.ParsesAndFormatsUtcDates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseDateTest::RunTest(const FString& Parameters)
{
    const FString PocketBaseValue = TEXT("2026-08-22 10:01:02.345Z");
    FDateTime Parsed;
    TestTrue(TEXT("A PocketBase UTC date parses"), OpenPocketBase::Date::TryParse(PocketBaseValue, Parsed));
    TestEqual(TEXT("The parsed date formats without loss"), OpenPocketBase::Date::Format(Parsed), PocketBaseValue);

    TestFalse(
        TEXT("A date without an explicit UTC suffix is rejected"),
        OpenPocketBase::Date::TryParse(TEXT("2026-08-22 10:01:02.345"), Parsed));
    TestFalse(
        TEXT("An invalid calendar date is rejected"),
        OpenPocketBase::Date::TryParse(TEXT("2026-13-42 10:01:02.345Z"), Parsed));
    return true;
}

#endif
