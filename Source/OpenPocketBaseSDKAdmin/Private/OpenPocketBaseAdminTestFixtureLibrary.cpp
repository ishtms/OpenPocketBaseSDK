#include "OpenPocketBaseAdminTestFixtureLibrary.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenPocketBaseClientConfig.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr int64 MaxFixtureBytes = 16 * 1024;
constexpr int32 MaxFixturePathCharacters = 512;
constexpr int32 MaxIdentityCharacters = 320;
constexpr int32 MaxPasswordCharacters = 4096;

bool Fail(
    FOpenPocketBaseError& Error,
    const EOpenPocketBaseErrorKind Kind,
    const TCHAR* Message)
{
    Error = {};
    Error.Kind = Kind;
    Error.Message = Message;
    return false;
}

bool ContainsControlCharacter(const FString& Value)
{
    for (const TCHAR Character : Value)
    {
        if (FChar::IsControl(Character))
        {
            return true;
        }
    }
    return false;
}

bool IsSafeProjectRelativePath(const FString& Path)
{
    if (Path.IsEmpty() || Path.Len() > MaxFixturePathCharacters ||
        ContainsControlCharacter(Path) || !FPaths::IsRelative(Path) ||
        Path.StartsWith(TEXT("/")) || Path.StartsWith(TEXT("\\")) ||
        Path.Contains(TEXT(":")))
    {
        return false;
    }

    FString StandardPath = Path;
    StandardPath.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
    TArray<FString> Segments;
    StandardPath.ParseIntoArray(Segments, TEXT("/"), false);
    if (Segments.IsEmpty())
    {
        return false;
    }
    for (const FString& Segment : Segments)
    {
        if (Segment.IsEmpty() || Segment == TEXT(".") || Segment == TEXT(".."))
        {
            return false;
        }
    }
    return true;
}

bool IsLoopbackBaseUrl(
    const FString& Value,
    FString& OutNormalized,
    FOpenPocketBaseError& Error)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = Value;
    if (!Config.TryGetNormalizedBaseUrl(OutNormalized, Error))
    {
        Error.Message = TEXT("Admin test credential baseUrl must be a valid loopback HTTP or HTTPS origin.");
        return false;
    }

    const int32 SchemeLength = OutNormalized.StartsWith(TEXT("https://")) ? 8 : 7;
    FString Authority = OutNormalized.Mid(SchemeLength);
    FString Host = Authority;
    if (Host.StartsWith(TEXT("[")))
    {
        int32 ClosingBracket = INDEX_NONE;
        Host.FindChar(TEXT(']'), ClosingBracket);
        Host = ClosingBracket == INDEX_NONE ? FString() : Host.Mid(1, ClosingBracket - 1);
    }
    else
    {
        int32 Colon = INDEX_NONE;
        if (Host.FindLastChar(TEXT(':'), Colon))
        {
            Host.LeftInline(Colon, EAllowShrinking::No);
        }
    }

    if (!Host.Equals(TEXT("localhost"), ESearchCase::IgnoreCase) &&
        Host != TEXT("127.0.0.1") && Host != TEXT("::1"))
    {
        Error = {};
        Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
        Error.Message = TEXT("Admin test credential baseUrl must use localhost or another exact loopback host.");
        return false;
    }
    return true;
}
}

bool UOpenPocketBaseAdminTestFixtureLibrary::LoadAdminTestCredentials(
    FString ProjectRelativePath,
    FOpenPocketBaseAdminTestCredentials& Credentials,
    FOpenPocketBaseError& Error)
{
    Credentials = {};
    Error = {};

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
    return Fail(
        Error,
        EOpenPocketBaseErrorKind::Unsupported,
        TEXT("Admin test credentials are available only in Editor or Development builds."));
#else
    if (ProjectRelativePath.IsEmpty())
    {
        ProjectRelativePath = TEXT(".runtime/admin-credentials.json");
    }
    if (!IsSafeProjectRelativePath(ProjectRelativePath))
    {
        return Fail(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Admin credential Fixture Path must be a safe project-relative path without traversal or control characters."));
    }

    const FString FullPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), ProjectRelativePath));
    const int64 FileSize = IFileManager::Get().FileSize(*FullPath);
    if (FileSize < 0)
    {
        return Fail(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Admin credential fixture could not be read. Create the project-relative JSON file before running privileged tests."));
    }
    if (FileSize > MaxFixtureBytes)
    {
        return Fail(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Admin credential fixture exceeds 16384 bytes. Keep the local test fixture small."));
    }

    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *FullPath))
    {
        return Fail(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Admin credential fixture could not be read. Check its local file permissions."));
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        return Fail(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Admin credential fixture must contain one valid JSON object."));
    }

    FString BaseUrl;
    FString Identity;
    FString Password;
    if (!Root->HasTypedField<EJson::String>(TEXT("baseUrl")) ||
        !Root->TryGetStringField(TEXT("baseUrl"), BaseUrl) || BaseUrl.IsEmpty())
    {
        return Fail(Error, EOpenPocketBaseErrorKind::InvalidArgument, TEXT("Admin credential fixture needs a non-empty baseUrl string."));
    }
    if (!Root->HasTypedField<EJson::String>(TEXT("identity")) ||
        !Root->TryGetStringField(TEXT("identity"), Identity) || Identity.IsEmpty())
    {
        return Fail(Error, EOpenPocketBaseErrorKind::InvalidArgument, TEXT("Admin credential fixture needs a non-empty identity string."));
    }
    if (!Root->HasTypedField<EJson::String>(TEXT("password")) ||
        !Root->TryGetStringField(TEXT("password"), Password) || Password.IsEmpty())
    {
        return Fail(Error, EOpenPocketBaseErrorKind::InvalidArgument, TEXT("Admin credential fixture needs a non-empty password string."));
    }
    if (Identity.Len() > MaxIdentityCharacters || Password.Len() > MaxPasswordCharacters)
    {
        return Fail(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Admin credential identity or password exceeds its supported local test limit."));
    }
    if (ContainsControlCharacter(BaseUrl) || ContainsControlCharacter(Identity) ||
        ContainsControlCharacter(Password))
    {
        return Fail(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Admin credential values must not contain control characters."));
    }

    FString NormalizedBaseUrl;
    if (!IsLoopbackBaseUrl(BaseUrl, NormalizedBaseUrl, Error))
    {
        return false;
    }

    Credentials.BaseUrl = MoveTemp(NormalizedBaseUrl);
    Credentials.Identity = MoveTemp(Identity);
    Credentials.Password = MoveTemp(Password);
    return true;
#endif
}

FString UOpenPocketBaseAdminTestFixtureLibrary::Conv_OpenPocketBaseAdminTestCredentialsToString(
    const FOpenPocketBaseAdminTestCredentials& Credentials)
{
    return FString::Printf(
        TEXT("Open PocketBase Admin Test Credentials\n{\n    \"baseUrl\": \"%s\",\n    \"identity\": \"<redacted>\",\n    \"password\": \"<redacted>\"\n}"),
        *Credentials.BaseUrl);
}
