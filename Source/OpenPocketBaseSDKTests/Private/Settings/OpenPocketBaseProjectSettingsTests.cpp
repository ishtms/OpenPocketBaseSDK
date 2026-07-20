#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "OpenPocketBaseProjectSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseProjectSettingsTest,
    "OpenPocketBase.Settings.ResolveNonSecretProfiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseProjectSettingsTest::RunTest(const FString& Parameters)
{
    const FSoftObjectProperty* ProfileSchema = CastField<FSoftObjectProperty>(
        FOpenPocketBaseProjectProfile::StaticStruct()->FindPropertyByName(TEXT("Schema")));
    TestNotNull(TEXT("Each project profile can select its PocketBase schema"), ProfileSchema);
    if (ProfileSchema != nullptr)
    {
        TestEqual(
            TEXT("Profile schema references use the PocketBase schema asset"),
            ProfileSchema->PropertyClass->GetName(),
            FString(TEXT("OpenPocketBaseSchema")));
    }

    UOpenPocketBaseProjectSettings* Settings = NewObject<UOpenPocketBaseProjectSettings>();
    Settings->DefaultProfile = TEXT("Local");
    FOpenPocketBaseProjectProfile Local;
    Local.Name = TEXT("Local");
    Local.BaseUrl = TEXT("http://127.0.0.1:8090/");
    Local.AcceptLanguage = TEXT("en-US");
    Settings->Profiles.Add(Local);

    FOpenPocketBaseClientConfig Config;
    FOpenPocketBaseError Error;
    TestTrue(TEXT("The default project profile resolves"),
        Settings->TryResolveProfile(NAME_None, Config, Error));
    TestEqual(TEXT("The origin is normalized"),
        Config.BaseUrl, FString(TEXT("http://127.0.0.1:8090")));
    TestEqual(TEXT("The profile name is retained"),
        Config.ProfileName, FString(TEXT("Local")));
    TestEqual(TEXT("Non-secret language settings are retained"),
        Config.AcceptLanguage, FString(TEXT("en-US")));
    TestTrue(TEXT("Project profiles cannot inject default headers"),
        Config.DefaultHeaders.IsEmpty());

    TestTrue(TEXT("A development origin override is validated and applied"),
        Settings->TryResolveProfileWithDevelopmentOverride(
            TEXT("Local"),
            TEXT("https://dev.example.test/"),
            Config,
            Error));
    TestEqual(TEXT("The development origin override is normalized"),
        Config.BaseUrl, FString(TEXT("https://dev.example.test")));

    TestFalse(TEXT("A credential-bearing override is rejected"),
        Settings->TryResolveProfileWithDevelopmentOverride(
            TEXT("Local"),
            TEXT("https://operator:secret@dev.example.test"),
            Config,
            Error));
    TestEqual(TEXT("Credential rejection is typed"),
        Error.Kind, EOpenPocketBaseErrorKind::InvalidArgument);
    TestFalse(TEXT("Credential material is not copied into the error"),
        Error.ServerMessage.Contains(TEXT("operator")) ||
        Error.ServerMessage.Contains(TEXT("secret")));

    Settings->Profiles.Add(Local);
    TestFalse(TEXT("Duplicate profile names are rejected"),
        Settings->TryResolveProfile(TEXT("Local"), Config, Error));
    return true;
}

#endif
