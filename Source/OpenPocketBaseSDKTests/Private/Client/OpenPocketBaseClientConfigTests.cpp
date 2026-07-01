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

    Config.BaseUrl = TEXT("https://pb.example.com:not-a-port");
    TestFalse(TEXT("A non-numeric port is rejected"), Config.TryGetNormalizedBaseUrl(NormalizedBaseUrl, Error));

    Config.BaseUrl = TEXT("https://pb%2eexample.com");
    TestFalse(TEXT("A pre-encoded host is rejected"), Config.TryGetNormalizedBaseUrl(NormalizedBaseUrl, Error));

    Config.BaseUrl = TEXT("http://localhost:8090");
    Config.ProfileName = TEXT("local-dev");
    TestTrue(TEXT("A named local profile is accepted"), Config.TryGetNormalizedBaseUrl(NormalizedBaseUrl, Error));

    Config.ProfileName = TEXT("profile\nsecret");
    TestFalse(TEXT("A profile with control characters is rejected"), Config.TryGetNormalizedBaseUrl(NormalizedBaseUrl, Error));

    return true;
}

#endif
