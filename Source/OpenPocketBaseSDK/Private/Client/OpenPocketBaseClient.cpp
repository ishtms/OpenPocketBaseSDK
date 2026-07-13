#include "OpenPocketBaseClient.h"

#include "OpenPocketBaseFilter.h"

#include "Async/Async.h"
#include "Clock/OpenPocketBaseClock.h"
#include "Crypto/OpenPocketBaseSha256.h"
#include "Dom/JsonObject.h"
#include "Files/OpenPocketBaseMultipart.h"
#include "Files/OpenPocketBaseDownload.h"
#include "Files/OpenPocketBaseTransferProgress.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformProperties.h"
#include "HAL/CriticalSection.h"
#include "Math/RandomStream.h"
#include "Misc/Base64.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "Misc/SecureHash.h"
#include "Request/OpenPocketBaseRequestState.h"
#include "Realtime/OpenPocketBaseRealtimeManager.h"
#include "Serialization/OpenPocketBaseJson.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Session/OpenPocketBaseSessionEnvelope.h"
#include "Transport/OpenPocketBaseHttpTransport.h"

#include <atomic>

namespace
{
using FOpenPocketBaseResponseHandler = TUniqueFunction<void(
    FOpenPocketBaseHttpResponse&&,
    const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>&)>;

template <typename ValueType>
class TCompletionState final
{
public:
    explicit TCompletionState(TUniqueFunction<void(TOpenPocketBaseResult<ValueType>&&)> InCallback)
        : Callback(MoveTemp(InCallback))
    {
    }

    void Invoke(TOpenPocketBaseResult<ValueType>&& Result)
    {
        if (Callback)
        {
            TUniqueFunction<void(TOpenPocketBaseResult<ValueType>&&)> LocalCallback = MoveTemp(Callback);
            LocalCallback(MoveTemp(Result));
        }
    }

private:
    TUniqueFunction<void(TOpenPocketBaseResult<ValueType>&&)> Callback;
};

FOpenPocketBaseError MakeLocalError(
    const EOpenPocketBaseErrorKind Kind,
    const TCHAR* Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = Kind;
    Error.ServerMessage = Message;
    return Error;
}

FOpenPocketBaseError MakeCancelledError()
{
    return MakeLocalError(EOpenPocketBaseErrorKind::Cancelled, TEXT("The request was cancelled."));
}

FString GetBuildConfigurationName()
{
#if UE_BUILD_SHIPPING
    return TEXT("Shipping");
#elif UE_BUILD_TEST
    return TEXT("Test");
#elif UE_BUILD_DEBUG
    return TEXT("Debug");
#elif UE_BUILD_DEVELOPMENT
    return TEXT("Development");
#else
    return TEXT("Unknown");
#endif
}

FOpenPocketBaseError SanitizeProtectedFileError(FOpenPocketBaseError Error)
{
    Error.ServerMessage = Error.Kind == EOpenPocketBaseErrorKind::Timeout
        ? TEXT("The protected file download timed out.")
        : TEXT("The protected file download failed.");
    Error.ServerCode.Reset();
    Error.FieldErrors.Reset();
    return Error;
}

FOpenPocketBaseError SanitizeOAuthExchangeError(FOpenPocketBaseError Error)
{
    Error.ServerMessage = Error.Kind == EOpenPocketBaseErrorKind::Timeout
        ? TEXT("The OAuth code exchange timed out.")
        : TEXT("The OAuth code exchange failed.");
    Error.ServerCode.Reset();
    Error.FieldErrors.Reset();
    return Error;
}

template <typename ValueType>
void DispatchFailure(
    TUniqueFunction<void(TOpenPocketBaseResult<ValueType>&&)> Callback,
    FOpenPocketBaseError Error)
{
    if (!Callback)
    {
        return;
    }

    AsyncTask(
        ENamedThreads::GameThread,
        [Callback = MoveTemp(Callback), Error = MoveTemp(Error)]() mutable
        {
            Callback(TOpenPocketBaseResult<ValueType>::Failure(MoveTemp(Error)));
        });
}

template <typename ValueType>
void DispatchSuccess(
    TUniqueFunction<void(TOpenPocketBaseResult<ValueType>&&)> Callback,
    ValueType Value)
{
    if (!Callback)
    {
        return;
    }

    AsyncTask(
        ENamedThreads::GameThread,
        [Callback = MoveTemp(Callback), Value = MoveTemp(Value)]() mutable
        {
            Callback(TOpenPocketBaseResult<ValueType>::Success(MoveTemp(Value)));
        });
}

bool IsHeaderName(const FString& Header, const TCHAR* Expected)
{
    return Header.Equals(Expected, ESearchCase::IgnoreCase);
}

bool IsValidHeaderName(const FString& Header)
{
    if (Header.IsEmpty())
    {
        return false;
    }

    for (const TCHAR Character : Header)
    {
        const bool bTokenPunctuation = Character == TEXT('!') || Character == TEXT('#') ||
            Character == TEXT('$') || Character == TEXT('%') || Character == TEXT('&') ||
            Character == TEXT('\'') || Character == TEXT('*') || Character == TEXT('+') ||
            Character == TEXT('-') || Character == TEXT('.') || Character == TEXT('^') ||
            Character == TEXT('_') || Character == TEXT('`') || Character == TEXT('|') ||
            Character == TEXT('~');
        if (!FChar::IsAlnum(Character) && !bTokenPunctuation)
        {
            return false;
        }
    }
    return true;
}

bool IsValidHeaderValue(const FString& Value)
{
    for (const TCHAR Character : Value)
    {
        if (FChar::IsControl(Character))
        {
            return false;
        }
    }
    return true;
}

bool IsProtectedDefaultHeader(const FString& Header)
{
    static const TCHAR* ProtectedHeaders[] = {
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

    for (const TCHAR* ProtectedHeader : ProtectedHeaders)
    {
        if (IsHeaderName(Header, ProtectedHeader))
        {
            return true;
        }
    }
    return false;
}

bool IsProtectedRequestHeader(const FString& Header)
{
    static const TCHAR* ProtectedHeaders[] = {
        TEXT("Connection"),
        TEXT("Keep-Alive"),
        TEXT("TE"),
        TEXT("Traceparent"),
        TEXT("Trailer"),
        TEXT("Transfer-Encoding"),
        TEXT("Upgrade")
    };
    if (IsProtectedDefaultHeader(Header) ||
        Header.StartsWith(TEXT("Sec-"), ESearchCase::IgnoreCase))
    {
        return true;
    }
    for (const TCHAR* ProtectedHeader : ProtectedHeaders)
    {
        if (IsHeaderName(Header, ProtectedHeader))
        {
            return true;
        }
    }
    return false;
}

bool IsValidTraceParent(const FString& Value)
{
    if (Value.IsEmpty())
    {
        return true;
    }
    if (Value.Len() != 55 || !Value.StartsWith(TEXT("00")) ||
        Value[2] != TEXT('-') || Value[35] != TEXT('-') ||
        Value[52] != TEXT('-'))
    {
        return false;
    }
    for (int32 Index = 0; Index < Value.Len(); ++Index)
    {
        if (Index == 2 || Index == 35 || Index == 52)
        {
            continue;
        }
        if (!FChar::IsHexDigit(Value[Index]) ||
            (Value[Index] >= TEXT('A') && Value[Index] <= TEXT('F')))
        {
            return false;
        }
    }
    return Value.Mid(3, 32) != TEXT("00000000000000000000000000000000") &&
        Value.Mid(36, 16) != TEXT("0000000000000000");
}

bool ValidateDefaultHeaders(
    const FOpenPocketBaseClientConfig& Config,
    FOpenPocketBaseError& OutError)
{
    if (Config.AcceptLanguage.Contains(TEXT("\r")) || Config.AcceptLanguage.Contains(TEXT("\n")))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Accept-Language must not contain line breaks."));
        return false;
    }

    for (const TPair<FString, FString>& Header : Config.DefaultHeaders)
    {
        if (!IsValidHeaderName(Header.Key) || !IsValidHeaderValue(Header.Value))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Default headers contain an invalid name or value."));
            return false;
        }

        if (IsProtectedDefaultHeader(Header.Key))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Default headers must not override transport-owned headers."));
            return false;
        }
    }

    return true;
}

FString EncodeSegment(const FString& Value)
{
    return FGenericPlatformHttp::UrlEncode(Value);
}

bool IsSafePathSegment(const FString& Value)
{
    if (Value.IsEmpty() || Value == TEXT(".") || Value == TEXT(".."))
    {
        return false;
    }

    for (const TCHAR Character : Value)
    {
        if (Character == TEXT('/') || Character == TEXT('\\') || Character == TEXT('%') ||
            Character == TEXT('?') || Character == TEXT('#') || FChar::IsControl(Character))
        {
            return false;
        }
    }
    return true;
}

const TCHAR* GetCustomRouteMethod(const EOpenPocketBaseCustomRouteMethod Method)
{
    switch (Method)
    {
    case EOpenPocketBaseCustomRouteMethod::Get:
        return TEXT("GET");
    case EOpenPocketBaseCustomRouteMethod::Post:
        return TEXT("POST");
    case EOpenPocketBaseCustomRouteMethod::Put:
        return TEXT("PUT");
    case EOpenPocketBaseCustomRouteMethod::Patch:
        return TEXT("PATCH");
    case EOpenPocketBaseCustomRouteMethod::Delete:
        return TEXT("DELETE");
    default:
        return nullptr;
    }
}

bool IsBoundedPlainText(const FString& Value, const int32 MaxLength)
{
    if (Value.Len() > MaxLength)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (FChar::IsControl(Character))
        {
            return false;
        }
    }
    return true;
}

bool IsSafeCustomContentType(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() > 127 || !Value.Contains(TEXT("/")))
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        const bool bPunctuation = Character == TEXT('/') || Character == TEXT('-') ||
            Character == TEXT('+') || Character == TEXT('.') || Character == TEXT(';') ||
            Character == TEXT('=') || Character == TEXT(' ');
        if (!FChar::IsAlnum(Character) && !bPunctuation)
        {
            return false;
        }
    }
    return true;
}

bool TryBuildCustomRoutePath(
    const FString& Path,
    const TMap<FString, FString>& Query,
    FString& OutPath,
    FOpenPocketBaseError& OutError)
{
    if (Path.Len() < 2 || Path.Len() > 2048 || !Path.StartsWith(TEXT("/")) ||
        Path.StartsWith(TEXT("//")) || Path.EndsWith(TEXT("/")) ||
        Path.Contains(TEXT("//")) || Path.Contains(TEXT("?")) ||
        Path.Contains(TEXT("#")) || Path.Contains(TEXT("\\")) ||
        Path.Contains(TEXT("%")) || Query.Num() > 64)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("The custom route path or query is invalid."));
        return false;
    }

    TArray<FString> Segments;
    Path.RightChop(1).ParseIntoArray(Segments, TEXT("/"), false);
    if (Segments.IsEmpty() || Segments.Num() > 32)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("The custom route path exceeds the supported segment bound."));
        return false;
    }

    TArray<FString> EncodedSegments;
    EncodedSegments.Reserve(Segments.Num());
    for (const FString& Segment : Segments)
    {
        if (!IsSafePathSegment(Segment) || Segment.Len() > 255)
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("The custom route path contains an invalid segment."));
            return false;
        }
        EncodedSegments.Add(EncodeSegment(Segment));
    }
    OutPath = TEXT("/") + FString::Join(EncodedSegments, TEXT("/"));

    TArray<FString> QueryNames;
    Query.GetKeys(QueryNames);
    QueryNames.Sort();
    TArray<FString> QueryParts;
    QueryParts.Reserve(QueryNames.Num());
    for (const FString& Name : QueryNames)
    {
        const FString* Value = Query.Find(Name);
        if (Value == nullptr || Name.IsEmpty() || !IsBoundedPlainText(Name, 128) ||
            !IsBoundedPlainText(*Value, 4096))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("The custom route query contains an invalid name or value."));
            return false;
        }
        QueryParts.Add(FGenericPlatformHttp::UrlEncode(Name) + TEXT("=") +
            FGenericPlatformHttp::UrlEncode(*Value));
    }
    if (!QueryParts.IsEmpty())
    {
        OutPath += TEXT("?") + FString::Join(QueryParts, TEXT("&"));
    }
    if (OutPath.Len() > 8192)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("The custom route URL exceeds the supported bound."));
        return false;
    }
    OutError = FOpenPocketBaseError();
    return true;
}

bool TrySerializeCustomForm(
    const TMap<FString, FString>& Fields,
    TArray<uint8>& OutBody,
    FOpenPocketBaseError& OutError)
{
    if (Fields.IsEmpty() || Fields.Num() > 128)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("A form body requires a bounded set of fields."));
        return false;
    }
    TArray<FString> Names;
    Fields.GetKeys(Names);
    Names.Sort();
    TArray<FString> Parts;
    Parts.Reserve(Names.Num());
    for (const FString& Name : Names)
    {
        const FString* Value = Fields.Find(Name);
        if (Value == nullptr || Name.IsEmpty() || !IsBoundedPlainText(Name, 128) ||
            !IsBoundedPlainText(*Value, 4096))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("The form body contains an invalid name or value."));
            return false;
        }
        Parts.Add(FGenericPlatformHttp::UrlEncode(Name) + TEXT("=") +
            FGenericPlatformHttp::UrlEncode(*Value));
    }
    const FString Form = FString::Join(Parts, TEXT("&"));
    const FTCHARToUTF8 Converted(*Form);
    OutBody.Reset(Converted.Length());
    OutBody.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    OutError = FOpenPocketBaseError();
    return true;
}

FString FindResponseHeader(
    const FOpenPocketBaseHttpResponse& Response,
    const TCHAR* HeaderName)
{
    for (const TPair<FString, FString>& Header : Response.Headers)
    {
        if (Header.Key.Equals(HeaderName, ESearchCase::IgnoreCase))
        {
            return Header.Value;
        }
    }
    return {};
}

bool TryParseJsonObject(
    const TArray<uint8>& Body,
    TSharedPtr<FJsonObject>& OutObject,
    FString* OutJson = nullptr)
{
    if (Body.IsEmpty())
    {
        return false;
    }
    const FUTF8ToTCHAR Converted(
        reinterpret_cast<const ANSICHAR*>(Body.GetData()),
        Body.Num());
    const FString Json(Converted.Length(), Converted.Get());
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
    {
        return false;
    }
    if (OutJson != nullptr)
    {
        *OutJson = Json;
    }
    return true;
}

bool TryParseJsonValue(
    const TArray<uint8>& Body,
    TSharedPtr<FJsonValue>& OutValue,
    FString& OutJson)
{
    if (Body.IsEmpty())
    {
        return false;
    }
    const FUTF8ToTCHAR Converted(
        reinterpret_cast<const ANSICHAR*>(Body.GetData()),
        Body.Num());
    OutJson = FString(Converted.Length(), Converted.Get());
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(OutJson);
    return FJsonSerializer::Deserialize(Reader, OutValue) && OutValue.IsValid();
}

FOpenPocketBaseError MakeResponseSerializationError(
    const FOpenPocketBaseHttpResponse& Response,
    const TCHAR* Message)
{
    FOpenPocketBaseError Error = MakeLocalError(
        EOpenPocketBaseErrorKind::Serialization,
        Message);
    Error.HttpStatus = Response.HttpStatus;
    Error.RequestId = Response.RequestId;
    return Error;
}

bool IsBoundedTransientAuthId(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() > 256)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (FChar::IsControl(Character) || FChar::IsWhitespace(Character))
        {
            return false;
        }
    }
    return true;
}

void AddQueryValue(TArray<FString>& Parts, const TCHAR* Name, const FString& Value)
{
    if (!Value.IsEmpty())
    {
        Parts.Add(FString::Printf(TEXT("%s=%s"), Name, *FGenericPlatformHttp::UrlEncode(Value)));
    }
}

bool TryGetNormalizedOrigin(const FString& Url, FString& OutOrigin)
{
    const int32 SchemeSeparator = Url.Find(TEXT("://"), ESearchCase::CaseSensitive);
    if (SchemeSeparator <= 0)
    {
        return false;
    }

    const int32 AuthorityStart = SchemeSeparator + 3;
    int32 OriginEnd = Url.Len();
    for (int32 Index = AuthorityStart; Index < Url.Len(); ++Index)
    {
        const TCHAR Character = Url[Index];
        if (Character == TEXT('/') || Character == TEXT('?') || Character == TEXT('#'))
        {
            OriginEnd = Index;
            break;
        }
    }

    FOpenPocketBaseClientConfig OriginConfig;
    OriginConfig.BaseUrl = Url.Left(OriginEnd);
    FOpenPocketBaseError Error;
    if (!OriginConfig.TryGetNormalizedBaseUrl(OutOrigin, Error))
    {
        return false;
    }

    if (OutOrigin.StartsWith(TEXT("https://")) && OutOrigin.EndsWith(TEXT(":443")))
    {
        OutOrigin.LeftChopInline(4, EAllowShrinking::No);
    }
    else if (OutOrigin.StartsWith(TEXT("http://")) && OutOrigin.EndsWith(TEXT(":80")))
    {
        OutOrigin.LeftChopInline(3, EAllowShrinking::No);
    }
    return true;
}

bool HaveSameOrigin(const FString& FirstUrl, const FString& SecondUrl)
{
    FString FirstOrigin;
    FString SecondOrigin;
    return TryGetNormalizedOrigin(FirstUrl, FirstOrigin) &&
        TryGetNormalizedOrigin(SecondUrl, SecondOrigin) &&
        FirstOrigin == SecondOrigin;
}

FString GenerateOAuthRandomToken(const int32 GuidCount)
{
    FString Result;
    Result.Reserve(GuidCount * 32);
    for (int32 Index = 0; Index < GuidCount; ++Index)
    {
        Result += FGuid::NewGuid().ToString(EGuidFormats::Digits);
    }
    return Result;
}

bool TryComputePkceChallenge(const FString& Verifier, FString& OutChallenge)
{
    const FTCHARToUTF8 Utf8(*Verifier);
    uint8 Signature[32] = {};
    OpenPocketBase::Crypto::Sha256(
        MakeArrayView(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length()),
        Signature);
    OutChallenge = FBase64::Encode(Signature, 32, EBase64Mode::UrlSafe);
    while (OutChallenge.EndsWith(TEXT("=")))
    {
        OutChallenge.LeftChopInline(1, EAllowShrinking::No);
    }
    return OutChallenge.Len() == 43;
}

bool IsSafeOAuthValue(const FString& Value, const int32 MaxLength)
{
    if (Value.IsEmpty() || Value.Len() > MaxLength)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (FChar::IsControl(Character))
        {
            return false;
        }
    }
    return true;
}

bool TrySplitHttpUrl(
    const FString& Url,
    FString& OutOrigin,
    FString& OutPath,
    FOpenPocketBaseError& OutError)
{
    if (Url.IsEmpty() || Url.Len() > 8192 || Url.Contains(TEXT("\\")) ||
        Url.Contains(TEXT("#")) || Url.Contains(TEXT("?")))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("OAuth redirect URLs must be bounded HTTP or HTTPS URLs without query or fragment data."));
        return false;
    }
    if (!TryGetNormalizedOrigin(Url, OutOrigin))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("OAuth redirect URLs must contain a valid HTTP or HTTPS origin."));
        return false;
    }

    const int32 SchemeSeparator = Url.Find(TEXT("://"), ESearchCase::CaseSensitive);
    const int32 AuthorityStart = SchemeSeparator + 3;
    const int32 PathStart = Url.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart,
        AuthorityStart);
    OutPath = PathStart == INDEX_NONE ? TEXT("/") : Url.Mid(PathStart);
    OutError = FOpenPocketBaseError();
    return true;
}

bool TryParseQuery(
    const FString& Query,
    TMap<FString, FString>& OutValues)
{
    OutValues.Reset();
    TArray<FString> Parts;
    Query.ParseIntoArray(Parts, TEXT("&"), true);
    if (Parts.Num() > 64)
    {
        return false;
    }
    for (const FString& Part : Parts)
    {
        if (Part.IsEmpty())
        {
            continue;
        }
        FString EncodedName;
        FString EncodedValue;
        if (!Part.Split(TEXT("="), &EncodedName, &EncodedValue))
        {
            EncodedName = Part;
        }
        const FString Name = FGenericPlatformHttp::UrlDecode(EncodedName);
        const FString Value = FGenericPlatformHttp::UrlDecode(EncodedValue);
        if (Name.IsEmpty() || Name.Len() > 128 || Value.Len() > 8192 || OutValues.Contains(Name))
        {
            return false;
        }
        OutValues.Add(Name, Value);
    }
    return true;
}

bool TryBuildOAuthAuthorizationUrl(
    const FString& ProviderUrl,
    const FString& RedirectUrl,
    const FString& State,
    const FString& Challenge,
    const TArray<FString>& Scopes,
    FString& OutUrl,
    FOpenPocketBaseError& OutError)
{
    if (ProviderUrl.IsEmpty() || ProviderUrl.Len() > 8192 ||
        ProviderUrl.Contains(TEXT("#")) || ProviderUrl.Contains(TEXT("\\")))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Serialization,
            TEXT("PocketBase returned an invalid OAuth authorization URL."));
        return false;
    }
    FString ProviderOrigin;
    if (!TryGetNormalizedOrigin(ProviderUrl, ProviderOrigin))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Serialization,
            TEXT("PocketBase returned an OAuth authorization URL without a valid origin."));
        return false;
    }

    FString Base = ProviderUrl;
    FString Query;
    ProviderUrl.Split(TEXT("?"), &Base, &Query);
    TMap<FString, FString> Values;
    if (!TryParseQuery(Query, Values))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Serialization,
            TEXT("PocketBase returned invalid OAuth authorization parameters."));
        return false;
    }
    Values.Add(TEXT("redirect_uri"), RedirectUrl);
    Values.Add(TEXT("state"), State);
    Values.Add(TEXT("code_challenge"), Challenge);
    Values.Add(TEXT("code_challenge_method"), TEXT("S256"));
    if (!Scopes.IsEmpty())
    {
        if (Scopes.Num() > 32)
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("OAuth scopes exceed the supported bound."));
            return false;
        }
        for (const FString& Scope : Scopes)
        {
            if (!IsSafeOAuthValue(Scope, 256) || Scope.Contains(TEXT(" ")))
            {
                OutError = MakeLocalError(
                    EOpenPocketBaseErrorKind::InvalidArgument,
                    TEXT("OAuth scopes contain an invalid value."));
                return false;
            }
        }
        Values.Add(TEXT("scope"), FString::Join(Scopes, TEXT(" ")));
    }

    TArray<FString> Parts;
    Parts.Reserve(Values.Num());
    Values.KeySort([](const FString& First, const FString& Second)
    {
        return First < Second;
    });
    for (const TPair<FString, FString>& Pair : Values)
    {
        Parts.Add(FGenericPlatformHttp::UrlEncode(Pair.Key) + TEXT("=") +
            FGenericPlatformHttp::UrlEncode(Pair.Value));
    }
    OutUrl = Base + TEXT("?") + FString::Join(Parts, TEXT("&"));
    if (OutUrl.Len() > 8192)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("The OAuth authorization URL exceeds the supported bound."));
        return false;
    }
    OutError = FOpenPocketBaseError();
    return true;
}

bool TryBuildAssistedOAuthAuthorizationUrl(
    const FString& ProviderUrl,
    const FString& RedirectUrl,
    const FString& State,
    const TArray<FString>& Scopes,
    FString& OutUrl,
    FOpenPocketBaseError& OutError)
{
    if (ProviderUrl.IsEmpty() || ProviderUrl.Len() > 8192 ||
        ProviderUrl.Contains(TEXT("#")) || ProviderUrl.Contains(TEXT("\\")))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Serialization,
            TEXT("PocketBase returned an invalid OAuth authorization URL."));
        return false;
    }
    FString ProviderOrigin;
    if (!TryGetNormalizedOrigin(ProviderUrl, ProviderOrigin))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Serialization,
            TEXT("PocketBase returned an OAuth authorization URL without a valid origin."));
        return false;
    }

    FString Base = ProviderUrl;
    FString Query;
    ProviderUrl.Split(TEXT("?"), &Base, &Query);
    TMap<FString, FString> Values;
    if (!TryParseQuery(Query, Values))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Serialization,
            TEXT("PocketBase returned invalid OAuth authorization parameters."));
        return false;
    }
    Values.Add(TEXT("redirect_uri"), RedirectUrl);
    Values.Add(TEXT("state"), State);
    if (!Scopes.IsEmpty())
    {
        if (Scopes.Num() > 32)
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("OAuth scopes exceed the supported bound."));
            return false;
        }
        for (const FString& Scope : Scopes)
        {
            if (!IsSafeOAuthValue(Scope, 256) || Scope.Contains(TEXT(" ")))
            {
                OutError = MakeLocalError(
                    EOpenPocketBaseErrorKind::InvalidArgument,
                    TEXT("OAuth scopes contain an invalid value."));
                return false;
            }
        }
        Values.Add(TEXT("scope"), FString::Join(Scopes, TEXT(" ")));
    }

    TArray<FString> Parts;
    Parts.Reserve(Values.Num());
    Values.KeySort([](const FString& First, const FString& Second)
    {
        return First < Second;
    });
    for (const TPair<FString, FString>& Pair : Values)
    {
        Parts.Add(FGenericPlatformHttp::UrlEncode(Pair.Key) + TEXT("=") +
            FGenericPlatformHttp::UrlEncode(Pair.Value));
    }
    OutUrl = Base + TEXT("?") + FString::Join(Parts, TEXT("&"));
    if (OutUrl.Len() > 8192)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("The OAuth authorization URL exceeds the supported bound."));
        return false;
    }
    OutError = FOpenPocketBaseError();
    return true;
}

bool TryValidateOAuthCallback(
    const FString& CallbackUrl,
    const FString& ExpectedRedirectUrl,
    const FString& ExpectedState,
    FString& OutCode,
    FOpenPocketBaseError& OutError)
{
    OutCode.Reset();
    if (CallbackUrl.IsEmpty() || CallbackUrl.Len() > 8192 || CallbackUrl.Contains(TEXT("#")))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Authentication,
            TEXT("The OAuth callback URL is invalid."));
        return false;
    }
    FString CallbackBase;
    FString Query;
    if (!CallbackUrl.Split(TEXT("?"), &CallbackBase, &Query))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Authentication,
            TEXT("The OAuth callback is missing authorization parameters."));
        return false;
    }

    FString ExpectedOrigin;
    FString ExpectedPath;
    FString CallbackOrigin;
    FString CallbackPath;
    FOpenPocketBaseError UrlError;
    if (!TrySplitHttpUrl(ExpectedRedirectUrl, ExpectedOrigin, ExpectedPath, UrlError) ||
        !TrySplitHttpUrl(CallbackBase, CallbackOrigin, CallbackPath, UrlError) ||
        ExpectedOrigin != CallbackOrigin || ExpectedPath != CallbackPath)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Authentication,
            TEXT("The OAuth callback origin or path does not match the requested redirect."));
        return false;
    }

    TMap<FString, FString> Values;
    if (!TryParseQuery(Query, Values))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Authentication,
            TEXT("The OAuth callback parameters are invalid."));
        return false;
    }
    if (Values.FindRef(TEXT("state")) != ExpectedState)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Authentication,
            TEXT("The OAuth callback state does not match the active transaction."));
        return false;
    }
    if (Values.Contains(TEXT("error")))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Authentication,
            TEXT("The OAuth provider rejected the authorization request."));
        return false;
    }
    OutCode = Values.FindRef(TEXT("code"));
    if (!IsSafeOAuthValue(OutCode, 4096))
    {
        OutCode.Reset();
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Authentication,
            TEXT("The OAuth callback did not contain a valid authorization code."));
        return false;
    }
    OutError = FOpenPocketBaseError();
    return true;
}

bool TryDecodeJwtPayload(const FString& Token, TSharedPtr<FJsonObject>& OutPayload)
{
    OutPayload.Reset();
    TArray<FString> Segments;
    Token.ParseIntoArray(Segments, TEXT("."), false);
    if (Segments.Num() != 3 || Segments[1].IsEmpty() || Segments[1].Len() > 16384)
    {
        return false;
    }

    FString EncodedPayload = Segments[1].Replace(TEXT("-"), TEXT("+"));
    EncodedPayload.ReplaceInline(TEXT("_"), TEXT("/"));
    while (EncodedPayload.Len() % 4 != 0)
    {
        EncodedPayload.AppendChar(TEXT('='));
    }

    TArray<uint8> PayloadBytes;
    if (!FBase64::Decode(EncodedPayload, PayloadBytes) || PayloadBytes.IsEmpty() ||
        PayloadBytes.Num() > 8192)
    {
        return false;
    }

    const FUTF8ToTCHAR Converted(
        reinterpret_cast<const ANSICHAR*>(PayloadBytes.GetData()),
        PayloadBytes.Num());
    const FString PayloadJson(Converted.Length(), Converted.Get());
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
    if (!FJsonSerializer::Deserialize(Reader, OutPayload) || !OutPayload.IsValid())
    {
        OutPayload.Reset();
        return false;
    }
    return true;
}

TOptional<int64> TryDecodeJwtExpiry(const FString& Token)
{
    TSharedPtr<FJsonObject> Payload;
    double Expiry = 0;
    if (!TryDecodeJwtPayload(Token, Payload) ||
        !Payload->TryGetNumberField(TEXT("exp"), Expiry) || !FMath::IsFinite(Expiry) ||
        Expiry < 0 || Expiry > static_cast<double>(MAX_int64))
    {
        return {};
    }
    return static_cast<int64>(Expiry);
}

bool TryDecodeJwtIdentity(
    const FString& Token,
    FString& OutRecordId,
    FString& OutCollectionId)
{
    OutRecordId.Reset();
    OutCollectionId.Reset();
    TSharedPtr<FJsonObject> Payload;
    return TryDecodeJwtPayload(Token, Payload) &&
        Payload->TryGetStringField(TEXT("id"), OutRecordId) &&
        Payload->TryGetStringField(TEXT("collectionId"), OutCollectionId) &&
        IsSafePathSegment(OutRecordId) && IsSafePathSegment(OutCollectionId);
}

FString MakeListQuery(const FOpenPocketBaseListOptions& Options)
{
    TArray<FString> Parts;
    Parts.Add(FString::Printf(TEXT("page=%d"), Options.Page));
    Parts.Add(FString::Printf(TEXT("perPage=%d"), Options.PerPage));
    AddQueryValue(Parts, TEXT("filter"), Options.Filter.ToString());
    AddQueryValue(Parts, TEXT("sort"), FString::Join(Options.Sort, TEXT(",")));
    AddQueryValue(Parts, TEXT("expand"), FString::Join(Options.Expand, TEXT(",")));
    AddQueryValue(Parts, TEXT("fields"), FString::Join(Options.Fields, TEXT(",")));
    if (Options.bSkipTotal)
    {
        Parts.Add(TEXT("skipTotal=true"));
    }
    return FString::Join(Parts, TEXT("&"));
}

FString MakeRecordQuery(const FOpenPocketBaseRecordOptions& Options)
{
    TArray<FString> Parts;
    AddQueryValue(Parts, TEXT("expand"), FString::Join(Options.Expand, TEXT(",")));
    AddQueryValue(Parts, TEXT("fields"), FString::Join(Options.Fields, TEXT(",")));
    return FString::Join(Parts, TEXT("&"));
}

FString AddQuery(FString Path, const FString& Query)
{
    if (!Query.IsEmpty())
    {
        Path += TEXT("?") + Query;
    }
    return Path;
}

FString GetBatchMethod(const EOpenPocketBaseBatchOperation Operation)
{
    switch (Operation)
    {
    case EOpenPocketBaseBatchOperation::Create:
        return TEXT("POST");
    case EOpenPocketBaseBatchOperation::Update:
        return TEXT("PATCH");
    case EOpenPocketBaseBatchOperation::Upsert:
        return TEXT("PUT");
    case EOpenPocketBaseBatchOperation::Delete:
        return TEXT("DELETE");
    default:
        return {};
    }
}

FString MakeBatchEntryPath(const FOpenPocketBaseBatchEntry& Entry)
{
    FString Path = FString::Printf(
        TEXT("/api/collections/%s/records"),
        *EncodeSegment(Entry.Collection));
    if (Entry.Operation == EOpenPocketBaseBatchOperation::Update ||
        Entry.Operation == EOpenPocketBaseBatchOperation::Delete)
    {
        Path += TEXT("/") + EncodeSegment(Entry.RecordId);
    }

    FOpenPocketBaseRecordOptions QueryOptions;
    QueryOptions.Expand = Entry.Expand;
    QueryOptions.Fields = Entry.Fields;
    return AddQuery(MoveTemp(Path), MakeRecordQuery(QueryOptions));
}

bool ValidateBatch(
    const FOpenPocketBaseBatchRequest& Batch,
    const FOpenPocketBaseBatchOptions& Options,
    FOpenPocketBaseError& OutError)
{
    if (Options.MaxOperations < 1 || Options.MaxOperations > 50 || Batch.Entries.IsEmpty() ||
        Batch.Entries.Num() > Options.MaxOperations ||
        Options.MaxBodyBytes < 1024 || Options.MaxBodyBytes > 16 * 1024 * 1024 ||
        Options.RequestOptions.TotalTimeoutSeconds <= 0 ||
        Options.RequestOptions.TotalTimeoutSeconds > 120 ||
        Options.RequestOptions.ActivityTimeoutSeconds <= 0 ||
        Options.RequestOptions.ActivityTimeoutSeconds > 120)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Batch count, body, and timeout bounds are invalid."));
        return false;
    }

    for (const FOpenPocketBaseBatchEntry& Entry : Batch.Entries)
    {
        if (!IsSafePathSegment(Entry.Collection))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Every batch entry requires a valid collection."));
            return false;
        }

        const bool bUsesRecordId = Entry.Operation == EOpenPocketBaseBatchOperation::Update ||
            Entry.Operation == EOpenPocketBaseBatchOperation::Delete;
        if (bUsesRecordId && !IsSafePathSegment(Entry.RecordId))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Batch update and delete entries require a valid record ID."));
            return false;
        }

        const bool bUsesBody = Entry.Operation != EOpenPocketBaseBatchOperation::Delete;
        if (bUsesBody && !Entry.Body.Data.JsonObject.IsValid())
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Batch create, update, and upsert entries require a JSON body."));
            return false;
        }
        if (Entry.Operation == EOpenPocketBaseBatchOperation::Upsert)
        {
            FString UpsertId;
            if (!Entry.Body.Data.JsonObject->TryGetStringField(TEXT("id"), UpsertId) ||
                UpsertId.Len() != 15 || !IsSafePathSegment(UpsertId))
            {
                OutError = MakeLocalError(
                    EOpenPocketBaseErrorKind::InvalidArgument,
                    TEXT("Batch upsert bodies require a valid 15-character record ID."));
                return false;
            }
        }
    }
    return true;
}

TArray<uint8> SerializeBatch(const FOpenPocketBaseBatchRequest& Batch)
{
    TArray<TSharedPtr<FJsonValue>> Requests;
    Requests.Reserve(Batch.Entries.Num());
    for (const FOpenPocketBaseBatchEntry& Entry : Batch.Entries)
    {
        const TSharedRef<FJsonObject> RequestObject = MakeShared<FJsonObject>();
        RequestObject->SetStringField(TEXT("method"), GetBatchMethod(Entry.Operation));
        RequestObject->SetStringField(TEXT("url"), MakeBatchEntryPath(Entry));
        if (Entry.Operation != EOpenPocketBaseBatchOperation::Delete)
        {
            RequestObject->SetObjectField(TEXT("body"), Entry.Body.Data.JsonObject);
        }
        Requests.Add(MakeShared<FJsonValueObject>(RequestObject));
    }

    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetArrayField(TEXT("requests"), MoveTemp(Requests));
    return OpenPocketBase::Json::SerializeObject(Root);
}

EOpenPocketBaseRequestState TerminalStateFor(const bool bSucceeded)
{
    return bSucceeded
        ? EOpenPocketBaseRequestState::Succeeded
        : EOpenPocketBaseRequestState::Failed;
}

bool ValidateRequestOptions(
    const FOpenPocketBaseRequestOptions& Options,
    FOpenPocketBaseError& OutError)
{
    bool bRequestKeyValid = Options.RequestKey.Len() <= 128;
    for (const TCHAR Character : Options.RequestKey)
    {
        bRequestKeyValid = bRequestKeyValid && !FChar::IsControl(Character);
    }

    bool bHeadersValid = Options.AdditionalHeaders.Num() <= 32;
    for (const TPair<FString, FString>& Header : Options.AdditionalHeaders)
    {
        bHeadersValid = bHeadersValid && Header.Key.Len() <= 128 &&
            Header.Value.Len() <= 4096 && IsValidHeaderName(Header.Key) &&
            IsValidHeaderValue(Header.Value) && !IsProtectedRequestHeader(Header.Key);
    }

    if (!bRequestKeyValid || !bHeadersValid || !IsValidTraceParent(Options.TraceParent) ||
        Options.TotalTimeoutSeconds < 0 || Options.ActivityTimeoutSeconds < 0 ||
        Options.MaxReadRetries < 0 || Options.MaxReadRetries > 5 ||
        Options.RetryBaseDelaySeconds < 0 || Options.RetryBaseDelaySeconds > 30 ||
        Options.RetryMaxDelaySeconds < 0 || Options.RetryMaxDelaySeconds > 60 ||
        Options.RetryJitterFraction < 0 || Options.RetryJitterFraction > 1 ||
        Options.MaxResponseBytes < 1024 || Options.MaxResponseBytes > 64 * 1024 * 1024)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Request options contain a value outside the supported bounds."));
        return false;
    }
    return true;
}

bool IsRetryableReadResponse(const FOpenPocketBaseHttpResponse& Response)
{
    if (!Response.bTransportSucceeded)
    {
        return true;
    }
    return Response.HttpStatus == 502 || Response.HttpStatus == 503 || Response.HttpStatus == 504;
}

double GetRetryAfterSeconds(const FOpenPocketBaseHttpResponse& Response)
{
    for (const TPair<FString, FString>& Header : Response.Headers)
    {
        if (Header.Key.Equals(TEXT("Retry-After"), ESearchCase::IgnoreCase))
        {
            double Seconds = 0;
            return LexTryParseString(Seconds, *Header.Value) && Seconds >= 0 ? Seconds : 0;
        }
    }
    return 0;
}

class FOpenPocketBaseRequestAttempts final
    : public TSharedFromThis<FOpenPocketBaseRequestAttempts, ESPMode::ThreadSafe>
{
public:
    FOpenPocketBaseRequestAttempts(
        FOpenPocketBaseHttpRequest InRequest,
        const FOpenPocketBaseRequestOptions& InOptions,
        const bool bInEligibleRead,
        TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> InTransport,
        TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> InClock,
        TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> InState,
        FOpenPocketBaseResponseHandler InHandler,
        FOpenPocketBaseHttpChunkCallback InOnChunk)
        : Request(MoveTemp(InRequest))
        , Options(InOptions)
        , bEligibleRead(bInEligibleRead)
        , Transport(InTransport)
        , Clock(MoveTemp(InClock))
        , State(MoveTemp(InState))
        , Handler(MoveTemp(InHandler))
        , OnChunk(MoveTemp(InOnChunk))
    {
    }

    void Start(const bool bStateAlreadySending = false)
    {
        if (!bStateAlreadySending && !State->TryMarkSending())
        {
            return;
        }

        const uint32 Generation = NextGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
        const TSharedRef<FOpenPocketBaseRequestAttempts, ESPMode::ThreadSafe> Self = AsShared();
        const TSharedPtr<IOpenPocketBaseTransport, ESPMode::ThreadSafe> PinnedTransport = Transport.Pin();
        if (!PinnedTransport.IsValid())
        {
            FOpenPocketBaseHttpResponse Response;
            Response.RequestId = Request.RequestId;
            Response.ErrorMessage = TEXT("The transport became unavailable.");
            HandleResponse(MoveTemp(Response), Generation);
            return;
        }

        FOpenPocketBaseHttpRequest AttemptRequest = Request;
        FOpenPocketBaseHttpChunkCallback ChunkCallback;
        if (OnChunk)
        {
            ChunkCallback = [Self, Generation](const TArrayView<const uint8> Chunk)
            {
                Self->HandleChunk(Chunk, Generation);
            };
        }
        FOpenPocketBaseTransportHandle Handle = PinnedTransport->Send(
            MoveTemp(AttemptRequest),
            MoveTemp(ChunkCallback),
            [Self, Generation](FOpenPocketBaseHttpResponse&& Response)
            {
                Self->HandleResponse(MoveTemp(Response), Generation);
            });
        State->AttachTransportHandle(MoveTemp(Handle));
    }

private:
    void HandleChunk(const TArrayView<const uint8> Chunk, const uint32 Generation)
    {
        if (Generation == NextGeneration.load(std::memory_order_acquire) &&
            State->GetState() == EOpenPocketBaseRequestState::Sending && OnChunk)
        {
            OnChunk(Chunk);
        }
    }

    void HandleResponse(FOpenPocketBaseHttpResponse&& Response, const uint32 Generation)
    {
        if (Generation != NextGeneration.load(std::memory_order_acquire) ||
            State->GetState() != EOpenPocketBaseRequestState::Sending)
        {
            return;
        }

        if (Response.RequestId.IsEmpty())
        {
            Response.RequestId = Request.RequestId;
        }

        if (Response.EffectiveUrl.IsEmpty())
        {
            Response.EffectiveUrl = Request.Url;
        }

        const bool bRejectedRedirect = !HaveSameOrigin(Request.Url, Response.EffectiveUrl);
        if (bRejectedRedirect)
        {
            Response = FOpenPocketBaseHttpResponse();
            Response.RequestId = Request.RequestId;
            Response.ErrorMessage = TEXT("The HTTP response used a disallowed redirect origin.");
        }

        const bool bExceededResponseLimit = Response.Body.Num() > Options.MaxResponseBytes;
        if (bExceededResponseLimit)
        {
            Response = FOpenPocketBaseHttpResponse();
            Response.RequestId = Request.RequestId;
            Response.ErrorMessage = TEXT("The response exceeded the configured byte limit.");
        }

        if (!bRejectedRedirect && !bExceededResponseLimit && bEligibleRead && Options.bRetryEligibleReads &&
            RetryCount < Options.MaxReadRetries && IsRetryableReadResponse(Response))
        {
            if (State->TryMarkWaitingForRetry())
            {
                const double RetryAfterSeconds = GetRetryAfterSeconds(Response);
                ScheduleRetry(RetryAfterSeconds);
            }
            return;
        }

        FOpenPocketBaseResponseHandler LocalHandler = MoveTemp(Handler);
        if (LocalHandler)
        {
            LocalHandler(MoveTemp(Response), State);
        }
    }

    void ScheduleRetry(const double RetryAfterSeconds)
    {
        const int32 RetryIndex = RetryCount++;
        const double ExponentialDelay = Options.RetryBaseDelaySeconds * FMath::Pow(2.0, RetryIndex);
        double Delay = FMath::Min(
            Options.RetryMaxDelaySeconds,
            FMath::Max(ExponentialDelay, RetryAfterSeconds));
        if (Delay > 0 && Options.RetryJitterFraction > 0)
        {
            FRandomStream RandomStream(GetTypeHash(Request.RequestId) + RetryIndex);
            const double Jitter = Delay * Options.RetryJitterFraction;
            Delay = FMath::Clamp(
                Delay + RandomStream.FRandRange(-Jitter, Jitter),
                0.0,
                Options.RetryMaxDelaySeconds);
        }

        const TSharedRef<FOpenPocketBaseRequestAttempts, ESPMode::ThreadSafe> Self = AsShared();
        FOpenPocketBaseClockHandle ClockHandle = Clock->Schedule(
            Delay,
            [Self]()
            {
                Self->Start();
            });
        State->AttachRetryHandle(FOpenPocketBaseTransportHandle(
            [ClockHandle = MoveTemp(ClockHandle)]() mutable
            {
                ClockHandle.Cancel();
            }));
    }

    FOpenPocketBaseHttpRequest Request;
    FOpenPocketBaseRequestOptions Options;
    bool bEligibleRead = false;
    TWeakPtr<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport;
    TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock;
    TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> State;
    FOpenPocketBaseResponseHandler Handler;
    FOpenPocketBaseHttpChunkCallback OnChunk;
    std::atomic<uint32> NextGeneration = 0;
    int32 RetryCount = 0;
};
}

struct FOpenPocketBaseClient::FImpl
{
    class FSessionEventQueue final
        : public TSharedFromThis<FSessionEventQueue, ESPMode::ThreadSafe>
    {
    public:
        void Enqueue(FOpenPocketBaseSessionSnapshot Snapshot)
        {
            bool bScheduleDrain = false;
            {
                FScopeLock Lock(&Mutex);
                Pending.Add(MoveTemp(Snapshot));
                if (!bDrainScheduled)
                {
                    bDrainScheduled = true;
                    bScheduleDrain = true;
                }
            }

            if (bScheduleDrain)
            {
                const TSharedRef<FSessionEventQueue, ESPMode::ThreadSafe> Self = AsShared();
                AsyncTask(
                    ENamedThreads::GameThread,
                    [Self]()
                    {
                        Self->Drain();
                    });
            }
        }

        FOpenPocketBaseSessionChanged Changed;

    private:
        void Drain()
        {
            while (true)
            {
                TArray<FOpenPocketBaseSessionSnapshot> LocalEvents;
                {
                    FScopeLock Lock(&Mutex);
                    if (Pending.IsEmpty())
                    {
                        bDrainScheduled = false;
                        return;
                    }
                    LocalEvents = MoveTemp(Pending);
                    Pending.Reset();
                }

                for (const FOpenPocketBaseSessionSnapshot& Event : LocalEvents)
                {
                    Changed.Broadcast(Event);
                }
            }
        }

        FCriticalSection Mutex;
        TArray<FOpenPocketBaseSessionSnapshot> Pending;
        bool bDrainScheduled = false;
    };

    struct FRefreshWaiter
    {
        TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> State;
        TSharedRef<TCompletionState<FOpenPocketBaseAuthResult>, ESPMode::ThreadSafe> Completion;
    };

    struct FRefreshFlight
    {
        int64 CapturedGeneration = 0;
        FString CapturedToken;
        FString AuthCollection;
        TArray<FRefreshWaiter> Waiters;
        FOpenPocketBaseRequestHandle ChildHandle;
    };

    struct FOAuthTransaction
    {
        FString AuthCollection;
        FString Provider;
        FString RedirectUrl;
        FString State;
        FString CodeVerifier;
        double ExpiresAtMonotonicSeconds = 0;
    };

    class FAuthorizedRequestOperation final
        : public TSharedFromThis<FAuthorizedRequestOperation, ESPMode::ThreadSafe>
    {
    public:
        FAuthorizedRequestOperation(
            FImpl* InImpl,
            TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient,
            FOpenPocketBaseHttpRequest InRequest,
            FOpenPocketBaseRequestOptions InOptions,
            const bool bInEligibleRead,
            TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> InState,
            FOpenPocketBaseResponseHandler InHandler)
            : Impl(InImpl)
            , Client(MoveTemp(InClient))
            , Request(MoveTemp(InRequest))
            , Options(MoveTemp(InOptions))
            , bEligibleRead(bInEligibleRead)
            , State(MoveTemp(InState))
            , Handler(MoveTemp(InHandler))
        {
            Impl->GetCurrentAuthIdentity(InitialAuthGeneration, InitialAuthToken);
            SentAuthGeneration = InitialAuthGeneration;
            SentAuthToken = InitialAuthToken;
        }

        void Start()
        {
            if (Request.Headers.FindRef(TEXT("Authorization")) != InitialAuthToken ||
                !Impl->IsAuthIdentityCurrent(InitialAuthGeneration, InitialAuthToken))
            {
                State->Cancel();
                return;
            }
            if (Impl->ShouldProactivelyRefresh(Request.Headers.FindRef(TEXT("Authorization"))))
            {
                if (State->TryMarkWaitingForAuthRefresh())
                {
                    BeginRefresh(true);
                }
                return;
            }
            StartRequest(false);
        }

    private:
        void BeginRefresh(const bool bInProactive)
        {
            bProactiveRefresh = bInProactive;
            const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
            if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
            {
                State->Cancel();
                return;
            }

            const TSharedRef<FAuthorizedRequestOperation, ESPMode::ThreadSafe> Self = AsShared();
            const FOpenPocketBaseRequestHandle RefreshHandle = PinnedClient->RefreshAuth(
                [Self](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result)
                {
                    Self->HandleRefresh(MoveTemp(Result));
                },
                Options);
            State->AttachAuthRefreshHandle(FOpenPocketBaseTransportHandle(
                [RefreshHandle]()
                {
                    RefreshHandle.Cancel();
                }));
        }

        void HandleRefresh(TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result)
        {
            if (State->GetState() != EOpenPocketBaseRequestState::WaitingForAuthRefresh)
            {
                return;
            }

            if (bProactiveRefresh)
            {
                if (!Result.IsSuccess() &&
                    !Impl->IsAuthIdentityCurrent(InitialAuthGeneration, InitialAuthToken))
                {
                    State->Cancel();
                    return;
                }

                if (!State->TryMarkSending())
                {
                    return;
                }
                Impl->ApplyCurrentAuthorization(Request, SentAuthGeneration, SentAuthToken);
                StartRequest(true);
                return;
            }

            if (!State->TryMarkSending())
            {
                return;
            }
            if (Result.IsSuccess())
            {
                Impl->ApplyCurrentAuthorization(Request, SentAuthGeneration, SentAuthToken);
                StartRequest(true);
                return;
            }
            FinishWithResponse(MoveTemp(AuthRejectedResponse));
        }

        void StartRequest(const bool bStateAlreadySending)
        {
            const TSharedRef<FAuthorizedRequestOperation, ESPMode::ThreadSafe> Self = AsShared();
            Impl->StartAttempts(
                Request,
                Options,
                bEligibleRead,
                State,
                [Self](
                    FOpenPocketBaseHttpResponse&& Response,
                    const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>&)
                {
                    Self->HandleResponse(MoveTemp(Response));
                },
                bStateAlreadySending);
        }

        void HandleResponse(FOpenPocketBaseHttpResponse&& Response)
        {
            const bool bAuthRejected = Response.bTransportSucceeded &&
                (Response.HttpStatus == 401 || Response.HttpStatus == 403);
            if (bEligibleRead && !bDidAuthReplay &&
                Impl->bAuthRefreshAllowed &&
                Impl->Config.bRetryEligibleReadsAfterAuthRefresh && bAuthRejected &&
                Impl->IsAuthIdentityCurrent(SentAuthGeneration, SentAuthToken))
            {
                bDidAuthReplay = true;
                AuthRejectedResponse = MoveTemp(Response);
                if (State->TryMarkWaitingForAuthRefresh())
                {
                    BeginRefresh(false);
                }
                return;
            }
            FinishWithResponse(MoveTemp(Response));
        }

        void FinishWithResponse(FOpenPocketBaseHttpResponse&& Response)
        {
            FOpenPocketBaseResponseHandler LocalHandler = MoveTemp(Handler);
            if (LocalHandler)
            {
                LocalHandler(MoveTemp(Response), State);
            }
        }

        FImpl* Impl;
        TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
        FOpenPocketBaseHttpRequest Request;
        FOpenPocketBaseRequestOptions Options;
        bool bEligibleRead = false;
        TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> State;
        FOpenPocketBaseResponseHandler Handler;
        FOpenPocketBaseHttpResponse AuthRejectedResponse;
        FString InitialAuthToken;
        FString SentAuthToken;
        int64 InitialAuthGeneration = 0;
        int64 SentAuthGeneration = 0;
        bool bProactiveRefresh = false;
        bool bDidAuthReplay = false;
    };

    FOpenPocketBaseClientConfig Config;
    FString BaseUrl;
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport;
    TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore;
    TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock;
    TSharedRef<IOpenPocketBaseOAuthBrowser, ESPMode::ThreadSafe> OAuthBrowser;
    TSharedPtr<OpenPocketBase::Realtime::FConnectionManager, ESPMode::ThreadSafe> Realtime;
    TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Owner;
    FString SecureStorageKey;
    std::atomic<bool> bShutdown = false;
    std::atomic<uint64> NextRequestId = 1;
    mutable FCriticalSection RequestsMutex;
    TMap<uint64, TWeakPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>> Requests;
    TMap<FString, TWeakPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>> RequestKeys;
    mutable FCriticalSection AuthMutex;
    FString AuthToken;
    FOpenPocketBaseRecord AuthRecord;
    FString AuthCollection;
    TOptional<int64> AuthExpiryUnixSeconds;
    int64 AuthGeneration = 0;
    bool bHasAuthRecord = false;
    bool bAuthRefreshAllowed = true;
    EOpenPocketBaseSessionPersistenceState PersistenceState =
        EOpenPocketBaseSessionPersistenceState::MemoryOnly;
    TSharedRef<FSessionEventQueue, ESPMode::ThreadSafe> SessionEvents =
        MakeShared<FSessionEventQueue, ESPMode::ThreadSafe>();
    mutable FCriticalSection RefreshMutex;
    TSharedPtr<FRefreshFlight, ESPMode::ThreadSafe> ActiveRefresh;
    mutable FCriticalSection OAuthMutex;
    TMap<FString, FOAuthTransaction> OAuthTransactions;

    FImpl(
        FOpenPocketBaseClientConfig InConfig,
        FString InBaseUrl,
        TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> InTransport,
        TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> InSecureStore,
        TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> InClock,
        TSharedRef<IOpenPocketBaseOAuthBrowser, ESPMode::ThreadSafe> InOAuthBrowser)
        : Config(MoveTemp(InConfig))
        , BaseUrl(MoveTemp(InBaseUrl))
        , Transport(MoveTemp(InTransport))
        , SecureStore(MoveTemp(InSecureStore))
        , Clock(MoveTemp(InClock))
        , OAuthBrowser(MoveTemp(InOAuthBrowser))
    {
        const FString Binding = BaseUrl + TEXT("|") + Config.ProfileName;
        SecureStorageKey = TEXT("openpocketbase.session.") + FMD5::HashAnsiString(*Binding);
        Realtime = MakeShared<OpenPocketBase::Realtime::FConnectionManager, ESPMode::ThreadSafe>(
            BaseUrl,
            Config.DefaultHeaders,
            Config.AcceptLanguage,
            Transport,
            Clock,
            [this]()
            {
                FScopeLock Lock(&AuthMutex);
                return bHasAuthRecord ? AuthToken : FString();
            },
            [this]()
            {
                return !bShutdown.load(std::memory_order_acquire) && Owner.IsValid();
            });
    }

    void RemoveExpiredOAuthTransactionsLocked(const double Now)
    {
        for (auto It = OAuthTransactions.CreateIterator(); It; ++It)
        {
            if (It.Value().ExpiresAtMonotonicSeconds <= Now)
            {
                It.RemoveCurrent();
            }
        }
    }

    bool TryCreateOAuthTransaction(
        const FString& TransactionAuthCollection,
        const FOpenPocketBaseOAuthProvider& Provider,
        const FOpenPocketBaseOAuth2StartOptions& Options,
        FOpenPocketBaseOAuth2Authorization& OutAuthorization,
        FOpenPocketBaseError& OutError)
    {
        OutAuthorization = FOpenPocketBaseOAuth2Authorization();
        if (bShutdown.load(std::memory_order_acquire) ||
            !IsSafePathSegment(TransactionAuthCollection) ||
            !IsSafeOAuthValue(Provider.Name, 128) ||
            Provider.Name != Options.Provider ||
            !IsSafeOAuthValue(Options.RedirectUrl, 8192))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A live client, valid auth collection, provider, and redirect URL are required."));
            return false;
        }

        FString RedirectOrigin;
        FString RedirectPath;
        if (!TrySplitHttpUrl(Options.RedirectUrl, RedirectOrigin, RedirectPath, OutError))
        {
            return false;
        }

        const FString CodeVerifier = GenerateOAuthRandomToken(3);
        const FString State = GenerateOAuthRandomToken(2);
        FString CodeChallenge;
        if (!TryComputePkceChallenge(CodeVerifier, CodeChallenge))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::Serialization,
                TEXT("The OAuth PKCE challenge could not be generated."));
            return false;
        }

        FString AuthorizationUrl;
        if (!TryBuildOAuthAuthorizationUrl(
                Provider.AuthUrl,
                Options.RedirectUrl,
                State,
                CodeChallenge,
                Options.Scopes,
                AuthorizationUrl,
                OutError))
        {
            return false;
        }

        const double Now = Clock->MonotonicSeconds();
        constexpr double LifetimeSeconds = 300;
        FString TransactionId;
        {
            FScopeLock Lock(&OAuthMutex);
            if (bShutdown.load(std::memory_order_acquire))
            {
                OutError = MakeCancelledError();
                return false;
            }
            RemoveExpiredOAuthTransactionsLocked(Now);
            if (OAuthTransactions.Num() >= 16)
            {
                OutError = MakeLocalError(
                    EOpenPocketBaseErrorKind::Authentication,
                    TEXT("The maximum number of active OAuth transactions has been reached."));
                return false;
            }

            for (int32 Attempt = 0; Attempt < 4; ++Attempt)
            {
                TransactionId = GenerateOAuthRandomToken(1);
                if (!OAuthTransactions.Contains(TransactionId))
                {
                    break;
                }
                TransactionId.Reset();
            }
            if (TransactionId.IsEmpty())
            {
                OutError = MakeLocalError(
                    EOpenPocketBaseErrorKind::Authentication,
                    TEXT("A unique OAuth transaction could not be created."));
                return false;
            }

            FOAuthTransaction Transaction;
            Transaction.AuthCollection = TransactionAuthCollection;
            Transaction.Provider = Provider.Name;
            Transaction.RedirectUrl = Options.RedirectUrl;
            Transaction.State = State;
            Transaction.CodeVerifier = CodeVerifier;
            Transaction.ExpiresAtMonotonicSeconds = Now + LifetimeSeconds;
            OAuthTransactions.Add(TransactionId, MoveTemp(Transaction));
        }

        OutAuthorization.TransactionId = TransactionId;
        OutAuthorization.Provider = Provider.Name;
        OutAuthorization.AuthorizationUrl = MoveTemp(AuthorizationUrl);
        OutAuthorization.RedirectUrl = Options.RedirectUrl;
        OutAuthorization.State = State;
        OutAuthorization.CodeChallenge = MoveTemp(CodeChallenge);
        OutAuthorization.CodeChallengeMethod = TEXT("S256");
        OutAuthorization.ExpiresAtUtc = Clock->UtcNow() + FTimespan::FromSeconds(LifetimeSeconds);
        OutError = FOpenPocketBaseError();
        return true;
    }

    bool TryConsumeOAuthTransaction(
        const FString& ExpectedAuthCollection,
        const FOpenPocketBaseOAuth2Callback& Callback,
        FOAuthTransaction& OutTransaction,
        FString& OutCode,
        FOpenPocketBaseError& OutError)
    {
        OutTransaction = FOAuthTransaction();
        OutCode.Reset();
        if (bShutdown.load(std::memory_order_acquire))
        {
            OutError = MakeCancelledError();
            return false;
        }
        if (!IsSafePathSegment(ExpectedAuthCollection) ||
            !IsBoundedTransientAuthId(Callback.TransactionId))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A valid auth collection and OAuth transaction ID are required."));
            return false;
        }

        FScopeLock Lock(&OAuthMutex);
        if (bShutdown.load(std::memory_order_acquire))
        {
            OutError = MakeCancelledError();
            return false;
        }
        FOAuthTransaction* Transaction = OAuthTransactions.Find(Callback.TransactionId);
        if (Transaction == nullptr)
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("The OAuth transaction is not active."));
            return false;
        }
        if (Transaction->ExpiresAtMonotonicSeconds <= Clock->MonotonicSeconds())
        {
            OAuthTransactions.Remove(Callback.TransactionId);
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("The OAuth transaction expired."));
            return false;
        }
        if (Transaction->AuthCollection != ExpectedAuthCollection)
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("The OAuth transaction belongs to another auth collection."));
            return false;
        }
        if (!TryValidateOAuthCallback(
                Callback.CallbackUrl,
                Transaction->RedirectUrl,
                Transaction->State,
                OutCode,
                OutError))
        {
            return false;
        }

        OutTransaction = MoveTemp(*Transaction);
        OAuthTransactions.Remove(Callback.TransactionId);
        OutError = FOpenPocketBaseError();
        return true;
    }

    bool TryCreateAssistedOAuthTransaction(
        const FString& TransactionAuthCollection,
        const FOpenPocketBaseOAuthProvider& Provider,
        const TArray<FString>& Scopes,
        const FString& RealtimeClientId,
        FString& OutTransactionId,
        FString& OutAuthorizationUrl,
        FOpenPocketBaseError& OutError)
    {
        OutTransactionId.Reset();
        OutAuthorizationUrl.Reset();
        if (!Config.bEnableAssistedOAuth || bShutdown.load(std::memory_order_acquire) ||
            !IsSafePathSegment(TransactionAuthCollection) ||
            !IsSafeOAuthValue(Provider.Name, 128) ||
            !IsSafeOAuthValue(Provider.CodeVerifier, 4096) ||
            !IsBoundedTransientAuthId(RealtimeClientId))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A configured client, provider, verifier, and realtime identity are required."));
            return false;
        }

        const FString RedirectUrl = BaseUrl + TEXT("/api/oauth2-redirect");
        if (!TryBuildAssistedOAuthAuthorizationUrl(
                Provider.AuthUrl,
                RedirectUrl,
                RealtimeClientId,
                Scopes,
                OutAuthorizationUrl,
                OutError))
        {
            return false;
        }

        const double Now = Clock->MonotonicSeconds();
        constexpr double LifetimeSeconds = 300;
        FScopeLock Lock(&OAuthMutex);
        if (bShutdown.load(std::memory_order_acquire))
        {
            OutError = MakeCancelledError();
            return false;
        }
        RemoveExpiredOAuthTransactionsLocked(Now);
        if (OAuthTransactions.Num() >= 16)
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("The maximum number of active OAuth transactions has been reached."));
            return false;
        }
        for (int32 Attempt = 0; Attempt < 4; ++Attempt)
        {
            OutTransactionId = GenerateOAuthRandomToken(1);
            if (!OAuthTransactions.Contains(OutTransactionId))
            {
                break;
            }
            OutTransactionId.Reset();
        }
        if (OutTransactionId.IsEmpty())
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("A unique OAuth transaction could not be created."));
            return false;
        }

        FOAuthTransaction Transaction;
        Transaction.AuthCollection = TransactionAuthCollection;
        Transaction.Provider = Provider.Name;
        Transaction.RedirectUrl = RedirectUrl;
        Transaction.State = RealtimeClientId;
        Transaction.CodeVerifier = Provider.CodeVerifier;
        Transaction.ExpiresAtMonotonicSeconds = Now + LifetimeSeconds;
        OAuthTransactions.Add(OutTransactionId, MoveTemp(Transaction));
        OutError = FOpenPocketBaseError();
        return true;
    }

    void CancelOAuthTransaction(const FString& TransactionId)
    {
        FScopeLock Lock(&OAuthMutex);
        OAuthTransactions.Remove(TransactionId);
    }

    void GetCurrentAuthIdentity(int64& OutGeneration, FString& OutToken) const
    {
        FScopeLock Lock(&AuthMutex);
        OutGeneration = AuthGeneration;
        OutToken = AuthToken;
    }

    bool IsAuthIdentityCurrent(
        const int64 ExpectedGeneration,
        const FString& ExpectedToken) const
    {
        FScopeLock Lock(&AuthMutex);
        return bHasAuthRecord && AuthGeneration == ExpectedGeneration &&
            !ExpectedToken.IsEmpty() && AuthToken == ExpectedToken;
    }

    bool ShouldProactivelyRefresh(const FString& RequestToken) const
    {
        if (!bAuthRefreshAllowed || !Config.bProactiveAuthRefresh || RequestToken.IsEmpty())
        {
            return false;
        }

        FScopeLock Lock(&AuthMutex);
        return bHasAuthRecord && RequestToken == AuthToken && AuthExpiryUnixSeconds.IsSet() &&
            Clock->UtcNow().ToUnixTimestamp() + Config.AuthRefreshLeadTimeSeconds >=
                static_cast<double>(AuthExpiryUnixSeconds.GetValue());
    }

    void ApplyCurrentAuthorization(
        FOpenPocketBaseHttpRequest& Request,
        int64& OutGeneration,
        FString& OutToken) const
    {
        FScopeLock Lock(&AuthMutex);
        OutGeneration = AuthGeneration;
        OutToken = AuthToken;
        if (bHasAuthRecord && !AuthToken.IsEmpty())
        {
            Request.Headers.Add(TEXT("Authorization"), AuthToken);
        }
        else
        {
            Request.Headers.Remove(TEXT("Authorization"));
        }
    }

    void StartAttempts(
        const FOpenPocketBaseHttpRequest& Request,
        const FOpenPocketBaseRequestOptions& Options,
        const bool bEligibleRead,
        const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State,
        FOpenPocketBaseResponseHandler Handler,
        const bool bStateAlreadySending = false,
        FOpenPocketBaseHttpChunkCallback OnChunk = {})
    {
        const TSharedRef<FOpenPocketBaseRequestAttempts, ESPMode::ThreadSafe> Attempts =
            MakeShared<FOpenPocketBaseRequestAttempts, ESPMode::ThreadSafe>(
                Request,
                Options,
                bEligibleRead,
                Transport,
                Clock,
                State,
                MoveTemp(Handler),
                MoveTemp(OnChunk));
        Attempts->Start(bStateAlreadySending);
    }

    FOpenPocketBaseHttpRequest MakeRequest(
        FString Method,
        FString Path,
        TArray<uint8> Body,
        const FOpenPocketBaseRequestOptions& Options,
        const bool bUseAuth) const
    {
        FOpenPocketBaseHttpRequest Request;
        Request.Method = MoveTemp(Method);
        Request.Url = BaseUrl + MoveTemp(Path);
        Request.Body = MoveTemp(Body);
        Request.RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
        Request.TotalTimeoutSeconds = Options.TotalTimeoutSeconds;
        Request.ActivityTimeoutSeconds = Options.ActivityTimeoutSeconds;
        Request.Headers = Config.DefaultHeaders;
        Request.Headers.Add(TEXT("X-Request-Id"), Request.RequestId);
        Request.Headers.Add(TEXT("Accept"), TEXT("application/json"));
        if (!Config.AcceptLanguage.IsEmpty())
        {
            Request.Headers.Add(TEXT("Accept-Language"), Config.AcceptLanguage);
        }
        for (const TPair<FString, FString>& Header : Options.AdditionalHeaders)
        {
            Request.Headers.Add(Header.Key, Header.Value);
        }
        if (!Options.TraceParent.IsEmpty())
        {
            Request.Headers.Add(TEXT("traceparent"), Options.TraceParent);
        }
        if (!Request.Body.IsEmpty())
        {
            Request.Headers.Add(TEXT("Content-Type"), TEXT("application/json"));
        }

        if (bUseAuth)
        {
            FScopeLock Lock(&AuthMutex);
            if (!AuthToken.IsEmpty())
            {
                Request.Headers.Add(TEXT("Authorization"), AuthToken);
            }
        }
        return Request;
    }

    FOpenPocketBaseRequestHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        const FOpenPocketBaseRequestOptions& Options,
        const bool bEligibleRead,
        FOpenPocketBaseResponseHandler Handler,
        TUniqueFunction<void()> OnCancelled,
        const bool bCoordinateAuth = true,
        FOpenPocketBaseHttpChunkCallback OnChunk = {})
    {
        const uint64 RequestId = NextRequestId.fetch_add(1, std::memory_order_relaxed);
        const FString RequestKey = bEligibleRead && Options.bCancelPreviousRequestWithSameKey
            ? Options.RequestKey
            : FString();
        const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> State =
            MakeShared<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>(
                RequestId,
                MoveTemp(OnCancelled),
                [this, RequestId, RequestKey]()
                {
                    FScopeLock Lock(&RequestsMutex);
                    Requests.Remove(RequestId);
                    if (!RequestKey.IsEmpty())
                    {
                        const TWeakPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>* KeyState =
                            RequestKeys.Find(RequestKey);
                        const TSharedPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> PinnedKeyState =
                            KeyState != nullptr ? KeyState->Pin() : nullptr;
                        if (!PinnedKeyState.IsValid() || PinnedKeyState->GetRequestId() == RequestId)
                        {
                            RequestKeys.Remove(RequestKey);
                        }
                    }
                });

        TSharedPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> PreviousRequest;
        {
            FScopeLock Lock(&RequestsMutex);
            Requests.Add(RequestId, State);
            if (!RequestKey.IsEmpty())
            {
                if (const TWeakPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>* Existing =
                        RequestKeys.Find(RequestKey))
                {
                    PreviousRequest = Existing->Pin();
                }
                RequestKeys.Add(RequestKey, State);
            }
        }

        if (PreviousRequest.IsValid())
        {
            PreviousRequest->Cancel();
        }

        if (bShutdown.load(std::memory_order_acquire))
        {
            State->Cancel();
            return FOpenPocketBaseRequestHandle(State);
        }

        if (bCoordinateAuth && Request.Headers.Contains(TEXT("Authorization")))
        {
            const TSharedRef<FAuthorizedRequestOperation, ESPMode::ThreadSafe> Operation =
                MakeShared<FAuthorizedRequestOperation, ESPMode::ThreadSafe>(
                    this,
                    Owner,
                    MoveTemp(Request),
                    Options,
                    bEligibleRead,
                    State,
                    MoveTemp(Handler));
            Operation->Start();
        }
        else
        {
            StartAttempts(
                Request,
                Options,
                bEligibleRead,
                State,
                MoveTemp(Handler),
                false,
                MoveTemp(OnChunk));
        }
        return FOpenPocketBaseRequestHandle(State);
    }

    TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> CreateCompositeState(
        TUniqueFunction<void()> OnCancelled)
    {
        const uint64 RequestId = NextRequestId.fetch_add(1, std::memory_order_relaxed);
        const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> State =
            MakeShared<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>(
                RequestId,
                MoveTemp(OnCancelled),
                [this, RequestId]()
                {
                    FScopeLock Lock(&RequestsMutex);
                    Requests.Remove(RequestId);
                });
        {
            FScopeLock Lock(&RequestsMutex);
            Requests.Add(RequestId, State);
        }

        if (bShutdown.load(std::memory_order_acquire))
        {
            State->Cancel();
        }
        else
        {
            State->TryMarkSending();
        }
        return State;
    }

    FOpenPocketBaseRequestHandle MakeRequestHandle(
        const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State) const
    {
        return FOpenPocketBaseRequestHandle(State);
    }

    bool TryPersistSessionLocked(
        const FString& Collection,
        const FString& Token,
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseError& OutError)
    {
        if (Config.SessionPersistence == EOpenPocketBaseSessionPersistence::MemoryOnly)
        {
            PersistenceState = EOpenPocketBaseSessionPersistenceState::MemoryOnly;
            OutError = FOpenPocketBaseError();
            return true;
        }

        TArray<uint8> Envelope;
        if (!OpenPocketBase::SessionEnvelope::Serialize(
                BaseUrl,
                Config.ProfileName,
                Collection,
                Token,
                Record,
                Envelope,
                OutError) ||
            !SecureStore->Save(SecureStorageKey, Envelope, OutError))
        {
            PersistenceState = EOpenPocketBaseSessionPersistenceState::Failed;
            if (!OutError.IsSet())
            {
                OutError = MakeLocalError(
                    EOpenPocketBaseErrorKind::SecureStorage,
                    TEXT("The secure session could not be saved."));
            }
            return false;
        }

        PersistenceState = EOpenPocketBaseSessionPersistenceState::Persisted;
        OutError = FOpenPocketBaseError();
        return true;
    }

    bool StoreAuth(
        FString Token,
        const FOpenPocketBaseRecord& Record,
        FString Collection,
        const EOpenPocketBaseSessionChangeReason RequestedReason,
        FOpenPocketBaseError& OutError)
    {
        FOpenPocketBaseSessionSnapshot Snapshot;
        {
            FScopeLock Lock(&AuthMutex);
            if (!TryPersistSessionLocked(Collection, Token, Record, OutError))
            {
                return false;
            }
            const bool bUserSwitched = bHasAuthRecord &&
                (!AuthCollection.Equals(Collection, ESearchCase::CaseSensitive) ||
                    AuthRecord.Id != Record.Id);
            AuthToken = MoveTemp(Token);
            AuthExpiryUnixSeconds = TryDecodeJwtExpiry(AuthToken);
            AuthRecord = Record;
            AuthCollection = MoveTemp(Collection);
            bHasAuthRecord = true;
            ++AuthGeneration;

            Snapshot.bAuthenticated = true;
            Snapshot.AuthCollection = AuthCollection;
            Snapshot.AuthGeneration = AuthGeneration;
            Snapshot.PersistenceState = PersistenceState;
            Snapshot.Reason = bUserSwitched
                ? EOpenPocketBaseSessionChangeReason::UserSwitched
                : RequestedReason;
            Snapshot.AuthRecord = AuthRecord;
        }
        SessionEvents->Enqueue(MoveTemp(Snapshot));
        Realtime->NotifyAuthChanged();
        return true;
    }

    bool TryStoreCurrentAuthRecord(
        const FString& Collection,
        const FOpenPocketBaseRecord& Record,
        bool& bOutUpdated,
        FOpenPocketBaseError& OutError)
    {
        bOutUpdated = false;
        FOpenPocketBaseSessionSnapshot Snapshot;
        {
            FScopeLock Lock(&AuthMutex);
            if (!bHasAuthRecord || AuthCollection != Collection || AuthRecord.Id != Record.Id)
            {
                OutError = FOpenPocketBaseError();
                return true;
            }
            if (!TryPersistSessionLocked(AuthCollection, AuthToken, Record, OutError))
            {
                return false;
            }

            AuthRecord = Record;
            ++AuthGeneration;
            bOutUpdated = true;

            Snapshot.bAuthenticated = true;
            Snapshot.AuthCollection = AuthCollection;
            Snapshot.AuthGeneration = AuthGeneration;
            Snapshot.PersistenceState = PersistenceState;
            Snapshot.Reason = EOpenPocketBaseSessionChangeReason::RecordUpdated;
            Snapshot.AuthRecord = AuthRecord;
        }
        SessionEvents->Enqueue(MoveTemp(Snapshot));
        Realtime->NotifyAuthChanged();
        OutError = FOpenPocketBaseError();
        return true;
    }

    bool TryMarkCurrentAuthRecordVerified(
        const FString& RecordId,
        const FString& CollectionId,
        FOpenPocketBaseError& OutError)
    {
        FOpenPocketBaseSessionSnapshot Snapshot;
        bool bUpdated = false;
        {
            FScopeLock Lock(&AuthMutex);
            if (!bHasAuthRecord || AuthRecord.Id != RecordId ||
                AuthRecord.CollectionId != CollectionId ||
                !AuthRecord.Data.JsonObject.IsValid())
            {
                OutError = FOpenPocketBaseError();
                return true;
            }

            FOpenPocketBaseRecord UpdatedRecord = AuthRecord;
            UpdatedRecord.Data.JsonObject =
                MakeShared<FJsonObject>(*AuthRecord.Data.JsonObject);
            UpdatedRecord.Data.JsonObject->SetBoolField(TEXT("verified"), true);
            if (!TryPersistSessionLocked(
                    AuthCollection,
                    AuthToken,
                    UpdatedRecord,
                    OutError))
            {
                return false;
            }

            AuthRecord = MoveTemp(UpdatedRecord);
            ++AuthGeneration;
            bUpdated = true;

            Snapshot.bAuthenticated = true;
            Snapshot.AuthCollection = AuthCollection;
            Snapshot.AuthGeneration = AuthGeneration;
            Snapshot.PersistenceState = PersistenceState;
            Snapshot.Reason = EOpenPocketBaseSessionChangeReason::RecordUpdated;
            Snapshot.AuthRecord = AuthRecord;
        }
        if (bUpdated)
        {
            SessionEvents->Enqueue(MoveTemp(Snapshot));
            Realtime->NotifyAuthChanged();
        }
        OutError = FOpenPocketBaseError();
        return true;
    }

    void TryClearCurrentAuthIdentity(
        const FString& RecordId,
        const FString& CollectionId)
    {
        FString CurrentCollection;
        {
            FScopeLock Lock(&AuthMutex);
            if (!bHasAuthRecord || AuthRecord.Id != RecordId ||
                AuthRecord.CollectionId != CollectionId)
            {
                return;
            }
            CurrentCollection = AuthCollection;
        }
        TryClearCurrentAuthRecord(CurrentCollection, RecordId);
    }

    bool TryClearCurrentAuthRecord(
        const FString& Collection,
        const FString& RecordId)
    {
        FOpenPocketBaseSessionSnapshot Snapshot;
        {
            FScopeLock Lock(&AuthMutex);
            if (!bHasAuthRecord || AuthCollection != Collection || AuthRecord.Id != RecordId)
            {
                return false;
            }
            if (Config.SessionPersistence == EOpenPocketBaseSessionPersistence::RequireSecureStorage)
            {
                FOpenPocketBaseError DeleteError;
                PersistenceState = SecureStore->Delete(SecureStorageKey, DeleteError)
                    ? EOpenPocketBaseSessionPersistenceState::MemoryOnly
                    : EOpenPocketBaseSessionPersistenceState::Failed;
            }
            AuthToken.Reset();
            AuthExpiryUnixSeconds.Reset();
            AuthRecord = FOpenPocketBaseRecord();
            AuthCollection.Reset();
            bHasAuthRecord = false;
            ++AuthGeneration;

            Snapshot.AuthGeneration = AuthGeneration;
            Snapshot.PersistenceState = PersistenceState;
            Snapshot.Reason = EOpenPocketBaseSessionChangeReason::LoggedOut;
        }
        SessionEvents->Enqueue(MoveTemp(Snapshot));
        Realtime->NotifyAuthChanged();
        return true;
    }

    bool TrySynchronizeBatchAuthRecord(
        const FOpenPocketBaseBatchRequest& Batch,
        const FOpenPocketBaseBatchResult& Result,
        FOpenPocketBaseError& OutError)
    {
        FString CurrentCollection;
        FString CurrentRecordId;
        {
            FScopeLock Lock(&AuthMutex);
            if (!bHasAuthRecord)
            {
                OutError = FOpenPocketBaseError();
                return true;
            }
            CurrentCollection = AuthCollection;
            CurrentRecordId = AuthRecord.Id;
        }

        for (int32 Index = Batch.Entries.Num() - 1; Index >= 0; --Index)
        {
            if (!Result.Results.IsValidIndex(Index) ||
                Batch.Entries[Index].Collection != CurrentCollection)
            {
                continue;
            }

            const FOpenPocketBaseBatchEntry& Entry = Batch.Entries[Index];
            const FOpenPocketBaseBatchOperationResult& OperationResult = Result.Results[Index];
            if (Entry.Operation == EOpenPocketBaseBatchOperation::Delete)
            {
                if (Entry.RecordId == CurrentRecordId)
                {
                    TryClearCurrentAuthRecord(CurrentCollection, CurrentRecordId);
                    OutError = FOpenPocketBaseError();
                    return true;
                }
                continue;
            }
            if (OperationResult.bHasRecord && OperationResult.Record.Id == CurrentRecordId)
            {
                bool bUpdated = false;
                return TryStoreCurrentAuthRecord(
                    CurrentCollection,
                    OperationResult.Record,
                    bUpdated,
                    OutError);
            }
        }

        OutError = FOpenPocketBaseError();
        return true;
    }

    void ClearAuth()
    {
        FOpenPocketBaseSessionSnapshot Snapshot;
        {
            FScopeLock Lock(&AuthMutex);
            if (Config.SessionPersistence == EOpenPocketBaseSessionPersistence::RequireSecureStorage)
            {
                FOpenPocketBaseError DeleteError;
                PersistenceState = SecureStore->Delete(SecureStorageKey, DeleteError)
                    ? EOpenPocketBaseSessionPersistenceState::MemoryOnly
                    : EOpenPocketBaseSessionPersistenceState::Failed;
            }
            AuthToken.Reset();
            AuthExpiryUnixSeconds.Reset();
            AuthRecord = FOpenPocketBaseRecord();
            AuthCollection.Reset();
            bHasAuthRecord = false;
            ++AuthGeneration;

            Snapshot.AuthGeneration = AuthGeneration;
            Snapshot.PersistenceState = PersistenceState;
            Snapshot.Reason = EOpenPocketBaseSessionChangeReason::LoggedOut;
        }
        SessionEvents->Enqueue(MoveTemp(Snapshot));
        Realtime->NotifyAuthChanged();
    }

    bool TryStoreRefreshedAuth(
        const int64 CapturedGeneration,
        const FString& CapturedToken,
        const FString& Collection,
        FString NewToken,
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseError& OutError)
    {
        FOpenPocketBaseSessionSnapshot Snapshot;
        {
            FScopeLock Lock(&AuthMutex);
            if (AuthGeneration != CapturedGeneration || AuthToken != CapturedToken ||
                AuthCollection != Collection || !bHasAuthRecord)
            {
                OutError = MakeLocalError(
                    EOpenPocketBaseErrorKind::Authentication,
                    TEXT("The session changed while Auth Refresh was in flight."));
                return false;
            }

            if (!TryPersistSessionLocked(Collection, NewToken, Record, OutError))
            {
                return false;
            }

            AuthToken = MoveTemp(NewToken);
            AuthExpiryUnixSeconds = TryDecodeJwtExpiry(AuthToken);
            AuthRecord = Record;
            ++AuthGeneration;

            Snapshot.bAuthenticated = true;
            Snapshot.AuthCollection = AuthCollection;
            Snapshot.AuthGeneration = AuthGeneration;
            Snapshot.PersistenceState = PersistenceState;
            Snapshot.Reason = EOpenPocketBaseSessionChangeReason::Refreshed;
            Snapshot.AuthRecord = AuthRecord;
        }
        SessionEvents->Enqueue(MoveTemp(Snapshot));
        Realtime->NotifyAuthChanged();
        OutError = FOpenPocketBaseError();
        return true;
    }

    bool RestorePersistedSession(
        FOpenPocketBaseSessionRestoreResult& OutResult,
        FOpenPocketBaseError& OutError)
    {
        OutResult = FOpenPocketBaseSessionRestoreResult();
        OutError = FOpenPocketBaseError();
        int64 CapturedGeneration = 0;
        {
            FScopeLock Lock(&AuthMutex);
            CapturedGeneration = AuthGeneration;
        }
        if (Config.SessionPersistence != EOpenPocketBaseSessionPersistence::RequireSecureStorage)
        {
            OutResult.Status = EOpenPocketBaseSessionRestoreStatus::PolicyRejected;
            return true;
        }

        FString UnavailableReason;
        if (!SecureStore->IsAvailable(UnavailableReason))
        {
            FScopeLock Lock(&AuthMutex);
            PersistenceState = EOpenPocketBaseSessionPersistenceState::Unavailable;
            OutResult.Status = EOpenPocketBaseSessionRestoreStatus::Unavailable;
            return true;
        }

        TArray<uint8> Envelope;
        bool bFound = false;
        if (!SecureStore->Load(SecureStorageKey, Envelope, bFound, OutError))
        {
            FScopeLock Lock(&AuthMutex);
            PersistenceState = EOpenPocketBaseSessionPersistenceState::Failed;
            return false;
        }
        if (!bFound)
        {
            OutResult.Status = EOpenPocketBaseSessionRestoreStatus::NotFound;
            return true;
        }

        FString Collection;
        FString Token;
        FOpenPocketBaseRecord Record;
        const OpenPocketBase::SessionEnvelope::EReadResult ReadResult =
            OpenPocketBase::SessionEnvelope::Deserialize(
                Envelope,
                BaseUrl,
                Config.ProfileName,
                Collection,
                Token,
                Record);
        if (ReadResult != OpenPocketBase::SessionEnvelope::EReadResult::Valid)
        {
            FOpenPocketBaseError DeleteError;
            if (!SecureStore->Delete(SecureStorageKey, DeleteError))
            {
                FScopeLock Lock(&AuthMutex);
                PersistenceState = EOpenPocketBaseSessionPersistenceState::Failed;
            }
            OutResult.Status = ReadResult == OpenPocketBase::SessionEnvelope::EReadResult::PolicyRejected
                ? EOpenPocketBaseSessionRestoreStatus::PolicyRejected
                : EOpenPocketBaseSessionRestoreStatus::Corrupt;
            return true;
        }

        FOpenPocketBaseSessionSnapshot Snapshot;
        {
            FScopeLock Lock(&AuthMutex);
            if (AuthGeneration != CapturedGeneration)
            {
                OutError = MakeLocalError(
                    EOpenPocketBaseErrorKind::Authentication,
                    TEXT("The session changed while secure restore was in flight."));
                return false;
            }
            AuthToken = MoveTemp(Token);
            AuthExpiryUnixSeconds = TryDecodeJwtExpiry(AuthToken);
            AuthRecord = MoveTemp(Record);
            AuthCollection = MoveTemp(Collection);
            bHasAuthRecord = true;
            ++AuthGeneration;
            PersistenceState = EOpenPocketBaseSessionPersistenceState::Persisted;

            Snapshot.bAuthenticated = true;
            Snapshot.AuthCollection = AuthCollection;
            Snapshot.AuthGeneration = AuthGeneration;
            Snapshot.PersistenceState = PersistenceState;
            Snapshot.Reason = EOpenPocketBaseSessionChangeReason::Restored;
            Snapshot.AuthRecord = AuthRecord;
        }
        SessionEvents->Enqueue(Snapshot);
        Realtime->NotifyAuthChanged();
        OutResult.Status = EOpenPocketBaseSessionRestoreStatus::Restored;
        OutResult.Session = MoveTemp(Snapshot);
        return true;
    }

    void FinishRefresh(
        const TSharedRef<FRefreshFlight, ESPMode::ThreadSafe>& Flight,
        TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result)
    {
        TArray<FRefreshWaiter> Waiters;
        {
            FScopeLock Lock(&RefreshMutex);
            if (ActiveRefresh == Flight)
            {
                ActiveRefresh.Reset();
            }
            Waiters = MoveTemp(Flight->Waiters);
        }

        if (Result.IsSuccess())
        {
            const FOpenPocketBaseAuthResult Value = Result.GetValue();
            for (FRefreshWaiter& Waiter : Waiters)
            {
                Waiter.State->TryComplete(
                    EOpenPocketBaseRequestState::Succeeded,
                    [Completion = Waiter.Completion, Value]() mutable
                    {
                        Completion->Invoke(
                            TOpenPocketBaseResult<FOpenPocketBaseAuthResult>::Success(Value));
                    });
            }
            return;
        }

        const FOpenPocketBaseError Error = Result.GetError();
        const EOpenPocketBaseRequestState Terminal = Error.Kind == EOpenPocketBaseErrorKind::Cancelled
            ? EOpenPocketBaseRequestState::Cancelled
            : EOpenPocketBaseRequestState::Failed;
        for (FRefreshWaiter& Waiter : Waiters)
        {
            Waiter.State->TryComplete(
                Terminal,
                [Completion = Waiter.Completion, Error]() mutable
                {
                    Completion->Invoke(
                        TOpenPocketBaseResult<FOpenPocketBaseAuthResult>::Failure(Error));
                });
        }
    }

    void Shutdown()
    {
        if (bShutdown.exchange(true, std::memory_order_acq_rel))
        {
            return;
        }

        {
            FScopeLock Lock(&OAuthMutex);
            OAuthTransactions.Reset();
        }

        if (Realtime.IsValid())
        {
            Realtime->Shutdown();
        }

        TArray<TSharedPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>> ActiveRequests;
        {
            FScopeLock Lock(&RequestsMutex);
            ActiveRequests.Reserve(Requests.Num());
            for (const TPair<uint64, TWeakPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>>& Pair : Requests)
            {
                if (TSharedPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> State = Pair.Value.Pin())
                {
                    ActiveRequests.Add(MoveTemp(State));
                }
            }
        }

        for (const TSharedPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State : ActiveRequests)
        {
            State->Cancel();
        }
    }
};

namespace OpenPocketBase::Internal
{
class FAssistedOAuthOperation final
    : public TSharedFromThis<FAssistedOAuthOperation, ESPMode::ThreadSafe>
{
public:
    struct FCancelBridge
    {
        void Set(TFunction<void()> InCancel)
        {
            FScopeLock Lock(&Mutex);
            Cancel = MoveTemp(InCancel);
        }

        void Invoke()
        {
            TFunction<void()> LocalCancel;
            {
                FScopeLock Lock(&Mutex);
                LocalCancel = Cancel;
            }
            if (LocalCancel)
            {
                LocalCancel();
            }
        }

        FCriticalSection Mutex;
        TFunction<void()> Cancel;
    };

    static FOpenPocketBaseRequestHandle Start(
        const TSharedRef<FOpenPocketBaseClient, ESPMode::ThreadSafe>& Client,
        FString AuthCollection,
        FOpenPocketBaseAssistedOAuth2Options Options,
        FOpenPocketBaseAuthAttemptCallback OnComplete)
    {
        const TSharedRef<TCompletionState<FOpenPocketBaseAuthAttempt>, ESPMode::ThreadSafe>
            Completion = MakeShared<
                TCompletionState<FOpenPocketBaseAuthAttempt>,
                ESPMode::ThreadSafe>(MoveTemp(OnComplete));
        const TSharedRef<FCancelBridge, ESPMode::ThreadSafe> CancelBridge =
            MakeShared<FCancelBridge, ESPMode::ThreadSafe>();
        const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> RequestState =
            Client->Impl->CreateCompositeState(
                [CancelBridge, Completion]()
                {
                    CancelBridge->Invoke();
                    Completion->Invoke(
                        TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>::Failure(
                            MakeCancelledError()));
                });
        const FOpenPocketBaseRequestHandle Handle =
            Client->Impl->MakeRequestHandle(RequestState);
        const TSharedRef<FAssistedOAuthOperation, ESPMode::ThreadSafe> Operation =
            MakeShared<FAssistedOAuthOperation, ESPMode::ThreadSafe>(
                Client,
                MoveTemp(AuthCollection),
                MoveTemp(Options),
                RequestState,
                Completion);
        const TWeakPtr<FAssistedOAuthOperation, ESPMode::ThreadSafe> WeakOperation = Operation;
        CancelBridge->Set([WeakOperation]()
        {
            if (const TSharedPtr<FAssistedOAuthOperation, ESPMode::ThreadSafe> Pinned =
                    WeakOperation.Pin())
            {
                Pinned->CancelStages();
            }
        });
        Operation->BeginDiscovery();
        return Handle;
    }

    FAssistedOAuthOperation(
        TSharedRef<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient,
        FString InAuthCollection,
        FOpenPocketBaseAssistedOAuth2Options InOptions,
        TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> InRequestState,
        TSharedRef<TCompletionState<FOpenPocketBaseAuthAttempt>, ESPMode::ThreadSafe> InCompletion)
        : Client(MoveTemp(InClient))
        , AuthCollection(MoveTemp(InAuthCollection))
        , Options(MoveTemp(InOptions))
        , RequestState(MoveTemp(InRequestState))
        , Completion(MoveTemp(InCompletion))
    {
    }

private:
    void BeginDiscovery()
    {
        if (!RequestState->IsActive())
        {
            return;
        }
        const TSharedRef<FAssistedOAuthOperation, ESPMode::ThreadSafe> Self = AsShared();
        SetChildHandle(Client->Collection(AuthCollection).ListAuthMethods(
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAuthMethods>&& Result)
            {
                Self->HandleDiscovery(MoveTemp(Result));
            },
            Options.RequestOptions));
    }

    void HandleDiscovery(TOpenPocketBaseResult<FOpenPocketBaseAuthMethods>&& Result)
    {
        if (!RequestState->IsActive())
        {
            return;
        }
        if (!Result.IsSuccess())
        {
            FinishFailure(Result.GetError());
            return;
        }
        if (!Result.GetValue().OAuth2.bEnabled)
        {
            FinishFailure(MakeLocalError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("OAuth2 is not enabled for this auth collection.")));
            return;
        }
        const FOpenPocketBaseOAuthProvider* MatchedProvider =
            Result.GetValue().OAuth2.Providers.FindByPredicate(
                [this](const FOpenPocketBaseOAuthProvider& Candidate)
                {
                    return Candidate.Name == Options.Provider;
                });
        if (MatchedProvider == nullptr)
        {
            FinishFailure(MakeLocalError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("The requested OAuth provider is not available.")));
            return;
        }
        Provider = *MatchedProvider;

        const TSharedRef<FAssistedOAuthOperation, ESPMode::ThreadSafe> Self = AsShared();
        FOpenPocketBaseRealtimeCallbacks Callbacks;
        Callbacks.OnEvent = [Self](const FOpenPocketBaseRealtimeEvent& Event)
        {
            Self->HandleRealtimeEvent(Event);
        };
        Callbacks.OnConnectionStateChanged =
            [Self](const EOpenPocketBaseRealtimeConnectionState State)
            {
                Self->HandleConnectionState(State);
            };
        Callbacks.OnError = [Self](const FOpenPocketBaseError& Error)
        {
            Self->HandleRealtimeError(Error);
        };
        Callbacks.OnResyncRequired = [Self]()
        {
            Self->HandleRealtimeGap();
        };

        FOpenPocketBaseError SubscribeError;
        FOpenPocketBaseSubscriptionHandle NewSubscription = Client->Subscribe(
            TEXT("@oauth2"),
            MoveTemp(Callbacks),
            {},
            SubscribeError);
        if (!NewSubscription.IsActive())
        {
            FinishFailure(MoveTemp(SubscribeError));
            return;
        }
        SetSubscription(MoveTemp(NewSubscription));
    }

    void HandleConnectionState(const EOpenPocketBaseRealtimeConnectionState State)
    {
        if (!RequestState->IsActive())
        {
            return;
        }
        if (State == EOpenPocketBaseRealtimeConnectionState::Active)
        {
            BeginBrowser();
            return;
        }
        if (State == EOpenPocketBaseRealtimeConnectionState::Reconnecting ||
            State == EOpenPocketBaseRealtimeConnectionState::Stopped)
        {
            bool bIgnore = false;
            {
                FScopeLock Lock(&StageMutex);
                bIgnore = bHandoffAccepted;
            }
            if (!bIgnore)
            {
                FinishFailure(MakeLocalError(
                    EOpenPocketBaseErrorKind::Transport,
                    TEXT("The assisted OAuth realtime handoff was interrupted.")));
            }
        }
    }

    void BeginBrowser()
    {
        FString RealtimeClientId;
        if (!Client->Impl->Realtime->TryGetActiveClientId(RealtimeClientId))
        {
            FinishFailure(MakeLocalError(
                EOpenPocketBaseErrorKind::Transport,
                TEXT("The assisted OAuth realtime identity is unavailable.")));
            return;
        }

        {
            FScopeLock Lock(&StageMutex);
            if (bBrowserOpened || bHandoffAccepted || !RequestState->IsActive())
            {
                return;
            }
            bBrowserOpened = true;
            ExpectedState = RealtimeClientId;
        }

        FString NewTransactionId;
        FString AuthorizationUrl;
        FOpenPocketBaseError Error;
        if (!Client->Impl->TryCreateAssistedOAuthTransaction(
                AuthCollection,
                Provider,
                Options.Scopes,
                RealtimeClientId,
                NewTransactionId,
                AuthorizationUrl,
                Error))
        {
            FinishFailure(MoveTemp(Error));
            return;
        }
        {
            FScopeLock Lock(&StageMutex);
            TransactionId = NewTransactionId;
        }
        if (!Client->Impl->OAuthBrowser->OpenExternalAuthorizationUrl(
                AuthorizationUrl,
                Error))
        {
            FinishFailure(MoveTemp(Error));
        }
    }

    void HandleRealtimeEvent(const FOpenPocketBaseRealtimeEvent& Event)
    {
        if (!RequestState->IsActive() || Event.Topic != TEXT("@oauth2") ||
            !Event.Data.JsonObject.IsValid())
        {
            return;
        }

        FString EventState;
        FString Code;
        FString ProviderError;
        Event.Data.JsonObject->TryGetStringField(TEXT("state"), EventState);
        Event.Data.JsonObject->TryGetStringField(TEXT("code"), Code);
        Event.Data.JsonObject->TryGetStringField(TEXT("error"), ProviderError);

        FString ActiveTransactionId;
        bool bStateMismatch = false;
        bool bProviderRejected = false;
        {
            FScopeLock Lock(&StageMutex);
            if (!bBrowserOpened || bHandoffAccepted)
            {
                return;
            }
            bStateMismatch = EventState != ExpectedState;
            bProviderRejected = !ProviderError.IsEmpty() || !IsSafeOAuthValue(Code, 4096);
            if (!bStateMismatch && !bProviderRejected)
            {
                bHandoffAccepted = true;
                ActiveTransactionId = TransactionId;
            }
        }
        if (bStateMismatch)
        {
            FinishFailure(MakeLocalError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("The assisted OAuth handoff state does not match.")));
            return;
        }
        if (bProviderRejected)
        {
            FinishFailure(MakeLocalError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("The OAuth provider rejected the assisted authorization request.")));
            return;
        }

        UnsubscribeHandoff();
        FOpenPocketBaseOAuth2Callback Callback;
        Callback.TransactionId = ActiveTransactionId;
        Callback.CallbackUrl = Client->Impl->BaseUrl + TEXT("/api/oauth2-redirect?state=") +
            FGenericPlatformHttp::UrlEncode(EventState) + TEXT("&code=") +
            FGenericPlatformHttp::UrlEncode(Code);
        Callback.CreateData = Options.CreateData;
        Callback.Mfa = Options.Mfa;
        Callback.RequestOptions = Options.RequestOptions;
        const TSharedRef<FAssistedOAuthOperation, ESPMode::ThreadSafe> Self = AsShared();
        SetChildHandle(Client->Collection(AuthCollection).CompleteOAuth2(
            MoveTemp(Callback),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
            {
                Self->HandleExchange(MoveTemp(Result));
            }));
    }

    void HandleExchange(TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
    {
        if (!RequestState->IsActive())
        {
            return;
        }
        if (Result.IsSuccess())
        {
            FinishSuccess(MoveTemp(Result.GetValue()));
        }
        else
        {
            FinishFailure(Result.GetError());
        }
    }

    void HandleRealtimeError(const FOpenPocketBaseError& Error)
    {
        if (!RequestState->IsActive())
        {
            return;
        }
        {
            FScopeLock Lock(&StageMutex);
            if (bHandoffAccepted)
            {
                return;
            }
        }
        FOpenPocketBaseError Sanitized = Error;
        Sanitized.ServerMessage = TEXT("The assisted OAuth realtime handoff failed.");
        Sanitized.ServerCode.Reset();
        Sanitized.FieldErrors.Reset();
        FinishFailure(MoveTemp(Sanitized));
    }

    void HandleRealtimeGap()
    {
        bool bIgnore = false;
        {
            FScopeLock Lock(&StageMutex);
            bIgnore = bHandoffAccepted;
        }
        if (RequestState->IsActive() && !bIgnore)
        {
            FinishFailure(MakeLocalError(
                EOpenPocketBaseErrorKind::Transport,
                TEXT("The assisted OAuth realtime handoff may have missed its result.")));
        }
    }

    void SetChildHandle(FOpenPocketBaseRequestHandle Handle)
    {
        bool bCancel = false;
        {
            FScopeLock Lock(&StageMutex);
            bCancel = !RequestState->IsActive();
            if (!bCancel)
            {
                ChildHandle = Handle;
            }
        }
        if (bCancel)
        {
            Handle.Cancel();
        }
    }

    void SetSubscription(FOpenPocketBaseSubscriptionHandle Handle)
    {
        bool bCancel = false;
        {
            FScopeLock Lock(&StageMutex);
            bCancel = !RequestState->IsActive();
            if (!bCancel)
            {
                Subscription = MoveTemp(Handle);
            }
        }
        if (bCancel)
        {
            Handle.Unsubscribe();
        }
    }

    void UnsubscribeHandoff()
    {
        FOpenPocketBaseSubscriptionHandle LocalSubscription;
        {
            FScopeLock Lock(&StageMutex);
            LocalSubscription = MoveTemp(Subscription);
        }
        LocalSubscription.Unsubscribe();
    }

    void CancelStages()
    {
        FOpenPocketBaseRequestHandle LocalChild;
        FOpenPocketBaseSubscriptionHandle LocalSubscription;
        FString LocalTransactionId;
        {
            FScopeLock Lock(&StageMutex);
            LocalChild = ChildHandle;
            ChildHandle = {};
            LocalSubscription = MoveTemp(Subscription);
            LocalTransactionId = MoveTemp(TransactionId);
        }
        LocalChild.Cancel();
        LocalSubscription.Unsubscribe();
        if (!LocalTransactionId.IsEmpty())
        {
            Client->Impl->CancelOAuthTransaction(LocalTransactionId);
        }
    }

    void FinishSuccess(FOpenPocketBaseAuthAttempt Result)
    {
        CancelStages();
        RequestState->TryComplete(
            EOpenPocketBaseRequestState::Succeeded,
            [Completion = Completion, Result = MoveTemp(Result)]() mutable
            {
                Completion->Invoke(
                    TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>::Success(
                        MoveTemp(Result)));
            });
    }

    void FinishFailure(FOpenPocketBaseError Error)
    {
        CancelStages();
        const EOpenPocketBaseRequestState Terminal =
            Error.Kind == EOpenPocketBaseErrorKind::Cancelled
                ? EOpenPocketBaseRequestState::Cancelled
                : EOpenPocketBaseRequestState::Failed;
        RequestState->TryComplete(
            Terminal,
            [Completion = Completion, Error = MoveTemp(Error)]() mutable
            {
                Completion->Invoke(
                    TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>::Failure(
                        MoveTemp(Error)));
            });
    }

    TSharedRef<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    FString AuthCollection;
    FOpenPocketBaseAssistedOAuth2Options Options;
    TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> RequestState;
    TSharedRef<TCompletionState<FOpenPocketBaseAuthAttempt>, ESPMode::ThreadSafe> Completion;
    FCriticalSection StageMutex;
    FOpenPocketBaseRequestHandle ChildHandle;
    FOpenPocketBaseSubscriptionHandle Subscription;
    FOpenPocketBaseOAuthProvider Provider;
    FString ExpectedState;
    FString TransactionId;
    bool bBrowserOpened = false;
    bool bHandoffAccepted = false;
};
}

namespace
{
class FChainedAccountRequest final
{
public:
    void SetChild(FOpenPocketBaseRequestHandle Handle)
    {
        bool bCancel = false;
        {
            FScopeLock Lock(&Mutex);
            bCancel = bCancelled;
            if (!bCancel)
            {
                Child = Handle;
            }
        }
        if (bCancel)
        {
            Handle.Cancel();
        }
    }

    void Cancel()
    {
        FOpenPocketBaseRequestHandle LocalChild;
        {
            FScopeLock Lock(&Mutex);
            bCancelled = true;
            LocalChild = Child;
            Child = {};
        }
        LocalChild.Cancel();
    }

private:
    FCriticalSection Mutex;
    FOpenPocketBaseRequestHandle Child;
    bool bCancelled = false;
};

bool TryMakeExternalAuth(
    FOpenPocketBaseRecord Record,
    FOpenPocketBaseExternalAuth& OutExternalAuth)
{
    OutExternalAuth = FOpenPocketBaseExternalAuth();
    if (!Record.Data.JsonObject.IsValid() ||
        !Record.Data.JsonObject->TryGetStringField(
            TEXT("collectionRef"), OutExternalAuth.CollectionRef) ||
        !Record.Data.JsonObject->TryGetStringField(
            TEXT("recordRef"), OutExternalAuth.RecordRef) ||
        !Record.Data.JsonObject->TryGetStringField(
            TEXT("provider"), OutExternalAuth.Provider) ||
        !Record.Data.JsonObject->TryGetStringField(
            TEXT("providerId"), OutExternalAuth.ProviderId) ||
        OutExternalAuth.CollectionRef.Len() > 256 ||
        OutExternalAuth.RecordRef.Len() > 256 ||
        OutExternalAuth.Provider.Len() > 128 ||
        OutExternalAuth.ProviderId.Len() > 4096)
    {
        return false;
    }
    OutExternalAuth.Record = MoveTemp(Record);
    return true;
}

class FOpenPocketBaseFullListOperation final
    : public TSharedFromThis<FOpenPocketBaseFullListOperation, ESPMode::ThreadSafe>
{
public:
    FOpenPocketBaseFullListOperation(
        TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient,
        FString InCollection,
        FOpenPocketBaseFullListOptions InOptions,
        TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> InRequestState,
        TSharedRef<TCompletionState<FOpenPocketBaseFullListResult>, ESPMode::ThreadSafe> InCompletion)
        : Client(MoveTemp(InClient))
        , Collection(MoveTemp(InCollection))
        , Options(MoveTemp(InOptions))
        , RequestState(MoveTemp(InRequestState))
        , Completion(MoveTemp(InCompletion))
    {
    }

    void Start()
    {
        RequestNextPage();
    }

private:
    void RequestNextPage()
    {
        if (RequestState->GetState() != EOpenPocketBaseRequestState::Sending)
        {
            return;
        }

        const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
        if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
        {
            FinishFailure(MakeCancelledError());
            return;
        }

        FOpenPocketBaseListOptions PageOptions = Options.ListOptions;
        PageOptions.Page = NextPage;
        const TSharedRef<FOpenPocketBaseFullListOperation, ESPMode::ThreadSafe> Self = AsShared();
        const FOpenPocketBaseRequestHandle PageRequest =
            PinnedClient->Collection(Collection).GetList(
                MoveTemp(PageOptions),
                [Self](TOpenPocketBaseResult<FOpenPocketBaseRecordPage>&& Result)
                {
                    Self->HandlePage(MoveTemp(Result));
                });
        RequestState->AttachTransportHandle(FOpenPocketBaseTransportHandle(
            [PageRequest]()
            {
                PageRequest.Cancel();
            }));
    }

    void HandlePage(TOpenPocketBaseResult<FOpenPocketBaseRecordPage>&& PageResult)
    {
        if (RequestState->GetState() != EOpenPocketBaseRequestState::Sending)
        {
            return;
        }
        if (!PageResult.IsSuccess())
        {
            FinishFailure(PageResult.GetError());
            return;
        }

        FOpenPocketBaseRecordPage Page = PageResult.TakeValue();
        ++Result.PagesFetched;
        const int32 ReceivedItems = Page.Items.Num();
        int32 ItemsToAdd = ReceivedItems;
        if (Options.MaxItems > 0)
        {
            ItemsToAdd = FMath::Min(ItemsToAdd, Options.MaxItems - Result.Items.Num());
        }
        Result.Items.Reserve(Result.Items.Num() + ItemsToAdd);
        for (int32 Index = 0; Index < ItemsToAdd; ++Index)
        {
            Result.Items.Add(MoveTemp(Page.Items[Index]));
        }

        Result.bReachedEnd = ReceivedItems < Options.ListOptions.PerPage ||
            (Page.bHasTotalPages && Page.Page >= Page.TotalPages);
        Result.bReachedItemLimit = Options.MaxItems > 0 && Result.Items.Num() >= Options.MaxItems;
        Result.bReachedPageLimit = Options.MaxPages > 0 && Result.PagesFetched >= Options.MaxPages;
        if (Result.bReachedEnd || Result.bReachedItemLimit || Result.bReachedPageLimit)
        {
            FinishSuccess();
            return;
        }

        ++NextPage;
        RequestNextPage();
    }

    void FinishSuccess()
    {
        RequestState->TryComplete(
            EOpenPocketBaseRequestState::Succeeded,
            [Completion = Completion, Result = MoveTemp(Result)]() mutable
            {
                Completion->Invoke(
                    TOpenPocketBaseResult<FOpenPocketBaseFullListResult>::Success(MoveTemp(Result)));
            });
    }

    void FinishFailure(FOpenPocketBaseError Error)
    {
        const EOpenPocketBaseRequestState Terminal = Error.Kind == EOpenPocketBaseErrorKind::Cancelled
            ? EOpenPocketBaseRequestState::Cancelled
            : EOpenPocketBaseRequestState::Failed;
        RequestState->TryComplete(
            Terminal,
            [Completion = Completion, Error = MoveTemp(Error)]() mutable
            {
                Completion->Invoke(
                    TOpenPocketBaseResult<FOpenPocketBaseFullListResult>::Failure(MoveTemp(Error)));
            });
    }

    TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    FString Collection;
    FOpenPocketBaseFullListOptions Options;
    TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> RequestState;
    TSharedRef<TCompletionState<FOpenPocketBaseFullListResult>, ESPMode::ThreadSafe> Completion;
    FOpenPocketBaseFullListResult Result;
    int32 NextPage = 1;
};
}

FOpenPocketBaseClientResult FOpenPocketBaseClient::Create(
    const FOpenPocketBaseClientConfig& Config,
    FOpenPocketBaseClientDependencies Dependencies)
{
    const TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport =
        Dependencies.Transport.IsValid()
        ? Dependencies.Transport.ToSharedRef()
        : CreateOpenPocketBaseHttpTransport();
    const TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore =
        Dependencies.SecureStore.IsValid()
        ? Dependencies.SecureStore.ToSharedRef()
        : CreateOpenPocketBaseSecureStore();
    const TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock =
        Dependencies.Clock.IsValid()
        ? Dependencies.Clock.ToSharedRef()
        : CreateOpenPocketBaseClock();
    const TSharedRef<IOpenPocketBaseOAuthBrowser, ESPMode::ThreadSafe> OAuthBrowser =
        Dependencies.OAuthBrowser.IsValid()
        ? Dependencies.OAuthBrowser.ToSharedRef()
        : CreateOpenPocketBaseOAuthBrowser();

    FOpenPocketBaseError Error;
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client = CreateInternal(
        Config,
        Transport,
        SecureStore,
        Clock,
        OAuthBrowser,
        Error);
    if (!Client.IsValid())
    {
        return FOpenPocketBaseClientResult::Failure(MoveTemp(Error));
    }
    return FOpenPocketBaseClientResult::Success(Client.ToSharedRef());
}

FOpenPocketBaseClientResult
FOpenPocketBaseClient::CreateEphemeralAuthenticated(
    const FOpenPocketBaseClientConfig& Config,
    FString Token,
    FString AuthCollection,
    const FOpenPocketBaseRecord& AuthRecord,
    FOpenPocketBaseClientDependencies Dependencies)
{
    FOpenPocketBaseClientConfig EphemeralConfig = Config;
    EphemeralConfig.SessionPersistence = EOpenPocketBaseSessionPersistence::MemoryOnly;
    EphemeralConfig.bEnableAssistedOAuth = false;
    FOpenPocketBaseClientResult ClientResult = Create(
        EphemeralConfig,
        MoveTemp(Dependencies));
    if (!ClientResult.IsSuccess())
    {
        return ClientResult;
    }
    FOpenPocketBaseClientRef Client = ClientResult.TakeValue();
    if (Token.IsEmpty() || Token.Len() > 8192 || !IsSafePathSegment(AuthCollection) ||
        AuthRecord.Id.IsEmpty() || AuthRecord.Id.Len() > 255)
    {
        Client->Shutdown();
        return FOpenPocketBaseClientResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("A bounded token, auth collection, and auth record are required.")));
    }
    for (const TCHAR Character : Token)
    {
        if (FChar::IsControl(Character) || FChar::IsWhitespace(Character))
        {
            Client->Shutdown();
            return FOpenPocketBaseClientResult::Failure(MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("The ephemeral authentication token is invalid.")));
        }
    }
    Client->Impl->bAuthRefreshAllowed = false;
    FOpenPocketBaseError Error;
    if (!Client->Impl->StoreAuth(
            MoveTemp(Token),
            AuthRecord,
            MoveTemp(AuthCollection),
            EOpenPocketBaseSessionChangeReason::LoggedIn,
            Error))
    {
        Client->Shutdown();
        return FOpenPocketBaseClientResult::Failure(MoveTemp(Error));
    }
    return FOpenPocketBaseClientResult::Success(Client);
}

TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> FOpenPocketBaseClient::CreateInternal(
    const FOpenPocketBaseClientConfig& Config,
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
    TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore,
    TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock,
    TSharedRef<IOpenPocketBaseOAuthBrowser, ESPMode::ThreadSafe> OAuthBrowser,
    FOpenPocketBaseError& OutError)
{
    FString BaseUrl;
    if (!Config.TryGetNormalizedBaseUrl(BaseUrl, OutError) ||
        !ValidateDefaultHeaders(Config, OutError))
    {
        return nullptr;
    }
    if (!FMath::IsFinite(Config.AuthRefreshLeadTimeSeconds) ||
        Config.AuthRefreshLeadTimeSeconds < 0 || Config.AuthRefreshLeadTimeSeconds > 3600)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Auth Refresh lead time must be between zero and 3600 seconds."));
        return nullptr;
    }

    if (Config.SessionPersistence == EOpenPocketBaseSessionPersistence::RequireSecureStorage)
    {
        FString UnavailableReason;
        if (!SecureStore->IsAvailable(UnavailableReason))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::SecureStorage,
                TEXT("Required secure session storage is unavailable."));
            if (!UnavailableReason.IsEmpty())
            {
                OutError.ServerMessage += TEXT(" ") + UnavailableReason;
            }
            return nullptr;
        }
    }

    OutError = FOpenPocketBaseError();
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        MakeShareable(new FOpenPocketBaseClient(
            Config,
            MoveTemp(BaseUrl),
            MoveTemp(Transport),
            MoveTemp(SecureStore),
            MoveTemp(Clock),
            MoveTemp(OAuthBrowser)));
    Client->Impl->Owner = Client;
    return Client;
}

FOpenPocketBaseClient::FOpenPocketBaseClient(
    FOpenPocketBaseClientConfig Config,
    FString NormalizedBaseUrl,
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
    TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore,
    TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock,
    TSharedRef<IOpenPocketBaseOAuthBrowser, ESPMode::ThreadSafe> OAuthBrowser)
    : Impl(MakeUnique<FImpl>(
        MoveTemp(Config),
        MoveTemp(NormalizedBaseUrl),
        MoveTemp(Transport),
        MoveTemp(SecureStore),
        MoveTemp(Clock),
        MoveTemp(OAuthBrowser)))
{
}

FOpenPocketBaseClient::~FOpenPocketBaseClient()
{
    Shutdown();
}

FOpenPocketBaseCollectionService FOpenPocketBaseClient::Collection(FString CollectionName)
{
    return FOpenPocketBaseCollectionService(AsShared(), MoveTemp(CollectionName));
}

FOpenPocketBaseFileService FOpenPocketBaseClient::Files()
{
    return FOpenPocketBaseFileService(AsShared());
}

FOpenPocketBaseRequestHandle FOpenPocketBaseClient::Health(
    FOpenPocketBaseHealthCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseHealthResult>(
            MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseHealthResult>, ESPMode::ThreadSafe>
        Completion = MakeShared<TCompletionState<FOpenPocketBaseHealthResult>, ESPMode::ThreadSafe>(
            MoveTemp(OnComplete));
    const double StartedAt = Impl->Clock->MonotonicSeconds();
    const TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock = Impl->Clock;
    FOpenPocketBaseHttpRequest Request = Impl->MakeRequest(
        TEXT("GET"), TEXT("/api/health"), {}, Options, false);
    return Impl->Send(
        MoveTemp(Request),
        Options,
        true,
        [Completion, Clock, StartedAt](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<FOpenPocketBaseHealthResult> Result =
                [&Response, &Clock, StartedAt]()
            {
                TOpenPocketBaseResult<bool> Status =
                    OpenPocketBase::Json::ParseEmptyResponse(Response);
                if (!Status.IsSuccess())
                {
                    return TOpenPocketBaseResult<FOpenPocketBaseHealthResult>::Failure(
                        Status.GetError());
                }

                TSharedPtr<FJsonObject> Object;
                double NumericCode = 0;
                FString Message;
                if (!TryParseJsonObject(Response.Body, Object) ||
                    !Object->TryGetNumberField(TEXT("code"), NumericCode) ||
                    !FMath::IsFinite(NumericCode) ||
                    NumericCode != FMath::RoundToDouble(NumericCode) ||
                    NumericCode < MIN_int32 || NumericCode > MAX_int32 ||
                    !Object->TryGetStringField(TEXT("message"), Message) ||
                    !IsBoundedPlainText(Message, 1024))
                {
                    return TOpenPocketBaseResult<FOpenPocketBaseHealthResult>::Failure(
                        MakeResponseSerializationError(
                            Response,
                            TEXT("PocketBase returned an invalid health response.")));
                }

                FOpenPocketBaseHealthResult Health;
                Health.HttpStatus = Response.HttpStatus;
                Health.Code = static_cast<int32>(NumericCode);
                Health.Message = MoveTemp(Message);
                Health.bHealthy = Health.HttpStatus >= 200 && Health.HttpStatus < 300 &&
                    Health.Code == 200;
                Health.DurationSeconds = FMath::Max(
                    0.0, Clock->MonotonicSeconds() - StartedAt);
                return TOpenPocketBaseResult<FOpenPocketBaseHealthResult>::Success(
                    MoveTemp(Health));
            }();
            const EOpenPocketBaseRequestState Terminal = TerminalStateFor(Result.IsSuccess());
            State->TryComplete(
                Terminal,
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion]()
        {
            Completion->Invoke(
                TOpenPocketBaseResult<FOpenPocketBaseHealthResult>::Failure(
                    MakeCancelledError()));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseClient::SendCustomRoute(
    FOpenPocketBaseCustomRouteRequest CustomRequest,
    FOpenPocketBaseCustomRouteCallback OnComplete)
{
    FOpenPocketBaseError ValidationError;
    FString Path;
    const TCHAR* Method = GetCustomRouteMethod(CustomRequest.Method);
    if (Method == nullptr || !ValidateRequestOptions(CustomRequest.Options, ValidationError) ||
        !TryBuildCustomRoutePath(
            CustomRequest.Path, CustomRequest.Query, Path, ValidationError) ||
        CustomRequest.MaxRequestBytes < 0 ||
        CustomRequest.MaxRequestBytes > 64LL * 1024 * 1024 ||
        (CustomRequest.Method == EOpenPocketBaseCustomRouteMethod::Get &&
            CustomRequest.BodyFormat != EOpenPocketBaseCustomBodyFormat::None))
    {
        if (!ValidationError.IsSet())
        {
            ValidationError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("The custom route method or body bound is invalid."));
        }
        DispatchFailure<FOpenPocketBaseCustomRouteResponse>(
            MoveTemp(OnComplete), MoveTemp(ValidationError));
        return {};
    }

    TArray<uint8> Body;
    TSharedPtr<FArchive, ESPMode::ThreadSafe> BodyStream;
    int64 BodyLength = 0;
    FString ContentType;
    const bool bHasJson = CustomRequest.JsonBody.JsonObject.IsValid();
    const bool bHasFields = !CustomRequest.FormFields.IsEmpty();
    const bool bHasFiles = !CustomRequest.Files.IsEmpty();
    const bool bHasBody = !CustomRequest.Body.IsEmpty();
    bool bBodyValid = true;
    switch (CustomRequest.BodyFormat)
    {
    case EOpenPocketBaseCustomBodyFormat::None:
        bBodyValid = !bHasFields && !bHasFiles && !bHasBody &&
            CustomRequest.ContentType.IsEmpty();
        break;
    case EOpenPocketBaseCustomBodyFormat::Json:
        bBodyValid = bHasJson && !bHasFields && !bHasFiles && !bHasBody &&
            CustomRequest.ContentType.IsEmpty();
        if (bBodyValid)
        {
            Body = OpenPocketBase::Json::SerializeObject(
                CustomRequest.JsonBody.JsonObject.ToSharedRef());
            ContentType = TEXT("application/json");
        }
        break;
    case EOpenPocketBaseCustomBodyFormat::Form:
        bBodyValid = bHasFields && !bHasFiles && !bHasBody &&
            CustomRequest.ContentType.IsEmpty() &&
            TrySerializeCustomForm(CustomRequest.FormFields, Body, ValidationError);
        ContentType = TEXT("application/x-www-form-urlencoded");
        break;
    case EOpenPocketBaseCustomBodyFormat::Multipart:
        bBodyValid = (bHasFields || bHasFiles) && !bHasBody &&
            CustomRequest.ContentType.IsEmpty();
        if (bBodyValid)
        {
            FOpenPocketBaseUploadLimits Limits = CustomRequest.UploadLimits;
            Limits.MaxTotalBodyBytes = FMath::Min(
                Limits.MaxTotalBodyBytes, CustomRequest.MaxRequestBytes);
            OpenPocketBase::Multipart::FBuildResult Multipart;
            const FString Boundary = TEXT("openpocketbase-") +
                FGuid::NewGuid().ToString(EGuidFormats::DigitsLower);
            bBodyValid = OpenPocketBase::Multipart::BuildForm(
                CustomRequest.FormFields,
                CustomRequest.Files,
                Limits,
                Boundary,
                Multipart,
                ValidationError);
            if (bBodyValid)
            {
                BodyStream = MoveTemp(Multipart.Stream);
                BodyLength = Multipart.ContentLength;
                ContentType = MoveTemp(Multipart.ContentType);
            }
        }
        break;
    case EOpenPocketBaseCustomBodyFormat::Raw:
    case EOpenPocketBaseCustomBodyFormat::Binary:
        bBodyValid = !bHasFields && !bHasFiles &&
            IsSafeCustomContentType(CustomRequest.ContentType);
        if (bBodyValid)
        {
            Body = MoveTemp(CustomRequest.Body);
            ContentType = MoveTemp(CustomRequest.ContentType);
        }
        break;
    default:
        bBodyValid = false;
        break;
    }

    if (!bBodyValid || Body.Num() > CustomRequest.MaxRequestBytes ||
        BodyLength > CustomRequest.MaxRequestBytes)
    {
        if (!ValidationError.IsSet())
        {
            ValidationError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("The custom route body does not match its bounded body format."));
        }
        DispatchFailure<FOpenPocketBaseCustomRouteResponse>(
            MoveTemp(OnComplete), MoveTemp(ValidationError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseCustomRouteResponse>, ESPMode::ThreadSafe>
        Completion =
            MakeShared<TCompletionState<FOpenPocketBaseCustomRouteResponse>, ESPMode::ThreadSafe>(
                MoveTemp(OnComplete));
    const double StartedAt = Impl->Clock->MonotonicSeconds();
    const TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock = Impl->Clock;
    FOpenPocketBaseHttpRequest Request = Impl->MakeRequest(
        Method,
        MoveTemp(Path),
        MoveTemp(Body),
        CustomRequest.Options,
        CustomRequest.bUseAuth);
    if (!ContentType.IsEmpty())
    {
        Request.Headers.Add(TEXT("Content-Type"), MoveTemp(ContentType));
    }
    if (BodyStream.IsValid())
    {
        Request.BodyStream = MoveTemp(BodyStream);
        Request.BodyLength = BodyLength;
        Request.Headers.Add(TEXT("Content-Length"), LexToString(BodyLength));
    }
    const bool bEligibleRead = CustomRequest.Method == EOpenPocketBaseCustomRouteMethod::Get;
    return Impl->Send(
        MoveTemp(Request),
        CustomRequest.Options,
        bEligibleRead,
        [Completion, Clock, StartedAt](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<FOpenPocketBaseCustomRouteResponse> Result =
                [&Response, &Clock, StartedAt]()
            {
                TOpenPocketBaseResult<bool> Status =
                    OpenPocketBase::Json::ParseEmptyResponse(Response);
                if (!Status.IsSuccess())
                {
                    return TOpenPocketBaseResult<FOpenPocketBaseCustomRouteResponse>::Failure(
                        Status.GetError());
                }

                FOpenPocketBaseCustomRouteResponse CustomResponse;
                CustomResponse.HttpStatus = Response.HttpStatus;
                CustomResponse.RequestId = Response.RequestId;
                CustomResponse.ContentType = FindResponseHeader(Response, TEXT("Content-Type"));
                if (!CustomResponse.ContentType.IsEmpty() &&
                    !IsBoundedPlainText(CustomResponse.ContentType, 127))
                {
                    CustomResponse.ContentType.Reset();
                }
                CustomResponse.DurationSeconds = FMath::Max(
                    0.0, Clock->MonotonicSeconds() - StartedAt);
                CustomResponse.Body = MoveTemp(Response.Body);
                const bool bJsonContent =
                    CustomResponse.ContentType.Contains(
                        TEXT("application/json"), ESearchCase::IgnoreCase) ||
                    CustomResponse.ContentType.Contains(TEXT("+json"), ESearchCase::IgnoreCase);
                if (bJsonContent && !CustomResponse.Body.IsEmpty())
                {
                    FString Json;
                    if (!TryParseJsonValue(
                            CustomResponse.Body,
                            CustomResponse.ParsedJson,
                            Json))
                    {
                        return TOpenPocketBaseResult<FOpenPocketBaseCustomRouteResponse>::Failure(
                            MakeResponseSerializationError(
                                Response,
                                TEXT("The custom route returned invalid JSON.")));
                    }
                    CustomResponse.bHasJson = true;
                    switch (CustomResponse.ParsedJson->Type)
                    {
                    case EJson::Object:
                        CustomResponse.JsonRootType = EOpenPocketBaseJsonRootType::Object;
                        CustomResponse.JsonBody.JsonObject =
                            CustomResponse.ParsedJson->AsObject();
                        CustomResponse.JsonBody.JsonString = MoveTemp(Json);
                        break;
                    case EJson::Array:
                        CustomResponse.JsonRootType = EOpenPocketBaseJsonRootType::Array;
                        break;
                    default:
                        CustomResponse.JsonRootType = EOpenPocketBaseJsonRootType::Scalar;
                        break;
                    }
                }
                return TOpenPocketBaseResult<FOpenPocketBaseCustomRouteResponse>::Success(
                    MoveTemp(CustomResponse));
            }();
            const EOpenPocketBaseRequestState Terminal = TerminalStateFor(Result.IsSuccess());
            State->TryComplete(
                Terminal,
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion]()
        {
            Completion->Invoke(
                TOpenPocketBaseResult<FOpenPocketBaseCustomRouteResponse>::Failure(
                    MakeCancelledError()));
        });
}

FString FOpenPocketBaseClient::GetBaseUrl() const
{
    return Impl->BaseUrl;
}

FOpenPocketBaseCapabilityInfo FOpenPocketBaseClient::GetCapability(
    const EOpenPocketBaseCapability Capability) const
{
    FOpenPocketBaseCapabilityInfo Info;
    Info.Capability = Capability;
    Info.Platform = FPlatformProperties::PlatformName();
    Info.BuildConfiguration = GetBuildConfigurationName();

    switch (Capability)
    {
    case EOpenPocketBaseCapability::HttpStreaming:
        Info.Status = Impl->Transport->IsIncrementalResponseStreamingAvailable(Info.Reason)
            ? EOpenPocketBaseCapabilityStatus::Supported
            : EOpenPocketBaseCapabilityStatus::Unsupported;
        break;
    case EOpenPocketBaseCapability::SecurePersistence:
        Info.Status = Impl->SecureStore->IsAvailable(Info.Reason)
            ? EOpenPocketBaseCapabilityStatus::Supported
            : EOpenPocketBaseCapabilityStatus::Unavailable;
        if (Info.Reason.IsEmpty())
        {
            Info.Reason = Info.IsSupported()
                ? TEXT("A platform secure store is available.")
                : TEXT("A platform secure store is unavailable.");
        }
        break;
    case EOpenPocketBaseCapability::OAuthCallback:
        if (!Impl->Config.bEnableAssistedOAuth)
        {
            Info.Status = EOpenPocketBaseCapabilityStatus::DisabledByPolicy;
            Info.Reason = TEXT("Assisted OAuth requires explicit client configuration.");
            break;
        }
        if (!Impl->OAuthBrowser->IsAvailable(Info.Reason))
        {
            Info.Status = EOpenPocketBaseCapabilityStatus::Unsupported;
            break;
        }
        if (!Impl->OAuthBrowser->IsPlatformFlowValidated(Info.Reason))
        {
            Info.Status = EOpenPocketBaseCapabilityStatus::RequiresConfiguration;
            break;
        }
        {
            FString StreamingReason;
            if (!Impl->Transport->IsIncrementalResponseStreamingAvailable(StreamingReason))
            {
                Info.Status = EOpenPocketBaseCapabilityStatus::Unsupported;
                Info.Reason = TEXT("Assisted OAuth requires incremental realtime streaming.");
                break;
            }
        }
        Info.Status = EOpenPocketBaseCapabilityStatus::Supported;
        if (Info.Reason.IsEmpty())
        {
            Info.Reason = TEXT("The assisted OAuth browser and realtime handoff are available.");
        }
        break;
    case EOpenPocketBaseCapability::OfflineModule:
        Info.Status = EOpenPocketBaseCapabilityStatus::Unsupported;
        Info.Reason = TEXT("The optional mutation outbox is not implemented.");
        break;
    case EOpenPocketBaseCapability::EditorMock:
#if WITH_EDITOR
        Info.Status = EOpenPocketBaseCapabilityStatus::Supported;
        Info.Reason = TEXT("The scripted mock transport is available in Editor builds.");
#else
        Info.Status = EOpenPocketBaseCapabilityStatus::Unavailable;
        Info.Reason = TEXT("The scripted mock transport is available only in Editor builds.");
#endif
        break;
    case EOpenPocketBaseCapability::PrivilegedModule:
        Info.Status = EOpenPocketBaseCapabilityStatus::Unsupported;
        Info.Reason = TEXT("The optional privileged API module is not implemented.");
        break;
    default:
        Info.Status = EOpenPocketBaseCapabilityStatus::Unsupported;
        Info.Reason = TEXT("The requested capability is not recognized.");
        break;
    }
    return Info;
}

FOpenPocketBaseCapabilityReport FOpenPocketBaseClient::GetCapabilityReport() const
{
    static constexpr EOpenPocketBaseCapability Capabilities[] = {
        EOpenPocketBaseCapability::HttpStreaming,
        EOpenPocketBaseCapability::SecurePersistence,
        EOpenPocketBaseCapability::OAuthCallback,
        EOpenPocketBaseCapability::OfflineModule,
        EOpenPocketBaseCapability::EditorMock,
        EOpenPocketBaseCapability::PrivilegedModule
    };

    FOpenPocketBaseCapabilityReport Report;
    Report.Entries.Reserve(UE_ARRAY_COUNT(Capabilities));
    for (const EOpenPocketBaseCapability Capability : Capabilities)
    {
        Report.Entries.Add(GetCapability(Capability));
    }
    return Report;
}

bool FOpenPocketBaseClient::IsAuthenticated() const
{
    FScopeLock Lock(&Impl->AuthMutex);
    return !Impl->AuthToken.IsEmpty() && Impl->bHasAuthRecord;
}

bool FOpenPocketBaseClient::GetCurrentAuthRecord(FOpenPocketBaseRecord& OutRecord) const
{
    FScopeLock Lock(&Impl->AuthMutex);
    if (!Impl->bHasAuthRecord)
    {
        return false;
    }
    OutRecord = Impl->AuthRecord;
    return true;
}

bool FOpenPocketBaseClient::GetCurrentSession(FOpenPocketBaseSessionSnapshot& OutSession) const
{
    FScopeLock Lock(&Impl->AuthMutex);
    OutSession = FOpenPocketBaseSessionSnapshot();
    OutSession.bAuthenticated = !Impl->AuthToken.IsEmpty() && Impl->bHasAuthRecord;
    OutSession.AuthCollection = Impl->AuthCollection;
    OutSession.AuthGeneration = Impl->AuthGeneration;
    OutSession.PersistenceState = Impl->PersistenceState;
    if (Impl->bHasAuthRecord)
    {
        OutSession.AuthRecord = Impl->AuthRecord;
    }
    return OutSession.bAuthenticated;
}

FOpenPocketBaseSessionChanged& FOpenPocketBaseClient::OnSessionChanged()
{
    return Impl->SessionEvents->Changed;
}

void FOpenPocketBaseClient::Logout()
{
    Impl->ClearAuth();
}

FOpenPocketBaseRequestHandle FOpenPocketBaseClient::RefreshAuth(
    FOpenPocketBaseAuthCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!Impl->bAuthRefreshAllowed)
    {
        DispatchFailure<FOpenPocketBaseAuthResult>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::Unsupported,
                TEXT("Ephemeral impersonation sessions cannot be refreshed.")));
        return {};
    }
    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseAuthResult>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    FString CapturedToken;
    FString AuthCollection;
    int64 CapturedGeneration = 0;
    {
        FScopeLock Lock(&Impl->AuthMutex);
        if (Impl->AuthToken.IsEmpty() || Impl->AuthCollection.IsEmpty() || !Impl->bHasAuthRecord)
        {
            DispatchFailure<FOpenPocketBaseAuthResult>(
                MoveTemp(OnComplete),
                MakeLocalError(
                    EOpenPocketBaseErrorKind::Authentication,
                    TEXT("An authenticated session is required for Auth Refresh.")));
            return {};
        }
        CapturedToken = Impl->AuthToken;
        AuthCollection = Impl->AuthCollection;
        CapturedGeneration = Impl->AuthGeneration;
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseAuthResult>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseAuthResult>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> RequestState =
        Impl->CreateCompositeState(
            [Completion]()
            {
                Completion->Invoke(
                    TOpenPocketBaseResult<FOpenPocketBaseAuthResult>::Failure(MakeCancelledError()));
            });

    TSharedRef<FImpl::FRefreshFlight, ESPMode::ThreadSafe> Flight =
        MakeShared<FImpl::FRefreshFlight, ESPMode::ThreadSafe>();
    bool bStartsFlight = false;
    {
        FScopeLock Lock(&Impl->RefreshMutex);
        if (Impl->ActiveRefresh.IsValid() &&
            Impl->ActiveRefresh->CapturedGeneration == CapturedGeneration &&
            Impl->ActiveRefresh->CapturedToken == CapturedToken &&
            Impl->ActiveRefresh->AuthCollection == AuthCollection)
        {
            Flight = Impl->ActiveRefresh.ToSharedRef();
        }
        else
        {
            Flight->CapturedGeneration = CapturedGeneration;
            Flight->CapturedToken = CapturedToken;
            Flight->AuthCollection = AuthCollection;
            Impl->ActiveRefresh = Flight;
            bStartsFlight = true;
        }
        Flight->Waiters.Add({RequestState, Completion});
    }

    if (!bStartsFlight)
    {
        return Impl->MakeRequestHandle(RequestState);
    }

    const FString Path = FString::Printf(
        TEXT("/api/collections/%s/auth-refresh"),
        *EncodeSegment(AuthCollection));
    FOpenPocketBaseHttpRequest Request = Impl->MakeRequest(
        TEXT("POST"), Path, {}, Options, false);
    Request.Headers.Add(TEXT("Authorization"), CapturedToken);
    const TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> WeakClient = AsShared();
    FOpenPocketBaseRequestHandle ChildHandle = Impl->Send(
        MoveTemp(Request),
        Options,
        false,
        [WeakClient, Flight](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            FString Token;
            TOpenPocketBaseResult<FOpenPocketBaseAuthResult> Result =
                OpenPocketBase::Json::ParseAuthResponse(Response, Token);
            if (Result.IsSuccess())
            {
                const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client = WeakClient.Pin();
                FOpenPocketBaseError StoreError;
                if (!Client.IsValid() || Client->IsShutdown())
                {
                    StoreError = MakeLocalError(
                        EOpenPocketBaseErrorKind::Authentication,
                        TEXT("The session changed while Auth Refresh was in flight."));
                }
                else
                {
                    Client->Impl->TryStoreRefreshedAuth(
                        Flight->CapturedGeneration,
                        Flight->CapturedToken,
                        Flight->AuthCollection,
                        MoveTemp(Token),
                        Result.GetValue().Record,
                        StoreError);
                }
                if (StoreError.IsSet())
                {
                    Result = TOpenPocketBaseResult<FOpenPocketBaseAuthResult>::Failure(
                        MoveTemp(StoreError));
                }
            }

            const EOpenPocketBaseRequestState Terminal = TerminalStateFor(Result.IsSuccess());
            State->TryComplete(
                Terminal,
                [WeakClient, Flight, Result = MoveTemp(Result)]() mutable
                {
                    if (const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client = WeakClient.Pin())
                    {
                        Client->Impl->FinishRefresh(Flight, MoveTemp(Result));
                    }
                });
        },
        [WeakClient, Flight]()
        {
            if (const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client = WeakClient.Pin())
            {
                Client->Impl->FinishRefresh(
                    Flight,
                    TOpenPocketBaseResult<FOpenPocketBaseAuthResult>::Failure(MakeCancelledError()));
            }
        },
        false);
    {
        FScopeLock Lock(&Impl->RefreshMutex);
        Flight->ChildHandle = MoveTemp(ChildHandle);
    }
    return Impl->MakeRequestHandle(RequestState);
}

FOpenPocketBaseRequestHandle FOpenPocketBaseClient::RestoreSession(
    const bool bVerifyWithServer,
    FOpenPocketBaseSessionRestoreCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    FOpenPocketBaseSessionRestoreResult RestoreResult;
    FOpenPocketBaseError RestoreError;
    if (!Impl->RestorePersistedSession(RestoreResult, RestoreError))
    {
        DispatchFailure<FOpenPocketBaseSessionRestoreResult>(
            MoveTemp(OnComplete),
            MoveTemp(RestoreError));
        return {};
    }

    if (!bVerifyWithServer || RestoreResult.Status != EOpenPocketBaseSessionRestoreStatus::Restored)
    {
        DispatchSuccess<FOpenPocketBaseSessionRestoreResult>(
            MoveTemp(OnComplete),
            MoveTemp(RestoreResult));
        return {};
    }

    return RefreshAuth(
        [WeakClient = TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe>(AsShared()),
         OnComplete = MoveTemp(OnComplete)](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result) mutable
        {
            const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client = WeakClient.Pin();
            if (!Client.IsValid())
            {
                if (OnComplete)
                {
                    OnComplete(TOpenPocketBaseResult<FOpenPocketBaseSessionRestoreResult>::Failure(
                        MakeCancelledError()));
                }
                return;
            }

            if (!Result.IsSuccess())
            {
                const FOpenPocketBaseError Error = Result.GetError();
                if (Error.HttpStatus == 401 || Error.HttpStatus == 403)
                {
                    Client->Logout();
                    FOpenPocketBaseSessionRestoreResult Expired;
                    Expired.Status = EOpenPocketBaseSessionRestoreStatus::Expired;
                    Client->GetCurrentSession(Expired.Session);
                    if (OnComplete)
                    {
                        OnComplete(TOpenPocketBaseResult<FOpenPocketBaseSessionRestoreResult>::Success(
                            MoveTemp(Expired)));
                    }
                }
                else if (OnComplete)
                {
                    OnComplete(TOpenPocketBaseResult<FOpenPocketBaseSessionRestoreResult>::Failure(Error));
                }
                return;
            }

            FOpenPocketBaseSessionRestoreResult Verified;
            Verified.Status = EOpenPocketBaseSessionRestoreStatus::Verified;
            Client->GetCurrentSession(Verified.Session);
            if (OnComplete)
            {
                OnComplete(TOpenPocketBaseResult<FOpenPocketBaseSessionRestoreResult>::Success(
                    MoveTemp(Verified)));
            }
        },
        MoveTemp(Options));
}

FOpenPocketBaseRequestHandle FOpenPocketBaseClient::SendBatch(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseBatchCallback OnComplete,
    FOpenPocketBaseBatchOptions Options)
{
    FOpenPocketBaseError ValidationError;
    if (!ValidateRequestOptions(Options.RequestOptions, ValidationError) ||
        !ValidateBatch(Batch, Options, ValidationError))
    {
        DispatchFailure<FOpenPocketBaseBatchResult>(MoveTemp(OnComplete), MoveTemp(ValidationError));
        return {};
    }

    TArray<uint8> Body = SerializeBatch(Batch);
    if (Body.Num() > Options.MaxBodyBytes)
    {
        DispatchFailure<FOpenPocketBaseBatchResult>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("The serialized batch exceeds the configured body bound.")));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseBatchResult>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseBatchResult>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    FOpenPocketBaseHttpRequest Request = Impl->MakeRequest(
        TEXT("POST"), TEXT("/api/batch"), MoveTemp(Body), Options.RequestOptions, true);
    const TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> WeakClient = AsShared();
    return Impl->Send(
        MoveTemp(Request),
        Options.RequestOptions,
        false,
        [Completion, WeakClient, Batch = MoveTemp(Batch)](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<FOpenPocketBaseBatchResult> Result =
                OpenPocketBase::Json::ParseBatchResponse(Response, Batch);
            if (Result.IsSuccess())
            {
                if (const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client = WeakClient.Pin())
                {
                    FOpenPocketBaseError StoreError;
                    if (!Client->Impl->TrySynchronizeBatchAuthRecord(
                            Batch,
                            Result.GetValue(),
                            StoreError))
                    {
                        Result = TOpenPocketBaseResult<FOpenPocketBaseBatchResult>::Failure(
                            MoveTemp(StoreError));
                    }
                }
            }
            const EOpenPocketBaseRequestState Terminal = TerminalStateFor(Result.IsSuccess());
            State->TryComplete(
                Terminal,
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion]()
        {
            Completion->Invoke(
                TOpenPocketBaseResult<FOpenPocketBaseBatchResult>::Failure(MakeCancelledError()));
        });
}

FOpenPocketBaseSubscriptionResult FOpenPocketBaseClient::Subscribe(
    FString Topic,
    FOpenPocketBaseRealtimeCallbacks Callbacks,
    FOpenPocketBaseRealtimeOptions Options)
{
    if (IsShutdown() || !Impl->Realtime.IsValid())
    {
        return FOpenPocketBaseSubscriptionResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::Cancelled,
            TEXT("The client has shut down.")));
    }
    const FOpenPocketBaseCapabilityInfo Streaming =
        GetCapability(EOpenPocketBaseCapability::HttpStreaming);
    if (!Streaming.IsSupported())
    {
        return FOpenPocketBaseSubscriptionResult::Failure(
            MakeLocalError(EOpenPocketBaseErrorKind::Unsupported, *Streaming.Reason));
    }
    FOpenPocketBaseError Error;
    FOpenPocketBaseSubscriptionHandle Subscription = Impl->Realtime->Subscribe(
        MoveTemp(Topic),
        MoveTemp(Callbacks),
        MoveTemp(Options),
        Error);
    if (!Subscription.IsActive())
    {
        if (!Error.IsSet())
        {
            Error = MakeLocalError(
                EOpenPocketBaseErrorKind::Transport,
                TEXT("The realtime subscription could not be started."));
        }
        return FOpenPocketBaseSubscriptionResult::Failure(MoveTemp(Error));
    }
    return FOpenPocketBaseSubscriptionResult::Success(MoveTemp(Subscription));
}

void FOpenPocketBaseClient::UnsubscribeTopic(const FString& Topic)
{
    if (Impl->Realtime.IsValid())
    {
        Impl->Realtime->UnsubscribeTopic(Topic);
    }
}

void FOpenPocketBaseClient::UnsubscribeAllRealtime()
{
    if (Impl->Realtime.IsValid())
    {
        Impl->Realtime->UnsubscribeAll();
    }
}

bool FOpenPocketBaseClient::IsShutdown() const
{
    return Impl->bShutdown.load(std::memory_order_acquire);
}

void FOpenPocketBaseClient::Shutdown()
{
    if (Impl)
    {
        Impl->Shutdown();
    }
}

FOpenPocketBaseFileService::FOpenPocketBaseFileService(
    TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient)
    : Client(MoveTemp(InClient))
{
}

bool FOpenPocketBaseFileService::IsValid() const
{
    return Client.IsValid();
}

FOpenPocketBaseFileUrlResult FOpenPocketBaseFileService::BuildUrl(
    FString InCollection,
    FString RecordId,
    FString FileName,
    FOpenPocketBaseFileUrlOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown() ||
        !IsSafePathSegment(InCollection) || !IsSafePathSegment(RecordId) ||
        !IsSafePathSegment(FileName))
    {
        return FOpenPocketBaseFileUrlResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("A ready client and safe collection, record ID, and filename are required.")));
    }

    FString Thumbnail;
    const bool bHasThumbnailSize = Options.Thumbnail.Width != 0 || Options.Thumbnail.Height != 0;
    if (Options.Thumbnail.Width < 0 || Options.Thumbnail.Width > 16384 ||
        Options.Thumbnail.Height < 0 || Options.Thumbnail.Height > 16384 ||
        (Options.Thumbnail.Mode == EOpenPocketBaseThumbnailMode::None && bHasThumbnailSize) ||
        (Options.Thumbnail.Mode != EOpenPocketBaseThumbnailMode::None && !bHasThumbnailSize))
    {
        return FOpenPocketBaseFileUrlResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Thumbnail dimensions and crop mode are invalid.")));
    }

    if (Options.Thumbnail.Mode != EOpenPocketBaseThumbnailMode::None)
    {
        const TCHAR* Suffix = TEXT("");
        switch (Options.Thumbnail.Mode)
        {
        case EOpenPocketBaseThumbnailMode::CropTop:
            Suffix = TEXT("t");
            break;
        case EOpenPocketBaseThumbnailMode::CropBottom:
            Suffix = TEXT("b");
            break;
        case EOpenPocketBaseThumbnailMode::Fit:
            Suffix = TEXT("f");
            break;
        default:
            break;
        }
        Thumbnail = FString::Printf(
            TEXT("%dx%d%s"),
            Options.Thumbnail.Width,
            Options.Thumbnail.Height,
            Suffix);
    }

    TArray<FString> QueryParts;
    AddQueryValue(QueryParts, TEXT("thumb"), Thumbnail);
    if (Options.bForceDownload)
    {
        QueryParts.Add(TEXT("download=true"));
    }
    FString Path = FString::Printf(
        TEXT("/api/files/%s/%s/%s"),
        *EncodeSegment(InCollection),
        *EncodeSegment(RecordId),
        *EncodeSegment(FileName));
    return FOpenPocketBaseFileUrlResult::Success(
        PinnedClient->GetBaseUrl() + AddQuery(MoveTemp(Path), FString::Join(QueryParts, TEXT("&"))));
}

FOpenPocketBaseRequestHandle FOpenPocketBaseFileService::GetToken(
    FOpenPocketBaseFileTokenCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown() || !PinnedClient->IsAuthenticated())
    {
        DispatchFailure<FOpenPocketBaseFileToken>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("An authenticated PocketBase client is required.")));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseFileToken>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseFileToken>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseFileToken>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("POST"),
        TEXT("/api/files/token"),
        {},
        Options,
        true);

    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options,
        true,
        [Completion](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<FOpenPocketBaseFileToken> Result = [&Response]()
            {
                TOpenPocketBaseResult<bool> Status = OpenPocketBase::Json::ParseEmptyResponse(Response);
                if (!Status.IsSuccess())
                {
                    return TOpenPocketBaseResult<FOpenPocketBaseFileToken>::Failure(Status.GetError());
                }
                if (Response.Body.IsEmpty())
                {
                    return TOpenPocketBaseResult<FOpenPocketBaseFileToken>::Failure(
                        MakeLocalError(
                            EOpenPocketBaseErrorKind::Serialization,
                            TEXT("PocketBase returned an invalid file token response.")));
                }

                const FUTF8ToTCHAR Converted(
                    reinterpret_cast<const ANSICHAR*>(Response.Body.GetData()),
                    Response.Body.Num());
                const FString Json(Converted.Length(), Converted.Get());
                TSharedPtr<FJsonObject> Object;
                const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
                FString TokenValue;
                if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid() ||
                    !Object->TryGetStringField(TEXT("token"), TokenValue) ||
                    TokenValue.IsEmpty() || TokenValue.Len() > 4096)
                {
                    return TOpenPocketBaseResult<FOpenPocketBaseFileToken>::Failure(
                        MakeLocalError(
                            EOpenPocketBaseErrorKind::Serialization,
                            TEXT("PocketBase returned an invalid file token response.")));
                }
                for (const TCHAR Character : TokenValue)
                {
                    if (FChar::IsControl(Character))
                    {
                        return TOpenPocketBaseResult<FOpenPocketBaseFileToken>::Failure(
                            MakeLocalError(
                                EOpenPocketBaseErrorKind::Serialization,
                                TEXT("PocketBase returned an invalid file token response.")));
                    }
                }

                FOpenPocketBaseFileToken Token;
                Token.Value = MoveTemp(TokenValue);
                return TOpenPocketBaseResult<FOpenPocketBaseFileToken>::Success(MoveTemp(Token));
            }();
            const EOpenPocketBaseRequestState Terminal = TerminalStateFor(Result.IsSuccess());
            State->TryComplete(
                Terminal,
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion]()
        {
            Completion->Invoke(TOpenPocketBaseResult<FOpenPocketBaseFileToken>::Failure(MakeCancelledError()));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseFileService::Download(
    FString InCollection,
    FString RecordId,
    FString FileName,
    FOpenPocketBaseFileDownloadOptions Options,
    FOpenPocketBaseFileDownloadCallback OnComplete,
    FOpenPocketBaseFileToken Token,
    FOpenPocketBaseTransferProgressCallback OnProgress) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        DispatchFailure<FOpenPocketBaseFileDownloadResult>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A ready PocketBase client is required.")));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options.RequestOptions, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseFileDownloadResult>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    FOpenPocketBaseFileUrlResult UrlResult = BuildUrl(
        InCollection,
        RecordId,
        FileName,
        Options.UrlOptions);
    if (!UrlResult.IsSuccess())
    {
        DispatchFailure<FOpenPocketBaseFileDownloadResult>(
            MoveTemp(OnComplete),
            UrlResult.GetError());
        return {};
    }
    FString Url = UrlResult.TakeValue();

    FString ProtectedToken;
    if (Token.IsSet())
    {
        bool bTokenValid = Token.Value.Len() <= 4096;
        for (const TCHAR Character : Token.Value)
        {
            bTokenValid = bTokenValid && !FChar::IsControl(Character);
        }
        if (!bTokenValid)
        {
            DispatchFailure<FOpenPocketBaseFileDownloadResult>(
                MoveTemp(OnComplete),
                MakeLocalError(
                    EOpenPocketBaseErrorKind::InvalidArgument,
                    TEXT("The protected-file token is invalid.")));
            return {};
        }
        Url += Url.Contains(TEXT("?")) ? TEXT("&token=") : TEXT("?token=");
        Url += FGenericPlatformHttp::UrlEncode(Token.Value);
        ProtectedToken = MoveTemp(Token.Value);
    }

    FOpenPocketBaseError SinkError;
    const TSharedPtr<FOpenPocketBaseDownloadSink, ESPMode::ThreadSafe> Sink =
        FOpenPocketBaseDownloadSink::Create(Options, FileName, SinkError);
    if (!Sink.IsValid())
    {
        DispatchFailure<FOpenPocketBaseFileDownloadResult>(MoveTemp(OnComplete), MoveTemp(SinkError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseFileDownloadResult>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseFileDownloadResult>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const TSharedPtr<FOpenPocketBaseTransferProgressState, ESPMode::ThreadSafe> Progress =
        FOpenPocketBaseTransferProgressState::Create(
            PinnedClient->Impl->Clock,
            MoveTemp(OnProgress));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("GET"),
        {},
        {},
        Options.RequestOptions,
        false);
    Request.Url = MoveTemp(Url);
    Request.bStreamResponse = true;

    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options.RequestOptions,
        false,
        [Completion, Sink, Progress, ProtectedToken = MoveTemp(ProtectedToken)](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult> Result = [&Response, &Sink]()
            {
                TOpenPocketBaseResult<bool> Status = OpenPocketBase::Json::ParseEmptyResponse(Response);
                if (!Status.IsSuccess())
                {
                    Sink->Abort();
                    return TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>::Failure(
                        Status.GetError());
                }

                FOpenPocketBaseFileDownloadResult DownloadResult;
                FOpenPocketBaseError DownloadError;
                if (!Sink->Finalize(Response, DownloadResult, DownloadError))
                {
                    return TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>::Failure(
                        MoveTemp(DownloadError));
                }
                return TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>::Success(
                    MoveTemp(DownloadResult));
            }();
            if (!Result.IsSuccess() && !ProtectedToken.IsEmpty())
            {
                Result = TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>::Failure(
                    SanitizeProtectedFileError(Result.GetError()));
            }
            if (Progress.IsValid())
            {
                if (Result.IsSuccess())
                {
                    Progress->Finish(
                        Result.GetValue().ContentLength,
                        Result.GetValue().ContentLength);
                }
                else
                {
                    Progress->Stop();
                }
            }
            const EOpenPocketBaseRequestState Terminal = TerminalStateFor(Result.IsSuccess());
            State->TryComplete(
                Terminal,
                [Completion, Progress, Result = MoveTemp(Result)]() mutable
                {
                    if (Progress.IsValid())
                    {
                        Progress->Stop();
                    }
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion, Sink, Progress]()
        {
            Sink->Abort();
            if (Progress.IsValid())
            {
                Progress->Stop();
            }
            Completion->Invoke(
                TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>::Failure(
                    MakeCancelledError()));
        },
        false,
        [Sink, Progress](const TArrayView<const uint8> Chunk)
        {
            Sink->Receive(Chunk);
            if (Progress.IsValid())
            {
                Progress->Report(
                    Sink->GetTransferredBytes(),
                    {},
                    EOpenPocketBaseTransferPhase::Downloading);
            }
        });
}

FOpenPocketBaseCollectionService::FOpenPocketBaseCollectionService(
    TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient,
    FString InCollection)
    : Client(MoveTemp(InClient))
    , Collection(MoveTemp(InCollection))
{
}

bool FOpenPocketBaseCollectionService::IsValid() const
{
    return IsSafePathSegment(Collection) && Client.IsValid();
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::GetOne(
    FString RecordId,
    FOpenPocketBaseRecordCallback OnComplete,
    FOpenPocketBaseRecordOptions Options) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) || !IsSafePathSegment(RecordId))
    {
        DispatchFailure<FOpenPocketBaseRecord>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument, TEXT("Client, collection, and record ID are required.")));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options.RequestOptions, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseRecord>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseRecord>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseRecord>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const FString Path = AddQuery(
        FString::Printf(
            TEXT("/api/collections/%s/records/%s"),
            *EncodeSegment(Collection),
            *EncodeSegment(RecordId)),
        MakeRecordQuery(Options));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("GET"), Path, {}, Options.RequestOptions, true);

    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options.RequestOptions,
        true,
        [Completion](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<FOpenPocketBaseRecord> Result =
                OpenPocketBase::Json::ParseRecordResponse(Response);
            const EOpenPocketBaseRequestState Terminal = TerminalStateFor(Result.IsSuccess());
            State->TryComplete(
                Terminal,
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion]()
        {
            Completion->Invoke(TOpenPocketBaseResult<FOpenPocketBaseRecord>::Failure(MakeCancelledError()));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::GetList(
    FOpenPocketBaseListOptions Options,
    FOpenPocketBaseRecordPageCallback OnComplete) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) || Options.Page < 1 ||
        Options.PerPage < 1 || !Options.Filter.IsValid())
    {
        DispatchFailure<FOpenPocketBaseRecordPage>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                Options.Filter.IsValid()
                    ? TEXT("Client, collection, page, and per-page values are required.")
                    : *Options.Filter.ErrorMessage));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options.RequestOptions, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseRecordPage>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseRecordPage>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseRecordPage>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const FString Path = FString::Printf(
        TEXT("/api/collections/%s/records?%s"),
        *EncodeSegment(Collection),
        *MakeListQuery(Options));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("GET"), Path, {}, Options.RequestOptions, true);

    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options.RequestOptions,
        true,
        [Completion](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<FOpenPocketBaseRecordPage> Result =
                OpenPocketBase::Json::ParseRecordPageResponse(Response);
            const EOpenPocketBaseRequestState Terminal = TerminalStateFor(Result.IsSuccess());
            State->TryComplete(
                Terminal,
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion]()
        {
            Completion->Invoke(TOpenPocketBaseResult<FOpenPocketBaseRecordPage>::Failure(MakeCancelledError()));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::GetFullList(
    FOpenPocketBaseFullListOptions Options,
    FOpenPocketBaseFullListCallback OnComplete) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    const bool bBoundsValid = (Options.MaxItems > 0 || Options.MaxPages > 0) &&
        Options.MaxItems >= 0 && Options.MaxItems <= 1000000 &&
        Options.MaxPages >= 0 && Options.MaxPages <= 10000;
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) ||
        Options.ListOptions.Page != 1 || Options.ListOptions.PerPage < 1 || !bBoundsValid)
    {
        DispatchFailure<FOpenPocketBaseFullListResult>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Full-list traversal requires a client, collection, first page, and an explicit item or page bound.")));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options.ListOptions.RequestOptions, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseFullListResult>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseFullListResult>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseFullListResult>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> RequestState =
        PinnedClient->Impl->CreateCompositeState(
            [Completion]()
            {
                Completion->Invoke(
                    TOpenPocketBaseResult<FOpenPocketBaseFullListResult>::Failure(MakeCancelledError()));
            });
    const FOpenPocketBaseRequestHandle Handle = PinnedClient->Impl->MakeRequestHandle(RequestState);
    const TSharedRef<FOpenPocketBaseFullListOperation, ESPMode::ThreadSafe> Operation =
        MakeShared<FOpenPocketBaseFullListOperation, ESPMode::ThreadSafe>(
            PinnedClient,
            Collection,
            MoveTemp(Options),
            RequestState,
            Completion);
    Operation->Start();
    return Handle;
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::GetFirstListItem(
    FOpenPocketBaseFilter Filter,
    FOpenPocketBaseRecordCallback OnComplete,
    FOpenPocketBaseRecordOptions Options) const
{
    FOpenPocketBaseListOptions ListOptions;
    ListOptions.Page = 1;
    ListOptions.PerPage = 1;
    ListOptions.Filter = MoveTemp(Filter);
    ListOptions.Expand = MoveTemp(Options.Expand);
    ListOptions.Fields = MoveTemp(Options.Fields);
    ListOptions.bSkipTotal = true;
    ListOptions.RequestOptions = MoveTemp(Options.RequestOptions);

    return GetList(
        MoveTemp(ListOptions),
        [OnComplete = MoveTemp(OnComplete)](
            TOpenPocketBaseResult<FOpenPocketBaseRecordPage>&& Result) mutable
        {
            if (!OnComplete)
            {
                return;
            }
            if (!Result.IsSuccess())
            {
                OnComplete(TOpenPocketBaseResult<FOpenPocketBaseRecord>::Failure(Result.GetError()));
                return;
            }
            if (Result.GetValue().Items.IsEmpty())
            {
                FOpenPocketBaseError Error;
                Error.Kind = EOpenPocketBaseErrorKind::PocketBase;
                Error.HttpStatus = 404;
                Error.ServerCode = TEXT("404");
                Error.ServerMessage = TEXT("The requested record wasn't found.");
                OnComplete(TOpenPocketBaseResult<FOpenPocketBaseRecord>::Failure(MoveTemp(Error)));
                return;
            }
            OnComplete(TOpenPocketBaseResult<FOpenPocketBaseRecord>::Success(Result.GetValue().Items[0]));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::Create(
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordCallback OnComplete,
    FOpenPocketBaseRecordOptions Options) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) || !Body.Data.JsonObject.IsValid())
    {
        DispatchFailure<FOpenPocketBaseRecord>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Client, collection, and a JSON record body are required.")));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options.RequestOptions, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseRecord>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseRecord>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseRecord>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const FString Path = AddQuery(
        FString::Printf(
            TEXT("/api/collections/%s/records"),
            *EncodeSegment(Collection)),
        MakeRecordQuery(Options));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("POST"),
        Path,
        OpenPocketBase::Json::SerializeObject(Body.Data.JsonObject.ToSharedRef()),
        Options.RequestOptions,
        true);

    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options.RequestOptions,
        false,
        [Completion](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<FOpenPocketBaseRecord> Result =
                OpenPocketBase::Json::ParseRecordResponse(Response);
            const EOpenPocketBaseRequestState Terminal = TerminalStateFor(Result.IsSuccess());
            State->TryComplete(
                Terminal,
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion]()
        {
            Completion->Invoke(TOpenPocketBaseResult<FOpenPocketBaseRecord>::Failure(MakeCancelledError()));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::CreateWithFiles(
    FOpenPocketBaseRecordBody Body,
    TArray<FOpenPocketBaseFileInput> Files,
    FOpenPocketBaseRecordCallback OnComplete,
    FOpenPocketBaseRecordOptions Options,
    FOpenPocketBaseUploadLimits Limits,
    FOpenPocketBaseTransferProgressCallback OnProgress) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) || !Body.Data.JsonObject.IsValid() ||
        Files.IsEmpty())
    {
        DispatchFailure<FOpenPocketBaseRecord>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Client, collection, JSON record body, and at least one file are required.")));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options.RequestOptions, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseRecord>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    OpenPocketBase::Multipart::FBuildResult Multipart;
    FOpenPocketBaseError MultipartError;
    const FString Boundary = TEXT("openpocketbase-") +
        FGuid::NewGuid().ToString(EGuidFormats::DigitsLower);
    if (!OpenPocketBase::Multipart::Build(
            Body,
            Files,
            Limits,
            Boundary,
            Multipart,
            MultipartError))
    {
        DispatchFailure<FOpenPocketBaseRecord>(MoveTemp(OnComplete), MoveTemp(MultipartError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseRecord>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseRecord>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const TSharedPtr<FOpenPocketBaseTransferProgressState, ESPMode::ThreadSafe> Progress =
        FOpenPocketBaseTransferProgressState::Create(
            PinnedClient->Impl->Clock,
            MoveTemp(OnProgress));
    const FString Path = AddQuery(
        FString::Printf(
            TEXT("/api/collections/%s/records"),
            *EncodeSegment(Collection)),
        MakeRecordQuery(Options));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("POST"),
        Path,
        {},
        Options.RequestOptions,
        true);
    Request.BodyStream = MoveTemp(Multipart.Stream);
    Request.BodyLength = Multipart.ContentLength;
    if (Progress.IsValid())
    {
        Request.BodyStream = CreateOpenPocketBaseProgressArchive(
            MoveTemp(Request.BodyStream),
            Progress.ToSharedRef());
    }
    Request.Headers.Add(TEXT("Content-Type"), MoveTemp(Multipart.ContentType));
    Request.Headers.Add(TEXT("Content-Length"), LexToString(Request.BodyLength));
    const int64 TotalBytes = Request.BodyLength;

    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options.RequestOptions,
        false,
        [Completion, Progress, TotalBytes](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<FOpenPocketBaseRecord> Result =
                OpenPocketBase::Json::ParseRecordResponse(Response);
            if (Progress.IsValid())
            {
                if (Result.IsSuccess())
                {
                    Progress->Finish(TotalBytes, TotalBytes);
                }
                else
                {
                    Progress->Stop();
                }
            }
            const EOpenPocketBaseRequestState Terminal = TerminalStateFor(Result.IsSuccess());
            State->TryComplete(
                Terminal,
                [Completion, Progress, Result = MoveTemp(Result)]() mutable
                {
                    if (Progress.IsValid())
                    {
                        Progress->Stop();
                    }
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion, Progress]()
        {
            if (Progress.IsValid())
            {
                Progress->Stop();
            }
            Completion->Invoke(TOpenPocketBaseResult<FOpenPocketBaseRecord>::Failure(MakeCancelledError()));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::Update(
    FString RecordId,
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordCallback OnComplete,
    FOpenPocketBaseRecordOptions Options) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) || !IsSafePathSegment(RecordId) ||
        !Body.Data.JsonObject.IsValid())
    {
        DispatchFailure<FOpenPocketBaseRecord>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Client, collection, record ID, and a JSON record body are required.")));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options.RequestOptions, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseRecord>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseRecord>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseRecord>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const FString Path = AddQuery(
        FString::Printf(
            TEXT("/api/collections/%s/records/%s"),
            *EncodeSegment(Collection),
            *EncodeSegment(RecordId)),
        MakeRecordQuery(Options));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("PATCH"),
        Path,
        OpenPocketBase::Json::SerializeObject(Body.Data.JsonObject.ToSharedRef()),
        Options.RequestOptions,
        true);
    const TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> WeakClient = PinnedClient;

    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options.RequestOptions,
        false,
        [Completion, WeakClient, AuthCollection = Collection](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<FOpenPocketBaseRecord> Result =
                OpenPocketBase::Json::ParseRecordResponse(Response);
            if (Result.IsSuccess())
            {
                if (const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client = WeakClient.Pin())
                {
                    bool bUpdated = false;
                    FOpenPocketBaseError StoreError;
                    if (!Client->Impl->TryStoreCurrentAuthRecord(
                            AuthCollection,
                            Result.GetValue(),
                            bUpdated,
                            StoreError))
                    {
                        Result = TOpenPocketBaseResult<FOpenPocketBaseRecord>::Failure(
                            MoveTemp(StoreError));
                    }
                }
            }
            const EOpenPocketBaseRequestState Terminal = TerminalStateFor(Result.IsSuccess());
            State->TryComplete(
                Terminal,
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion]()
        {
            Completion->Invoke(TOpenPocketBaseResult<FOpenPocketBaseRecord>::Failure(MakeCancelledError()));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::UpdateWithFiles(
    FString RecordId,
    FOpenPocketBaseRecordBody Body,
    TArray<FOpenPocketBaseFileInput> Files,
    FOpenPocketBaseRecordCallback OnComplete,
    FOpenPocketBaseRecordOptions Options,
    FOpenPocketBaseUploadLimits Limits,
    FOpenPocketBaseTransferProgressCallback OnProgress) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) || !IsSafePathSegment(RecordId) ||
        !Body.Data.JsonObject.IsValid() || Files.IsEmpty())
    {
        DispatchFailure<FOpenPocketBaseRecord>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Client, collection, record ID, JSON record body, and at least one file are required.")));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options.RequestOptions, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseRecord>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    OpenPocketBase::Multipart::FBuildResult Multipart;
    FOpenPocketBaseError MultipartError;
    const FString Boundary = TEXT("openpocketbase-") +
        FGuid::NewGuid().ToString(EGuidFormats::DigitsLower);
    if (!OpenPocketBase::Multipart::Build(
            Body,
            Files,
            Limits,
            Boundary,
            Multipart,
            MultipartError))
    {
        DispatchFailure<FOpenPocketBaseRecord>(MoveTemp(OnComplete), MoveTemp(MultipartError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseRecord>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseRecord>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const TSharedPtr<FOpenPocketBaseTransferProgressState, ESPMode::ThreadSafe> Progress =
        FOpenPocketBaseTransferProgressState::Create(
            PinnedClient->Impl->Clock,
            MoveTemp(OnProgress));
    const FString Path = AddQuery(
        FString::Printf(
            TEXT("/api/collections/%s/records/%s"),
            *EncodeSegment(Collection),
            *EncodeSegment(RecordId)),
        MakeRecordQuery(Options));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("PATCH"),
        Path,
        {},
        Options.RequestOptions,
        true);
    Request.BodyStream = MoveTemp(Multipart.Stream);
    Request.BodyLength = Multipart.ContentLength;
    if (Progress.IsValid())
    {
        Request.BodyStream = CreateOpenPocketBaseProgressArchive(
            MoveTemp(Request.BodyStream),
            Progress.ToSharedRef());
    }
    Request.Headers.Add(TEXT("Content-Type"), MoveTemp(Multipart.ContentType));
    Request.Headers.Add(TEXT("Content-Length"), LexToString(Request.BodyLength));
    const int64 TotalBytes = Request.BodyLength;
    const TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> WeakClient = PinnedClient;

    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options.RequestOptions,
        false,
        [Completion, Progress, TotalBytes, WeakClient, AuthCollection = Collection](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<FOpenPocketBaseRecord> Result =
                OpenPocketBase::Json::ParseRecordResponse(Response);
            if (Result.IsSuccess())
            {
                if (const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client = WeakClient.Pin())
                {
                    bool bUpdated = false;
                    FOpenPocketBaseError StoreError;
                    if (!Client->Impl->TryStoreCurrentAuthRecord(
                            AuthCollection,
                            Result.GetValue(),
                            bUpdated,
                            StoreError))
                    {
                        Result = TOpenPocketBaseResult<FOpenPocketBaseRecord>::Failure(
                            MoveTemp(StoreError));
                    }
                }
            }
            if (Progress.IsValid())
            {
                if (Result.IsSuccess())
                {
                    Progress->Finish(TotalBytes, TotalBytes);
                }
                else
                {
                    Progress->Stop();
                }
            }
            const EOpenPocketBaseRequestState Terminal = TerminalStateFor(Result.IsSuccess());
            State->TryComplete(
                Terminal,
                [Completion, Progress, Result = MoveTemp(Result)]() mutable
                {
                    if (Progress.IsValid())
                    {
                        Progress->Stop();
                    }
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion, Progress]()
        {
            if (Progress.IsValid())
            {
                Progress->Stop();
            }
            Completion->Invoke(TOpenPocketBaseResult<FOpenPocketBaseRecord>::Failure(MakeCancelledError()));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::Delete(
    FString RecordId,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) || !IsSafePathSegment(RecordId))
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Client, collection, and record ID are required.")));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options, OptionsError))
    {
        DispatchFailure<bool>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<TCompletionState<bool>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<bool>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const FString Path = FString::Printf(
        TEXT("/api/collections/%s/records/%s"),
        *EncodeSegment(Collection),
        *EncodeSegment(RecordId));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("DELETE"), Path, {}, Options, true);
    const TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> WeakClient = PinnedClient;

    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options,
        false,
        [Completion, WeakClient, AuthCollection = Collection, DeletedRecordId = RecordId](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<bool> Result = OpenPocketBase::Json::ParseEmptyResponse(Response);
            if (Result.IsSuccess())
            {
                if (const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client = WeakClient.Pin())
                {
                    Client->Impl->TryClearCurrentAuthRecord(AuthCollection, DeletedRecordId);
                }
            }
            const EOpenPocketBaseRequestState Terminal = TerminalStateFor(Result.IsSuccess());
            State->TryComplete(
                Terminal,
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion]()
        {
            Completion->Invoke(TOpenPocketBaseResult<bool>::Failure(MakeCancelledError()));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::ListAuthMethods(
    FOpenPocketBaseAuthMethodsCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection))
    {
        DispatchFailure<FOpenPocketBaseAuthMethods>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A ready client and valid auth collection are required.")));
        return {};
    }
    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseAuthMethods>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseAuthMethods>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseAuthMethods>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const FString Path = FString::Printf(
        TEXT("/api/collections/%s/auth-methods?fields=mfa%%2Cotp%%2Cpassword%%2Coauth2"),
        *EncodeSegment(Collection));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("GET"), Path, {}, Options, false);
    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options,
        true,
        [Completion](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<FOpenPocketBaseAuthMethods> Result =
                OpenPocketBase::Json::ParseAuthMethodsResponse(Response);
            State->TryComplete(
                TerminalStateFor(Result.IsSuccess()),
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion]()
        {
            Completion->Invoke(TOpenPocketBaseResult<FOpenPocketBaseAuthMethods>::Failure(
                MakeCancelledError()));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::RequestOtp(
    FString Email,
    FOpenPocketBaseOtpRequestCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) ||
        Email.IsEmpty() || Email.Len() > 320)
    {
        DispatchFailure<FOpenPocketBaseOtpRequest>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A ready client, valid auth collection, and bounded email are required.")));
        return {};
    }
    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseOtpRequest>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseOtpRequest>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseOtpRequest>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("email"), Email);
    const FString Path = FString::Printf(
        TEXT("/api/collections/%s/request-otp"), *EncodeSegment(Collection));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("POST"), Path, OpenPocketBase::Json::SerializeObject(Body), Options, false);
    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options,
        false,
        [Completion](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<FOpenPocketBaseOtpRequest> Result =
                OpenPocketBase::Json::ParseOtpResponse(Response);
            State->TryComplete(
                TerminalStateFor(Result.IsSuccess()),
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion]()
        {
            Completion->Invoke(TOpenPocketBaseResult<FOpenPocketBaseOtpRequest>::Failure(
                MakeCancelledError()));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::AuthWithPassword(
    FString Identity,
    FString Password,
    FOpenPocketBaseAuthAttemptCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) ||
        Identity.IsEmpty() || Identity.Len() > 320 || Password.IsEmpty() || Password.Len() > 4096)
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Client, collection, bounded identity, and password are required.")));
        return {};
    }
    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseAuthAttempt>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseAuthAttempt>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("identity"), Identity);
    Body->SetStringField(TEXT("password"), Password);
    const FString Path = FString::Printf(
        TEXT("/api/collections/%s/auth-with-password"), *EncodeSegment(Collection));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("POST"), Path, OpenPocketBase::Json::SerializeObject(Body), Options, false);
    const TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> WeakClient = PinnedClient;
    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options,
        false,
        [Completion, WeakClient, AuthCollection = Collection](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            FString Token;
            TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt> Result =
                OpenPocketBase::Json::ParseAuthAttemptResponse(Response, Token);
            if (Result.IsSuccess() &&
                Result.GetValue().Status == EOpenPocketBaseAuthAttemptStatus::Authenticated)
            {
                if (const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> ClientToUpdate =
                        WeakClient.Pin())
                {
                    if (!ClientToUpdate->IsShutdown())
                    {
                        FOpenPocketBaseError StoreError;
                        if (!ClientToUpdate->Impl->StoreAuth(
                            MoveTemp(Token),
                            Result.GetValue().Authentication.Record,
                            AuthCollection,
                            EOpenPocketBaseSessionChangeReason::LoggedIn,
                            StoreError))
                        {
                            Result = TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>::Failure(
                                MoveTemp(StoreError));
                        }
                    }
                }
            }

            const EOpenPocketBaseRequestState Terminal = TerminalStateFor(Result.IsSuccess());
            State->TryComplete(
                Terminal,
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion]()
        {
            Completion->Invoke(TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>::Failure(
                MakeCancelledError()));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::AuthWithOtp(
    FString OtpId,
    FString Password,
    FOpenPocketBaseMfaContinuation Mfa,
    FOpenPocketBaseAuthAttemptCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) ||
        !IsBoundedTransientAuthId(OtpId) || Password.IsEmpty() || Password.Len() > 4096 ||
        (Mfa.IsSet() && !IsBoundedTransientAuthId(Mfa.Id)))
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Client, collection, bounded OTP ID, password, and MFA continuation are required.")));
        return {};
    }
    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseAuthAttempt>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseAuthAttempt>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("otpId"), OtpId);
    Body->SetStringField(TEXT("password"), Password);
    if (Mfa.IsSet())
    {
        Body->SetStringField(TEXT("mfaId"), Mfa.Id);
    }
    const FString Path = FString::Printf(
        TEXT("/api/collections/%s/auth-with-otp"), *EncodeSegment(Collection));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("POST"), Path, OpenPocketBase::Json::SerializeObject(Body), Options, false);
    const TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> WeakClient = PinnedClient;
    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options,
        false,
        [Completion, WeakClient, AuthCollection = Collection](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            FString Token;
            TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt> Result =
                OpenPocketBase::Json::ParseAuthAttemptResponse(Response, Token);
            if (Result.IsSuccess() &&
                Result.GetValue().Status == EOpenPocketBaseAuthAttemptStatus::Authenticated)
            {
                if (const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> ClientToUpdate =
                        WeakClient.Pin())
                {
                    if (!ClientToUpdate->IsShutdown())
                    {
                        FOpenPocketBaseError StoreError;
                        if (!ClientToUpdate->Impl->StoreAuth(
                                MoveTemp(Token),
                                Result.GetValue().Authentication.Record,
                                AuthCollection,
                                EOpenPocketBaseSessionChangeReason::LoggedIn,
                                StoreError))
                        {
                            Result = TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>::Failure(
                                MoveTemp(StoreError));
                        }
                    }
                }
            }
            State->TryComplete(
                TerminalStateFor(Result.IsSuccess()),
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion]()
        {
            Completion->Invoke(TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>::Failure(
                MakeCancelledError()));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::BeginOAuth2(
    FOpenPocketBaseOAuth2StartOptions Options,
    FOpenPocketBaseOAuth2AuthorizationCallback OnComplete) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown() ||
        !IsSafePathSegment(Collection) || !IsSafeOAuthValue(Options.Provider, 128) ||
        !IsSafeOAuthValue(Options.RedirectUrl, 8192))
    {
        DispatchFailure<FOpenPocketBaseOAuth2Authorization>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A live client, valid auth collection, provider, and redirect URL are required.")));
        return {};
    }
    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options.RequestOptions, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseOAuth2Authorization>(
            MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseOAuth2Authorization>, ESPMode::ThreadSafe>
        Completion = MakeShared<
            TCompletionState<FOpenPocketBaseOAuth2Authorization>,
            ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> WeakClient = PinnedClient;
    const FOpenPocketBaseRequestOptions RequestOptions = Options.RequestOptions;
    return ListAuthMethods(
        [Completion, WeakClient, AuthCollection = Collection, Options = MoveTemp(Options)](
            TOpenPocketBaseResult<FOpenPocketBaseAuthMethods>&& Methods) mutable
        {
            if (!Methods.IsSuccess())
            {
                Completion->Invoke(
                    TOpenPocketBaseResult<FOpenPocketBaseOAuth2Authorization>::Failure(
                        Methods.GetError()));
                return;
            }

            const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> ClientToUpdate =
                WeakClient.Pin();
            if (!ClientToUpdate.IsValid() || ClientToUpdate->IsShutdown())
            {
                Completion->Invoke(
                    TOpenPocketBaseResult<FOpenPocketBaseOAuth2Authorization>::Failure(
                        MakeCancelledError()));
                return;
            }
            if (!Methods.GetValue().OAuth2.bEnabled)
            {
                Completion->Invoke(
                    TOpenPocketBaseResult<FOpenPocketBaseOAuth2Authorization>::Failure(
                        MakeLocalError(
                            EOpenPocketBaseErrorKind::Authentication,
                            TEXT("OAuth2 is not enabled for this auth collection."))));
                return;
            }

            const FOpenPocketBaseOAuthProvider* Provider =
                Methods.GetValue().OAuth2.Providers.FindByPredicate(
                    [&Options](const FOpenPocketBaseOAuthProvider& Candidate)
                    {
                        return Candidate.Name == Options.Provider;
                    });
            if (Provider == nullptr)
            {
                Completion->Invoke(
                    TOpenPocketBaseResult<FOpenPocketBaseOAuth2Authorization>::Failure(
                        MakeLocalError(
                            EOpenPocketBaseErrorKind::Authentication,
                            TEXT("The requested OAuth provider is not available."))));
                return;
            }

            FOpenPocketBaseOAuth2Authorization Authorization;
            FOpenPocketBaseError Error;
            if (!ClientToUpdate->Impl->TryCreateOAuthTransaction(
                    AuthCollection,
                    *Provider,
                    Options,
                    Authorization,
                    Error))
            {
                Completion->Invoke(
                    TOpenPocketBaseResult<FOpenPocketBaseOAuth2Authorization>::Failure(
                        MoveTemp(Error)));
                return;
            }
            Completion->Invoke(
                TOpenPocketBaseResult<FOpenPocketBaseOAuth2Authorization>::Success(
                    MoveTemp(Authorization)));
        },
        RequestOptions);
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::CompleteOAuth2(
    FOpenPocketBaseOAuth2Callback Callback,
    FOpenPocketBaseAuthAttemptCallback OnComplete) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(MoveTemp(OnComplete), MakeCancelledError());
        return {};
    }
    if (!IsSafePathSegment(Collection) ||
        !IsBoundedTransientAuthId(Callback.TransactionId) ||
        Callback.CallbackUrl.IsEmpty() || Callback.CallbackUrl.Len() > 8192 ||
        (Callback.Mfa.IsSet() && !IsBoundedTransientAuthId(Callback.Mfa.Id)))
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A valid auth collection, OAuth transaction, callback URL, and MFA continuation are required.")));
        return {};
    }
    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Callback.RequestOptions, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const bool bHasCreateData = Callback.CreateData.Data.JsonObject.IsValid() &&
        !Callback.CreateData.Data.JsonObject->Values.IsEmpty();
    if (bHasCreateData &&
        OpenPocketBase::Json::SerializeObject(
            Callback.CreateData.Data.JsonObject.ToSharedRef()).Num() > 64 * 1024)
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("OAuth create data exceeds the supported byte limit.")));
        return {};
    }

    FOpenPocketBaseClient::FImpl::FOAuthTransaction Transaction;
    FString Code;
    FOpenPocketBaseError ConsumeError;
    if (!PinnedClient->Impl->TryConsumeOAuthTransaction(
            Collection,
            Callback,
            Transaction,
            Code,
            ConsumeError))
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete), MoveTemp(ConsumeError));
        return {};
    }

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("provider"), Transaction.Provider);
    Body->SetStringField(TEXT("code"), MoveTemp(Code));
    Body->SetStringField(TEXT("codeVerifier"), MoveTemp(Transaction.CodeVerifier));
    Body->SetStringField(TEXT("redirectURL"), Transaction.RedirectUrl);
    if (bHasCreateData)
    {
        Body->SetObjectField(TEXT("createData"), Callback.CreateData.Data.JsonObject);
    }
    if (Callback.Mfa.IsSet())
    {
        Body->SetStringField(TEXT("mfaId"), Callback.Mfa.Id);
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseAuthAttempt>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseAuthAttempt>, ESPMode::ThreadSafe>(
            MoveTemp(OnComplete));
    const FString Path = FString::Printf(
        TEXT("/api/collections/%s/auth-with-oauth2"), *EncodeSegment(Collection));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("POST"),
        Path,
        OpenPocketBase::Json::SerializeObject(Body),
        Callback.RequestOptions,
        false);
    const TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> WeakClient = PinnedClient;
    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Callback.RequestOptions,
        false,
        [Completion, WeakClient, AuthCollection = Collection](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            FString Token;
            TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt> Result =
                OpenPocketBase::Json::ParseAuthAttemptResponse(Response, Token);
            if (!Result.IsSuccess())
            {
                Result = TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>::Failure(
                    SanitizeOAuthExchangeError(Result.GetError()));
            }
            else if (Result.GetValue().Status == EOpenPocketBaseAuthAttemptStatus::Authenticated)
            {
                if (const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> ClientToUpdate =
                        WeakClient.Pin())
                {
                    if (!ClientToUpdate->IsShutdown())
                    {
                        FOpenPocketBaseError StoreError;
                        if (!ClientToUpdate->Impl->StoreAuth(
                                MoveTemp(Token),
                                Result.GetValue().Authentication.Record,
                                AuthCollection,
                                EOpenPocketBaseSessionChangeReason::LoggedIn,
                                StoreError))
                        {
                            Result = TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>::Failure(
                                MoveTemp(StoreError));
                        }
                    }
                }
            }
            State->TryComplete(
                TerminalStateFor(Result.IsSuccess()),
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion]()
        {
            Completion->Invoke(
                TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>::Failure(
                    MakeCancelledError()));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::AuthWithOAuth2(
    FOpenPocketBaseAssistedOAuth2Options Options,
    FOpenPocketBaseAuthAttemptCallback OnComplete) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown() ||
        !IsSafePathSegment(Collection) || !IsSafeOAuthValue(Options.Provider, 128) ||
        (Options.Mfa.IsSet() && !IsBoundedTransientAuthId(Options.Mfa.Id)))
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A live client, auth collection, provider, and bounded MFA continuation are required.")));
        return {};
    }
    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options.RequestOptions, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }
    if (Options.Scopes.Num() > 32)
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("OAuth scopes exceed the supported bound.")));
        return {};
    }
    for (const FString& Scope : Options.Scopes)
    {
        if (!IsSafeOAuthValue(Scope, 256) || Scope.Contains(TEXT(" ")))
        {
            DispatchFailure<FOpenPocketBaseAuthAttempt>(
                MoveTemp(OnComplete),
                MakeLocalError(
                    EOpenPocketBaseErrorKind::InvalidArgument,
                    TEXT("OAuth scopes contain an invalid value.")));
            return {};
        }
    }
    if (Options.CreateData.Data.JsonObject.IsValid() &&
        !Options.CreateData.Data.JsonObject->Values.IsEmpty() &&
        OpenPocketBase::Json::SerializeObject(
            Options.CreateData.Data.JsonObject.ToSharedRef()).Num() > 64 * 1024)
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("OAuth create data exceeds the supported byte limit.")));
        return {};
    }

    const FOpenPocketBaseCapabilityInfo Capability =
        PinnedClient->GetCapability(EOpenPocketBaseCapability::OAuthCallback);
    if (!Capability.IsSupported())
    {
        FOpenPocketBaseError Error = MakeLocalError(
            EOpenPocketBaseErrorKind::Unsupported,
            TEXT("Assisted OAuth is unavailable."));
        if (!Capability.Reason.IsEmpty())
        {
            Error.ServerMessage += TEXT(" ") + Capability.Reason;
        }
        DispatchFailure<FOpenPocketBaseAuthAttempt>(MoveTemp(OnComplete), MoveTemp(Error));
        return {};
    }

    return OpenPocketBase::Internal::FAssistedOAuthOperation::Start(
        PinnedClient.ToSharedRef(),
        Collection,
        MoveTemp(Options),
        MoveTemp(OnComplete));
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::SendAccountPost(
    FString Route,
    TMap<FString, FString> BodyFields,
    const bool bUseAuth,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options,
    TUniqueFunction<bool(FOpenPocketBaseError&)> OnSucceeded) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection))
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A ready client and valid auth collection are required.")));
        return {};
    }
    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options, OptionsError))
    {
        DispatchFailure<bool>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    for (TPair<FString, FString>& Field : BodyFields)
    {
        Body->SetStringField(MoveTemp(Field.Key), MoveTemp(Field.Value));
    }
    const TSharedRef<TCompletionState<bool>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<bool>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const FString Path = FString::Printf(
        TEXT("/api/collections/%s/%s"),
        *EncodeSegment(Collection),
        *Route);
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("POST"),
        Path,
        OpenPocketBase::Json::SerializeObject(Body),
        Options,
        bUseAuth);
    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options,
        false,
        [Completion, OnSucceeded = MoveTemp(OnSucceeded)](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State) mutable
        {
            TOpenPocketBaseResult<bool> Result =
                OpenPocketBase::Json::ParseEmptyResponse(Response);
            if (Result.IsSuccess() && OnSucceeded)
            {
                FOpenPocketBaseError Error;
                if (!OnSucceeded(Error))
                {
                    Result = TOpenPocketBaseResult<bool>::Failure(MoveTemp(Error));
                }
            }
            State->TryComplete(
                TerminalStateFor(Result.IsSuccess()),
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        },
        [Completion]()
        {
            Completion->Invoke(TOpenPocketBaseResult<bool>::Failure(MakeCancelledError()));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::RequestPasswordReset(
    FString Email,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    if (!IsSafeOAuthValue(Email, 320))
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A bounded email is required.")));
        return {};
    }
    TMap<FString, FString> Body;
    Body.Add(TEXT("email"), MoveTemp(Email));
    return SendAccountPost(
        TEXT("request-password-reset"),
        MoveTemp(Body),
        false,
        MoveTemp(OnComplete),
        MoveTemp(Options));
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::ConfirmPasswordReset(
    FString Token,
    FString Password,
    FString PasswordConfirm,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    if (!IsSafeOAuthValue(Token, 8192) || !IsSafeOAuthValue(Password, 4096) ||
        Password != PasswordConfirm)
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A bounded token and matching passwords are required.")));
        return {};
    }
    TMap<FString, FString> Body;
    Body.Add(TEXT("token"), MoveTemp(Token));
    Body.Add(TEXT("password"), MoveTemp(Password));
    Body.Add(TEXT("passwordConfirm"), MoveTemp(PasswordConfirm));
    return SendAccountPost(
        TEXT("confirm-password-reset"),
        MoveTemp(Body),
        false,
        MoveTemp(OnComplete),
        MoveTemp(Options));
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::RequestVerification(
    FString Email,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    if (!IsSafeOAuthValue(Email, 320))
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A bounded email is required.")));
        return {};
    }
    TMap<FString, FString> Body;
    Body.Add(TEXT("email"), MoveTemp(Email));
    return SendAccountPost(
        TEXT("request-verification"),
        MoveTemp(Body),
        false,
        MoveTemp(OnComplete),
        MoveTemp(Options));
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::ConfirmVerification(
    FString Token,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    if (!IsSafeOAuthValue(Token, 8192))
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A bounded verification token is required.")));
        return {};
    }
    FString RecordId;
    FString CollectionId;
    TryDecodeJwtIdentity(Token, RecordId, CollectionId);
    const TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> WeakClient = Client;
    TMap<FString, FString> Body;
    Body.Add(TEXT("token"), MoveTemp(Token));
    return SendAccountPost(
        TEXT("confirm-verification"),
        MoveTemp(Body),
        false,
        MoveTemp(OnComplete),
        MoveTemp(Options),
        [WeakClient, RecordId = MoveTemp(RecordId), CollectionId = MoveTemp(CollectionId)](
            FOpenPocketBaseError& OutError)
        {
            const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient =
                WeakClient.Pin();
            if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
            {
                OutError = MakeCancelledError();
                return false;
            }
            if (RecordId.IsEmpty() || CollectionId.IsEmpty())
            {
                OutError = FOpenPocketBaseError();
                return true;
            }
            return PinnedClient->Impl->TryMarkCurrentAuthRecordVerified(
                RecordId,
                CollectionId,
                OutError);
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::RequestEmailChange(
    FString NewEmail,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    if (!IsSafeOAuthValue(NewEmail, 320))
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A bounded new email is required.")));
        return {};
    }
    TMap<FString, FString> Body;
    Body.Add(TEXT("newEmail"), MoveTemp(NewEmail));
    return SendAccountPost(
        TEXT("request-email-change"),
        MoveTemp(Body),
        true,
        MoveTemp(OnComplete),
        MoveTemp(Options));
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::ConfirmEmailChange(
    FString Token,
    FString Password,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    if (!IsSafeOAuthValue(Token, 8192) || !IsSafeOAuthValue(Password, 4096))
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A bounded email-change token and password are required.")));
        return {};
    }
    FString RecordId;
    FString CollectionId;
    TryDecodeJwtIdentity(Token, RecordId, CollectionId);
    const TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> WeakClient = Client;
    TMap<FString, FString> Body;
    Body.Add(TEXT("token"), MoveTemp(Token));
    Body.Add(TEXT("password"), MoveTemp(Password));
    return SendAccountPost(
        TEXT("confirm-email-change"),
        MoveTemp(Body),
        false,
        MoveTemp(OnComplete),
        MoveTemp(Options),
        [WeakClient, RecordId = MoveTemp(RecordId), CollectionId = MoveTemp(CollectionId)](
            FOpenPocketBaseError& OutError)
        {
            const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient =
                WeakClient.Pin();
            if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
            {
                OutError = MakeCancelledError();
                return false;
            }
            if (!RecordId.IsEmpty() && !CollectionId.IsEmpty())
            {
                PinnedClient->Impl->TryClearCurrentAuthIdentity(RecordId, CollectionId);
            }
            OutError = FOpenPocketBaseError();
            return true;
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::ListExternalAuths(
    FString RecordId,
    FOpenPocketBaseExternalAuthsCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) ||
        !IsSafePathSegment(RecordId))
    {
        DispatchFailure<TArray<FOpenPocketBaseExternalAuth>>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A ready client, auth collection, and record ID are required.")));
        return {};
    }
    FOpenPocketBaseFilterParams Params;
    Params.AddString(TEXT("id"), RecordId);
    FOpenPocketBaseFilter Filter;
    FOpenPocketBaseError FilterError;
    if (!FOpenPocketBaseFilter::TryBind(
            TEXT("recordRef = {:id}"), Params, Filter, FilterError))
    {
        DispatchFailure<TArray<FOpenPocketBaseExternalAuth>>(
            MoveTemp(OnComplete), MoveTemp(FilterError));
        return {};
    }

    FOpenPocketBaseListOptions ListOptions;
    ListOptions.PerPage = 100;
    ListOptions.Filter = MoveTemp(Filter);
    ListOptions.bSkipTotal = true;
    ListOptions.RequestOptions = MoveTemp(Options);
    return PinnedClient->Collection(TEXT("_externalAuths")).GetList(
        MoveTemp(ListOptions),
        [OnComplete = MoveTemp(OnComplete)](
            TOpenPocketBaseResult<FOpenPocketBaseRecordPage>&& Result) mutable
        {
            if (!OnComplete)
            {
                return;
            }
            if (!Result.IsSuccess())
            {
                OnComplete(TOpenPocketBaseResult<TArray<FOpenPocketBaseExternalAuth>>::Failure(
                    Result.GetError()));
                return;
            }

            TArray<FOpenPocketBaseExternalAuth> ExternalAuths;
            ExternalAuths.Reserve(Result.GetValue().Items.Num());
            for (FOpenPocketBaseRecord& Record : Result.GetValue().Items)
            {
                FOpenPocketBaseExternalAuth ExternalAuth;
                if (!TryMakeExternalAuth(MoveTemp(Record), ExternalAuth))
                {
                    OnComplete(TOpenPocketBaseResult<TArray<FOpenPocketBaseExternalAuth>>::Failure(
                        MakeLocalError(
                            EOpenPocketBaseErrorKind::Serialization,
                            TEXT("PocketBase returned an invalid linked external-auth record."))));
                    return;
                }
                ExternalAuths.Add(MoveTemp(ExternalAuth));
            }
            OnComplete(TOpenPocketBaseResult<TArray<FOpenPocketBaseExternalAuth>>::Success(
                MoveTemp(ExternalAuths)));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::UnlinkExternalAuth(
    FString RecordId,
    FString Provider,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) ||
        !IsSafePathSegment(RecordId) || !IsSafeOAuthValue(Provider, 128))
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A ready client, auth collection, record ID, and provider are required.")));
        return {};
    }
    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options, OptionsError))
    {
        DispatchFailure<bool>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }
    FOpenPocketBaseFilterParams Params;
    Params.AddString(TEXT("recordId"), RecordId);
    Params.AddString(TEXT("provider"), Provider);
    FOpenPocketBaseFilter Filter;
    FOpenPocketBaseError FilterError;
    if (!FOpenPocketBaseFilter::TryBind(
            TEXT("recordRef = {:recordId} && provider = {:provider}"),
            Params,
            Filter,
            FilterError))
    {
        DispatchFailure<bool>(MoveTemp(OnComplete), MoveTemp(FilterError));
        return {};
    }

    const TSharedRef<TCompletionState<bool>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<bool>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const TSharedRef<FChainedAccountRequest, ESPMode::ThreadSafe> Chain =
        MakeShared<FChainedAccountRequest, ESPMode::ThreadSafe>();
    const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> RequestState =
        PinnedClient->Impl->CreateCompositeState(
            [Chain, Completion]()
            {
                Chain->Cancel();
                Completion->Invoke(TOpenPocketBaseResult<bool>::Failure(MakeCancelledError()));
            });
    const FOpenPocketBaseRequestHandle Handle =
        PinnedClient->Impl->MakeRequestHandle(RequestState);

    FOpenPocketBaseRecordOptions FindOptions;
    FindOptions.RequestOptions = Options;
    FOpenPocketBaseRequestHandle FindHandle =
        PinnedClient->Collection(TEXT("_externalAuths")).GetFirstListItem(
            MoveTemp(Filter),
            [PinnedClient, Chain, Completion, RequestState, Options](
                TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result) mutable
            {
                if (!RequestState->IsActive())
                {
                    return;
                }
                if (!Result.IsSuccess())
                {
                    const EOpenPocketBaseRequestState Terminal =
                        Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled
                            ? EOpenPocketBaseRequestState::Cancelled
                            : EOpenPocketBaseRequestState::Failed;
                    RequestState->TryComplete(
                        Terminal,
                        [Completion, Error = Result.GetError()]() mutable
                        {
                            Completion->Invoke(TOpenPocketBaseResult<bool>::Failure(
                                MoveTemp(Error)));
                        });
                    return;
                }

                FOpenPocketBaseRequestHandle DeleteHandle =
                    PinnedClient->Collection(TEXT("_externalAuths")).Delete(
                        Result.GetValue().Id,
                        [Completion, RequestState](TOpenPocketBaseResult<bool>&& DeleteResult) mutable
                        {
                            if (!RequestState->IsActive())
                            {
                                return;
                            }
                            const EOpenPocketBaseRequestState Terminal =
                                !DeleteResult.IsSuccess() &&
                                    DeleteResult.GetError().Kind ==
                                        EOpenPocketBaseErrorKind::Cancelled
                                ? EOpenPocketBaseRequestState::Cancelled
                                : TerminalStateFor(DeleteResult.IsSuccess());
                            RequestState->TryComplete(
                                Terminal,
                                [Completion, DeleteResult = MoveTemp(DeleteResult)]() mutable
                                {
                                    Completion->Invoke(MoveTemp(DeleteResult));
                                });
                        },
                        Options);
                Chain->SetChild(MoveTemp(DeleteHandle));
            },
            MoveTemp(FindOptions));
    Chain->SetChild(MoveTemp(FindHandle));
    return Handle;
}

FOpenPocketBaseSubscriptionResult FOpenPocketBaseCollectionService::SubscribeToRecords(
    FOpenPocketBaseRealtimeCallbacks Callbacks,
    FOpenPocketBaseRealtimeOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection))
    {
        return FOpenPocketBaseSubscriptionResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("A ready client and valid collection are required.")));
    }
    return PinnedClient->Subscribe(
        Collection + TEXT("/*"),
        MoveTemp(Callbacks),
        MoveTemp(Options));
}

FOpenPocketBaseSubscriptionResult FOpenPocketBaseCollectionService::SubscribeToRecord(
    FString RecordId,
    FOpenPocketBaseRealtimeCallbacks Callbacks,
    FOpenPocketBaseRealtimeOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) ||
        !IsSafePathSegment(RecordId))
    {
        return FOpenPocketBaseSubscriptionResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("A ready client, valid collection, and valid record ID are required.")));
    }
    return PinnedClient->Subscribe(
        Collection + TEXT("/") + MoveTemp(RecordId),
        MoveTemp(Callbacks),
        MoveTemp(Options));
}
