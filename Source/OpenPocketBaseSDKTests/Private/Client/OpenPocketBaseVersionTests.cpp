#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseVersion.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseVersionTest,
    "OpenPocketBase.Client.Config.PinsCompatibilityVersion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseVersionTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("The SDK source pins PocketBase"), FString(OpenPocketBase::Version::PocketBase), FString(TEXT("v0.39.11")));
    TestEqual(TEXT("The SDK version is declared"), FString(OpenPocketBase::Version::Sdk), FString(TEXT("0.1.0")));
    return true;
}

#endif
