#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenPocketBaseAdminTestFixtureLibrary.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAdminTestFixtureLoaderTest,
    "OpenPocketBase.Admin.TestFixtureLoaderIsBoundedAndRedacted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAdminTestFixtureLoaderTest::RunTest(const FString& Parameters)
{
    const UFunction* LoadFunction = UOpenPocketBaseAdminTestFixtureLibrary::StaticClass()
        ->FindFunctionByName(TEXT("LoadAdminTestCredentials"));
    TestNotNull(TEXT("The admin test credential loader is exposed"), LoadFunction);
    if (LoadFunction != nullptr)
    {
        TestTrue(
            TEXT("The admin test credential loader is development only"),
            LoadFunction->HasMetaData(TEXT("DevelopmentOnly")));
        TestEqual(
            TEXT("The loader exposes explicit success and failure paths"),
            LoadFunction->GetMetaData(TEXT("ExpandBoolAsExecs")),
            FString(TEXT("ReturnValue")));
        TestEqual(
            TEXT("The loader defaults to the private runtime fixture"),
            LoadFunction->GetMetaData(TEXT("CPP_Default_ProjectRelativePath")),
            FString(TEXT(".runtime/admin-credentials.json")));
    }
    const UScriptStruct* CredentialStruct = FOpenPocketBaseAdminTestCredentials::StaticStruct();
    for (const FName PropertyName : {FName(TEXT("BaseUrl")), FName(TEXT("Identity")), FName(TEXT("Password"))})
    {
        const FProperty* Property = FindFProperty<FProperty>(CredentialStruct, PropertyName);
        TestTrue(
            *FString::Printf(TEXT("%s stays transient"), *PropertyName.ToString()),
            Property != nullptr && Property->HasAnyPropertyFlags(CPF_Transient));
    }

    const FString RelativeDirectory = FString::Printf(
        TEXT(".runtime/openpocketbase-admin-fixture-tests/%s"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    const FString FullDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), RelativeDirectory));
    IFileManager::Get().MakeDirectory(*FullDirectory, true);

    const auto WriteFixture = [&](const FString& Name, const FString& Contents)
    {
        return FFileHelper::SaveStringToFile(
            Contents,
            *FPaths::Combine(FullDirectory, Name),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    };
    const auto LoadFixture = [&](const FString& Name,
                                 FOpenPocketBaseAdminTestCredentials& Credentials,
                                 FOpenPocketBaseError& Error)
    {
        return UOpenPocketBaseAdminTestFixtureLibrary::LoadAdminTestCredentials(
            FPaths::Combine(RelativeDirectory, Name),
            Credentials,
            Error);
    };

    const FString ValidJson =
        TEXT("{\"baseUrl\":\"http://127.0.0.1:18094\",\"identity\":\"admin@example.com\",\"password\":\"fixture-secret\"}");
    TestTrue(
        TEXT("A valid fixture is written"),
        WriteFixture(TEXT("valid.json"), ValidJson));

    FOpenPocketBaseAdminTestCredentials Credentials;
    FOpenPocketBaseError Error;
    TestTrue(TEXT("A valid loopback fixture loads"), LoadFixture(TEXT("valid.json"), Credentials, Error));
    TestEqual(TEXT("The fixture URL is preserved"), Credentials.BaseUrl, FString(TEXT("http://127.0.0.1:18094")));
    TestEqual(TEXT("The fixture identity is preserved"), Credentials.Identity, FString(TEXT("admin@example.com")));
    TestEqual(TEXT("The fixture password is preserved"), Credentials.Password, FString(TEXT("fixture-secret")));
    TestFalse(TEXT("A successful load has no error"), Error.IsSet());

    const FString Redacted =
        UOpenPocketBaseAdminTestFixtureLibrary::Conv_OpenPocketBaseAdminTestCredentialsToString(
            Credentials);
    TestTrue(TEXT("The credential conversion marks the identity as redacted"), Redacted.Contains(TEXT("<redacted>")));
    TestFalse(TEXT("The credential conversion hides the identity"), Redacted.Contains(TEXT("admin@example.com")));
    TestFalse(TEXT("The credential conversion hides the password"), Redacted.Contains(TEXT("fixture-secret")));

    const FString ExactMaximumJson =
        ValidJson + FString::ChrN(16384 - ValidJson.Len(), TCHAR(' '));
    TestTrue(
        TEXT("An exact maximum-size fixture is written"),
        WriteFixture(TEXT("exact-maximum.json"), ExactMaximumJson));
    TestTrue(
        TEXT("An exact 16384-byte fixture loads"),
        LoadFixture(TEXT("exact-maximum.json"), Credentials, Error));

    const TArray<FString> ValidLoopbackUrls = {
        TEXT("http://localhost:18094/"),
        TEXT("https://[::1]:18094")};
    for (int32 Index = 0; Index < ValidLoopbackUrls.Num(); ++Index)
    {
        const FString Name = FString::Printf(TEXT("loopback-%d.json"), Index);
        TestTrue(
            *FString::Printf(TEXT("%s is written"), *Name),
            WriteFixture(
                Name,
                FString::Printf(
                    TEXT("{\"baseUrl\":\"%s\",\"identity\":\"admin@example.com\",\"password\":\"fixture-secret\"}"),
                    *ValidLoopbackUrls[Index])));
        TestTrue(
            *FString::Printf(TEXT("%s loads"), *ValidLoopbackUrls[Index]),
            LoadFixture(Name, Credentials, Error));
    }

    const FString ExactFieldMaximumJson = FString::Printf(
        TEXT("{\"baseUrl\":\"http://127.0.0.1:18094\",\"identity\":\"%s\",\"password\":\"%s\"}"),
        *FString::ChrN(320, TCHAR('a')),
        *FString::ChrN(4096, TCHAR('p')));
    TestTrue(
        TEXT("Exact credential field maxima are written"),
        WriteFixture(TEXT("exact-field-maxima.json"), ExactFieldMaximumJson));
    TestTrue(
        TEXT("Exact credential field maxima load"),
        LoadFixture(TEXT("exact-field-maxima.json"), Credentials, Error));

    struct FInvalidFixture
    {
        FString Name;
        FString Contents;
        FString ExpectedMessagePart;
    };
    const TArray<FInvalidFixture> InvalidFixtures = {
        {TEXT("malformed.json"), TEXT("{"), TEXT("valid JSON object")},
        {TEXT("missing-url.json"), TEXT("{\"identity\":\"admin@example.com\",\"password\":\"fixture-secret\"}"), TEXT("baseUrl")},
        {TEXT("missing-identity.json"), TEXT("{\"baseUrl\":\"http://127.0.0.1:18094\",\"password\":\"fixture-secret\"}"), TEXT("identity")},
        {TEXT("missing-password.json"), TEXT("{\"baseUrl\":\"http://127.0.0.1:18094\",\"identity\":\"admin@example.com\"}"), TEXT("password")},
        {TEXT("wrong-type.json"), TEXT("{\"baseUrl\":\"http://127.0.0.1:18094\",\"identity\":7,\"password\":\"fixture-secret\"}"), TEXT("identity")},
        {TEXT("remote.json"), TEXT("{\"baseUrl\":\"https://example.com\",\"identity\":\"admin@example.com\",\"password\":\"fixture-secret\"}"), TEXT("loopback")},
        {TEXT("control.json"), TEXT("{\"baseUrl\":\"http://127.0.0.1:18094\",\"identity\":\"admin\\n@example.com\",\"password\":\"fixture-secret\"}"), TEXT("control")},
        {TEXT("identity-max-plus-one.json"), FString::Printf(TEXT("{\"baseUrl\":\"http://127.0.0.1:18094\",\"identity\":\"%s\",\"password\":\"fixture-secret\"}"), *FString::ChrN(321, TCHAR('a'))), TEXT("supported local test limit")},
        {TEXT("password-max-plus-one.json"), FString::Printf(TEXT("{\"baseUrl\":\"http://127.0.0.1:18094\",\"identity\":\"admin@example.com\",\"password\":\"%s\"}"), *FString::ChrN(4097, TCHAR('p'))), TEXT("supported local test limit")},
        {TEXT("oversized.json"), FString::ChrN(16385, TCHAR('x')), TEXT("16384")}};

    for (const FInvalidFixture& Fixture : InvalidFixtures)
    {
        TestTrue(*FString::Printf(TEXT("%s is written"), *Fixture.Name), WriteFixture(Fixture.Name, Fixture.Contents));
        Credentials.BaseUrl = TEXT("old-url");
        Credentials.Identity = TEXT("old-identity");
        Credentials.Password = TEXT("old-password");
        Error = {};
        TestFalse(*FString::Printf(TEXT("%s is rejected"), *Fixture.Name), LoadFixture(Fixture.Name, Credentials, Error));
        TestEqual(*FString::Printf(TEXT("%s is InvalidArgument"), *Fixture.Name), Error.Kind, EOpenPocketBaseErrorKind::InvalidArgument);
        TestTrue(*FString::Printf(TEXT("%s has a useful error"), *Fixture.Name), Error.Message.Contains(Fixture.ExpectedMessagePart));
        TestFalse(*FString::Printf(TEXT("%s does not leak the password"), *Fixture.Name), Error.Message.Contains(TEXT("fixture-secret")));
        TestTrue(*FString::Printf(TEXT("%s clears BaseUrl"), *Fixture.Name), Credentials.BaseUrl.IsEmpty());
        TestTrue(*FString::Printf(TEXT("%s clears Identity"), *Fixture.Name), Credentials.Identity.IsEmpty());
        TestTrue(*FString::Printf(TEXT("%s clears Password"), *Fixture.Name), Credentials.Password.IsEmpty());
    }

    const TArray<FString> InvalidPaths = {
        TEXT("../admin-credentials.json"),
        TEXT(".runtime/../admin-credentials.json"),
        TEXT("C:/admin-credentials.json"),
        TEXT(".runtime/admin\ncredentials.json"),
        FString::ChrN(513, TCHAR('a'))};
    for (const FString& InvalidPath : InvalidPaths)
    {
        Error = {};
        TestFalse(
            *FString::Printf(TEXT("Unsafe fixture path '%s' is rejected"), *InvalidPath),
            UOpenPocketBaseAdminTestFixtureLibrary::LoadAdminTestCredentials(
                InvalidPath,
                Credentials,
                Error));
        TestEqual(TEXT("Unsafe fixture paths are InvalidArgument"), Error.Kind, EOpenPocketBaseErrorKind::InvalidArgument);
        TestTrue(TEXT("Unsafe fixture paths have a useful error"), Error.Message.Contains(TEXT("project-relative")));
    }

    Error = {};
    TestFalse(TEXT("A missing fixture is rejected"), LoadFixture(TEXT("missing.json"), Credentials, Error));
    TestEqual(TEXT("A missing fixture is InvalidArgument"), Error.Kind, EOpenPocketBaseErrorKind::InvalidArgument);
    TestTrue(TEXT("A missing fixture has a useful error"), Error.Message.Contains(TEXT("could not be read")));

    IFileManager::Get().DeleteDirectory(*FullDirectory, false, true);
    return true;
}

#endif
