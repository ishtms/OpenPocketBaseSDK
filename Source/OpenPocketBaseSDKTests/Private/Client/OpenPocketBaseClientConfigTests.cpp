#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClientConfig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseClientConfigNormalizationTest,
    "OpenPocketBase.Client.Config.NormalizesAndRejectsCredentials",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseClientConfigNormalizationTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("  https://pb.example.com///  ");

    FString NormalizedBaseUrl;
    FOpenPocketBaseError Error;

    TestTrue(TEXT("A valid origin normalizes"), Config.TryGetNormalizedBaseUrl(NormalizedBaseUrl, Error));
    TestEqual(TEXT("Trailing slashes are removed"), NormalizedBaseUrl, FString(TEXT("https://pb.example.com")));

    Config.BaseUrl = TEXT("https://user:secret@pb.example.com");
    TestFalse(TEXT("Embedded credentials are rejected"), Config.TryGetNormalizedBaseUrl(NormalizedBaseUrl, Error));
    TestEqual(TEXT("Invalid origins use InvalidArgument"), Error.Kind, EOpenPocketBaseErrorKind::InvalidArgument);

    return true;
}

#endif
