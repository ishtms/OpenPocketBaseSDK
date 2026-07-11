#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseOwnedHeadersTest,
    "OpenPocketBase.Client.Config.RejectsOwnedAndCredentialHeaders",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseOwnedHeadersTest::RunTest(const FString& Parameters)
{
    const TArray<FString> ProtectedHeaders = {
        TEXT("Accept"),
        TEXT("Accept-Language"),
        TEXT("Authorization"),
        TEXT("Content-Length"),
        TEXT("Content-Type"),
        TEXT("Cookie"),
        TEXT("Host"),
        TEXT("Proxy-Authorization"),
        TEXT("Set-Cookie"),
        TEXT("X-Api-Key"),
        TEXT("X-Request-Id")
    };

    for (const FString& Header : ProtectedHeaders)
    {
        FOpenPocketBaseClientConfig Config;
        Config.BaseUrl = TEXT("https://pb.example.com");
        Config.DefaultHeaders.Add(Header, TEXT("caller-value"));
        FOpenPocketBaseError Error;
        const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
            CreateOpenPocketBaseTestClient(Config, Error);
        TestFalse(*FString::Printf(TEXT("%s is SDK or security owned"), *Header), Client.IsValid());
        TestEqual(TEXT("Protected headers fail as invalid arguments"), Error.Kind, EOpenPocketBaseErrorKind::InvalidArgument);
    }

    FOpenPocketBaseClientConfig InvalidNameConfig;
    InvalidNameConfig.BaseUrl = TEXT("https://pb.example.com");
    InvalidNameConfig.DefaultHeaders.Add(TEXT("Bad Header"), TEXT("value"));
    FOpenPocketBaseError Error;
    TestFalse(
        TEXT("Invalid HTTP field names are rejected"),
        CreateOpenPocketBaseTestClient(InvalidNameConfig, Error).IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAuthRefreshPolicyBoundsTest,
    "OpenPocketBase.Client.Config.BoundsAuthRefreshPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAuthRefreshPolicyBoundsTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    Config.AuthRefreshLeadTimeSeconds = -1;
    FOpenPocketBaseError Error;
    TestFalse(
        TEXT("A negative Auth Refresh lead time is rejected"),
        CreateOpenPocketBaseTestClient(Config, Error).IsValid());
    TestEqual(TEXT("The lower bound uses InvalidArgument"), Error.Kind, EOpenPocketBaseErrorKind::InvalidArgument);

    Config.AuthRefreshLeadTimeSeconds = 3601;
    TestFalse(
        TEXT("An excessive Auth Refresh lead time is rejected"),
        CreateOpenPocketBaseTestClient(Config, Error).IsValid());
    TestEqual(TEXT("The upper bound uses InvalidArgument"), Error.Kind, EOpenPocketBaseErrorKind::InvalidArgument);
    return true;
}

#endif
