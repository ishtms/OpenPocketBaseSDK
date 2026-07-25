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
#include "HAL/FileManager.h"
#include "Math/RandomStream.h"
#include "Misc/Base64.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "Misc/SecureHash.h"
#include "Query/OpenPocketBaseRecordQuery.h"
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
    const FString& Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = Kind;
    Error.Message = Message;
    return Error;
}

FOpenPocketBaseError MakeCancelledError()
{
    return MakeLocalError(
        EOpenPocketBaseErrorKind::Cancelled,
        TEXT("The PocketBase request was cancelled by the caller."));
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
    Error.Message = Error.Kind == EOpenPocketBaseErrorKind::Timeout
        ? TEXT("The protected file download timed out. Retry it or increase the request timeout.")
        : TEXT("The protected file download failed. Check the error kind, HTTP status, and request ID without logging the protected URL.");
    Error.Code.Reset();
    Error.FieldErrors.Reset();
    return Error;
}

FOpenPocketBaseError SanitizeOAuthExchangeError(FOpenPocketBaseError Error)
{
    Error.Message = Error.Kind == EOpenPocketBaseErrorKind::Timeout
        ? TEXT("The OAuth code exchange timed out. Restart the OAuth login and try again.")
        : TEXT("The OAuth code exchange failed. Restart the OAuth login and check the provider configuration.");
    Error.Code.Reset();
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

    int32 HeaderIndex = 0;
    for (const TPair<FString, FString>& Header : Config.DefaultHeaders)
    {
        ++HeaderIndex;
        if (!IsValidHeaderName(Header.Key))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                *FString::Printf(
                    TEXT("Default header %d has an invalid name. Use a non-empty HTTP token name without spaces or control characters."),
                    HeaderIndex));
            return false;
        }
        if (!IsValidHeaderValue(Header.Value))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                *FString::Printf(
                    TEXT("Default header '%s' contains a control character. Header values must stay on one line."),
                    *Header.Key));
            return false;
        }

        if (IsProtectedDefaultHeader(Header.Key))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                *FString::Printf(
                    TEXT("Default header '%s' is managed by the SDK and cannot be overridden."),
                    *Header.Key));
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
    if (Path.Len() < 2 || Path.Len() > 2048)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            *FString::Printf(
                TEXT("Custom route Path must contain between 2 and 2048 characters, but it contains %d."),
                Path.Len()));
        return false;
    }
    if (!Path.StartsWith(TEXT("/")))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Custom route Path must start with '/'. Pass only the route path, without the PocketBase server URL."));
        return false;
    }
    if (Path.StartsWith(TEXT("//")) || Path.EndsWith(TEXT("/")) ||
        Path.Contains(TEXT("//")))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Custom route Path must use single separators and must not end with '/'."));
        return false;
    }
    if (Path.Contains(TEXT("?")) || Path.Contains(TEXT("#")) ||
        Path.Contains(TEXT("\\")) || Path.Contains(TEXT("%")))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Custom route Path must not contain a query, fragment, backslash, or percent encoding. Add query values through the Query input."));
        return false;
    }
    if (Query.Num() > 64)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            *FString::Printf(
                TEXT("Custom route Query contains %d entries, but the maximum is 64."),
                Query.Num()));
        return false;
    }

    TArray<FString> Segments;
    Path.RightChop(1).ParseIntoArray(Segments, TEXT("/"), false);
    if (Segments.IsEmpty() || Segments.Num() > 32)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            *FString::Printf(
                TEXT("Custom route Path contains %d segments, but the maximum is 32."),
                Segments.Num()));
        return false;
    }

    TArray<FString> EncodedSegments;
    EncodedSegments.Reserve(Segments.Num());
    for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
    {
        const FString& Segment = Segments[SegmentIndex];
        if (!IsSafePathSegment(Segment) || Segment.Len() > 255)
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                *FString::Printf(
                    TEXT("Custom route Path segment %d is empty, unsafe, or longer than 255 characters."),
                    SegmentIndex + 1));
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
    for (int32 QueryIndex = 0; QueryIndex < QueryNames.Num(); ++QueryIndex)
    {
        const FString& Name = QueryNames[QueryIndex];
        const FString* Value = Query.Find(Name);
        if (Value == nullptr || Name.IsEmpty() || !IsBoundedPlainText(Name, 128) ||
            !IsBoundedPlainText(*Value, 4096))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                *FString::Printf(
                    TEXT("Custom route Query entry %d needs a non-empty name up to 128 characters and a value up to 4096 characters, without control characters."),
                    QueryIndex + 1));
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
            *FString::Printf(
                TEXT("The encoded custom route URL is %d characters, but the maximum is 8192. Reduce the path or query values."),
                OutPath.Len()));
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
    if (Fields.IsEmpty())
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Form Fields is empty. Add at least one field before sending a form body."));
        return false;
    }
    if (Fields.Num() > 128)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            *FString::Printf(
                TEXT("Form Fields contains %d entries, but the maximum is 128."),
                Fields.Num()));
        return false;
    }
    TArray<FString> Names;
    Fields.GetKeys(Names);
    Names.Sort();
    TArray<FString> Parts;
    Parts.Reserve(Names.Num());
    for (int32 FieldIndex = 0; FieldIndex < Names.Num(); ++FieldIndex)
    {
        const FString& Name = Names[FieldIndex];
        const FString* Value = Fields.Find(Name);
        if (Value == nullptr || Name.IsEmpty() || !IsBoundedPlainText(Name, 128) ||
            !IsBoundedPlainText(*Value, 4096))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                *FString::Printf(
                    TEXT("Form field %d needs a non-empty name up to 128 characters and a value up to 4096 characters, without control characters."),
                    FieldIndex + 1));
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
    if (Url.IsEmpty())
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("OAuth Redirect URL is empty. Provide the exact HTTP or HTTPS callback URL registered with the OAuth provider."));
        return false;
    }
    if (Url.Len() > 8192)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            FString::Printf(
                TEXT("OAuth Redirect URL is %d characters. Use at most 8192 characters."),
                Url.Len()));
        return false;
    }
    if (Url.Contains(TEXT("\\")) || Url.Contains(TEXT("#")) || Url.Contains(TEXT("?")))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("OAuth Redirect URL must not contain a backslash, query, or fragment. Provide only the registered callback origin and path."));
        return false;
    }
    if (!TryGetNormalizedOrigin(Url, OutOrigin))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("OAuth Redirect URL must contain a valid HTTP or HTTPS origin and an optional path."));
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
    if (ProviderUrl.IsEmpty())
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Serialization,
            TEXT("PocketBase returned an empty OAuth authorization URL. Check the provider configuration in PocketBase."));
        return false;
    }
    if (ProviderUrl.Len() > 8192 || ProviderUrl.Contains(TEXT("#")) ||
        ProviderUrl.Contains(TEXT("\\")))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Serialization,
            TEXT("PocketBase returned an OAuth authorization URL that exceeds 8192 characters or contains a fragment or backslash. Check the provider configuration in PocketBase."));
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
            TEXT("PocketBase returned duplicate, empty, or oversized OAuth authorization query parameters. Check the provider configuration in PocketBase."));
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
                FString::Printf(
                    TEXT("OAuth Scopes contains %d entries. Use at most 32 scopes."),
                    Scopes.Num()));
            return false;
        }
        for (int32 ScopeIndex = 0; ScopeIndex < Scopes.Num(); ++ScopeIndex)
        {
            const FString& Scope = Scopes[ScopeIndex];
            if (!IsSafeOAuthValue(Scope, 256) || Scope.Contains(TEXT(" ")))
            {
                OutError = MakeLocalError(
                    EOpenPocketBaseErrorKind::InvalidArgument,
                    FString::Printf(
                        TEXT("OAuth Scope %d is empty, exceeds 256 characters, contains a space, or contains a control character."),
                        ScopeIndex + 1));
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
            FString::Printf(
                TEXT("The OAuth authorization URL is %d characters after adding redirect, state, PKCE, and scopes. Reduce the configured scopes to stay within 8192 characters."),
                OutUrl.Len()));
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
    if (ProviderUrl.IsEmpty())
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Serialization,
            TEXT("PocketBase returned an empty OAuth authorization URL. Check the provider configuration in PocketBase."));
        return false;
    }
    if (ProviderUrl.Len() > 8192 || ProviderUrl.Contains(TEXT("#")) ||
        ProviderUrl.Contains(TEXT("\\")))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Serialization,
            TEXT("PocketBase returned an OAuth authorization URL that exceeds 8192 characters or contains a fragment or backslash. Check the provider configuration in PocketBase."));
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
            TEXT("PocketBase returned duplicate, empty, or oversized OAuth authorization query parameters. Check the provider configuration in PocketBase."));
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
                FString::Printf(
                    TEXT("OAuth Scopes contains %d entries. Use at most 32 scopes."),
                    Scopes.Num()));
            return false;
        }
        for (int32 ScopeIndex = 0; ScopeIndex < Scopes.Num(); ++ScopeIndex)
        {
            const FString& Scope = Scopes[ScopeIndex];
            if (!IsSafeOAuthValue(Scope, 256) || Scope.Contains(TEXT(" ")))
            {
                OutError = MakeLocalError(
                    EOpenPocketBaseErrorKind::InvalidArgument,
                    FString::Printf(
                        TEXT("OAuth Scope %d is empty, exceeds 256 characters, contains a space, or contains a control character."),
                        ScopeIndex + 1));
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
            FString::Printf(
                TEXT("The assisted OAuth authorization URL is %d characters after adding redirect, state, and scopes. Reduce the configured scopes to stay within 8192 characters."),
                OutUrl.Len()));
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
    if (CallbackUrl.IsEmpty())
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Authentication,
            TEXT("OAuth Callback URL is empty. Pass the full callback URL returned by the OAuth provider."));
        return false;
    }
    if (CallbackUrl.Len() > 8192 || CallbackUrl.Contains(TEXT("#")))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Authentication,
            TEXT("OAuth Callback URL exceeds 8192 characters or contains a fragment. Pass the provider callback URL without a fragment."));
        return false;
    }
    FString CallbackBase;
    FString Query;
    if (!CallbackUrl.Split(TEXT("?"), &CallbackBase, &Query))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Authentication,
            TEXT("OAuth Callback URL has no query parameters. Pass the full callback URL containing state and code or error."));
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
            TEXT("OAuth Callback URL contains duplicate, empty, or oversized query parameters. Restart the OAuth login and use the callback URL once."));
        return false;
    }
    if (Values.FindRef(TEXT("state")) != ExpectedState)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Authentication,
            TEXT("OAuth Callback state does not match the active transaction. Do not reuse a callback URL, and restart the OAuth login."));
        return false;
    }
    if (Values.Contains(TEXT("error")))
    {
        FString ProviderCode = Values.FindRef(TEXT("error"));
        if (!IsSafeOAuthValue(ProviderCode, 256))
        {
            ProviderCode = TEXT("oauth_provider_error");
        }
        FString ProviderDescription = Values.FindRef(TEXT("error_description"));
        if (!ProviderDescription.IsEmpty() &&
            !IsSafeOAuthValue(ProviderDescription, 2048))
        {
            ProviderDescription.Reset();
        }
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::Authentication,
            ProviderDescription.IsEmpty()
                ? FString::Printf(
                    TEXT("The OAuth provider returned '%s' instead of an authorization code. Restart the login and check the provider permissions and redirect URL."),
                    *ProviderCode)
                : FString::Printf(
                    TEXT("The OAuth provider returned '%s': %s"),
                    *ProviderCode,
                    *ProviderDescription));
        OutError.Code = MoveTemp(ProviderCode);
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

template <typename ValueType>
FString JoinQueryValues(const TArray<ValueType>& Values)
{
    TArray<FString> QueryValues;
    QueryValues.Reserve(Values.Num());
    for (const ValueType& Value : Values)
    {
        QueryValues.Add(Value.ToQueryValue());
    }
    return FString::Join(QueryValues, TEXT(","));
}

FString MakeListQuery(const FOpenPocketBaseListOptions& Options)
{
    TArray<FString> Parts;
    Parts.Add(FString::Printf(TEXT("page=%d"), Options.Page));
    Parts.Add(FString::Printf(TEXT("perPage=%d"), Options.PerPage));
    AddQueryValue(Parts, TEXT("filter"), Options.Filter.ToString());
    AddQueryValue(Parts, TEXT("sort"), JoinQueryValues(Options.Sort));
    AddQueryValue(Parts, TEXT("expand"), JoinQueryValues(Options.Expand));
    AddQueryValue(
        Parts,
        TEXT("fields"),
        OpenPocketBase::Internal::MakeRecordFieldsQuery(Options.Fields));
    if (Options.bSkipTotal)
    {
        Parts.Add(TEXT("skipTotal=true"));
    }
    return FString::Join(Parts, TEXT("&"));
}

FString MakeRecordQuery(const FOpenPocketBaseRecordOptions& Options)
{
    TArray<FString> Parts;
    AddQueryValue(Parts, TEXT("expand"), JoinQueryValues(Options.Expand));
    AddQueryValue(
        Parts,
        TEXT("fields"),
        OpenPocketBase::Internal::MakeRecordFieldsQuery(Options.Fields));
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
        *EncodeSegment(Entry.GetCollectionName()));
    if (Entry.Operation == EOpenPocketBaseBatchOperation::Update ||
        Entry.Operation == EOpenPocketBaseBatchOperation::Delete)
    {
        Path += TEXT("/") + EncodeSegment(Entry.RecordId);
    }

    TArray<FString> QueryParts;
    AddQueryValue(QueryParts, TEXT("expand"), Entry.GetExpandQuery());
    AddQueryValue(QueryParts, TEXT("fields"), Entry.GetFieldsQuery());
    return AddQuery(MoveTemp(Path), FString::Join(QueryParts, TEXT("&")));
}

bool ValidateBatch(
    const FOpenPocketBaseBatchRequest& Batch,
    const FOpenPocketBaseBatchOptions& Options,
    FOpenPocketBaseError& OutError)
{
    if (!Batch.IsValid())
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            *Batch.ErrorMessage);
        return false;
    }
    if (Batch.Entries.IsEmpty())
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Batch must contain at least one operation."));
        return false;
    }
    if (Options.MaxOperations < 1 || Options.MaxOperations > 50)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            FString::Printf(
                TEXT("Max Operations is %d. Use a value from 1 to 50."),
                Options.MaxOperations));
        return false;
    }
    if (Batch.Entries.Num() > Options.MaxOperations)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            *FString::Printf(
                TEXT("Batch contains %d operations, but Max Operations is %d."),
                Batch.Entries.Num(),
                Options.MaxOperations));
        return false;
    }
    if (Options.MaxBodyBytes < 1024 || Options.MaxBodyBytes > 16 * 1024 * 1024)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            FString::Printf(
                TEXT("Max Body Bytes is %d. Use a value from 1024 to 16777216 bytes."),
                Options.MaxBodyBytes));
        return false;
    }
    if (Options.RequestOptions.TotalTimeoutSeconds <= 0 ||
        Options.RequestOptions.TotalTimeoutSeconds > 120)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            FString::Printf(
                TEXT("Batch Total Timeout is %.3f seconds. Use a value greater than 0 and no more than 120 seconds."),
                Options.RequestOptions.TotalTimeoutSeconds));
        return false;
    }
    if (Options.RequestOptions.ActivityTimeoutSeconds <= 0 ||
        Options.RequestOptions.ActivityTimeoutSeconds > 120)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            FString::Printf(
                TEXT("Batch Activity Timeout is %.3f seconds. Use a value greater than 0 and no more than 120 seconds."),
                Options.RequestOptions.ActivityTimeoutSeconds));
        return false;
    }

    for (int32 EntryIndex = 0; EntryIndex < Batch.Entries.Num(); ++EntryIndex)
    {
        const FOpenPocketBaseBatchEntry& Entry = Batch.Entries[EntryIndex];
        const int32 OperationNumber = EntryIndex + 1;
        const bool bTypedCollection = Entry.Collection.IsSet();
        FOpenPocketBaseWritableCollectionRef CurrentCollection;
        if ((bTypedCollection &&
             (!Entry.Collection.ResolveCurrentAs(CurrentCollection) ||
              !IsSafePathSegment(CurrentCollection.Name))) ||
            (!bTypedCollection && !IsSafePathSegment(Entry.DynamicCollection)))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                *FString::Printf(
                    TEXT("Batch operation %d has no valid collection. Choose a current collection from the imported schema."),
                    OperationNumber));
            return false;
        }

        const bool bUsesRecordId = Entry.Operation == EOpenPocketBaseBatchOperation::Update ||
            Entry.Operation == EOpenPocketBaseBatchOperation::Upsert ||
            Entry.Operation == EOpenPocketBaseBatchOperation::Delete;
        if (bUsesRecordId && !IsSafePathSegment(Entry.RecordId))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                *FString::Printf(
                    TEXT("Batch operation %d requires a non-empty Record ID without path separators or control characters."),
                    OperationNumber));
            return false;
        }

        const bool bUsesBody = Entry.Operation != EOpenPocketBaseBatchOperation::Delete;
        if (bUsesBody &&
            (!Entry.Body.Data.JsonObject.IsValid() ||
             !Entry.Body.IsValid() ||
             (bTypedCollection && !Entry.Body.BelongsTo(Entry.Collection))))
        {
            FString BodyReason;
            if (!Entry.Body.IsValid() && !Entry.Body.ErrorMessage.IsEmpty())
            {
                BodyReason = Entry.Body.ErrorMessage;
            }
            else if (!Entry.Body.Data.JsonObject.IsValid())
            {
                BodyReason = TEXT("Start with New Record Body and add the fields for this operation.");
            }
            else
            {
                BodyReason = TEXT("The body was built for another collection. Rebuild it from this operation's Collection pin.");
            }
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                FString::Printf(
                    TEXT("Batch operation %d has an invalid Record Body. %s"),
                    OperationNumber,
                    *BodyReason));
            return false;
        }

        if (bTypedCollection &&
            (!Entry.ResponseOptions.IsValid() ||
             !Entry.ResponseOptions.BelongsTo(Entry.Collection)))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                *FString::Printf(
                    TEXT("Batch operation %d has response fields or expansions from another collection."),
                    OperationNumber));
            return false;
        }
        if (Entry.Operation == EOpenPocketBaseBatchOperation::Upsert)
        {
            if (Entry.RecordId.Len() != 15)
            {
                OutError = MakeLocalError(
                    EOpenPocketBaseErrorKind::InvalidArgument,
                    *FString::Printf(
                        TEXT("Batch upsert operation %d needs a 15-character Record ID, but the supplied ID contains %d characters."),
                        OperationNumber,
                        Entry.RecordId.Len()));
                return false;
            }
            FString BodyRecordId;
            if (!Entry.Body.Data.JsonObject->TryGetStringField(TEXT("id"), BodyRecordId) ||
                BodyRecordId != Entry.RecordId)
            {
                OutError = MakeLocalError(
                    EOpenPocketBaseErrorKind::InvalidArgument,
                    *FString::Printf(
                        TEXT("Batch upsert operation %d could not attach its Record ID to the request body. Rebuild the body with New Record Body and reconnect it to With Upsert."),
                        OperationNumber));
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
    if (Options.RequestKey.Len() > 128)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            *FString::Printf(
                TEXT("Request Key contains %d characters, but the maximum is 128."),
                Options.RequestKey.Len()));
        return false;
    }
    for (const TCHAR Character : Options.RequestKey)
    {
        if (FChar::IsControl(Character))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Request Key contains a control character. Use a short printable identifier."));
            return false;
        }
    }

    if (Options.AdditionalHeaders.Num() > 32)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            *FString::Printf(
                TEXT("Additional Headers contains %d entries, but the maximum is 32."),
                Options.AdditionalHeaders.Num()));
        return false;
    }
    int32 HeaderIndex = 0;
    for (const TPair<FString, FString>& Header : Options.AdditionalHeaders)
    {
        ++HeaderIndex;
        if (Header.Key.Len() > 128 || !IsValidHeaderName(Header.Key))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                *FString::Printf(
                    TEXT("Additional header %d has an invalid name. Use an HTTP token name up to 128 characters."),
                    HeaderIndex));
            return false;
        }
        if (Header.Value.Len() > 4096 || !IsValidHeaderValue(Header.Value))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                *FString::Printf(
                    TEXT("Additional header '%s' must be at most 4096 characters and must not contain control characters."),
                    *Header.Key));
            return false;
        }
        if (IsProtectedRequestHeader(Header.Key))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                *FString::Printf(
                    TEXT("Additional header '%s' is managed by the SDK and cannot be overridden."),
                    *Header.Key));
            return false;
        }
    }

    if (!IsValidTraceParent(Options.TraceParent))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Trace Parent is invalid. Leave it empty or provide a lowercase W3C traceparent value in the 00-<trace-id>-<parent-id>-<flags> format."));
        return false;
    }
    if (Options.TotalTimeoutSeconds < 0)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            *FString::Printf(
                TEXT("Total Timeout cannot be negative, but it is %.3f seconds."),
                Options.TotalTimeoutSeconds));
        return false;
    }
    if (Options.ActivityTimeoutSeconds < 0)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            *FString::Printf(
                TEXT("Activity Timeout cannot be negative, but it is %.3f seconds."),
                Options.ActivityTimeoutSeconds));
        return false;
    }
    if (Options.MaxReadRetries < 0 || Options.MaxReadRetries > 5)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            *FString::Printf(
                TEXT("Max Read Retries must be between 0 and 5, but it is %d."),
                Options.MaxReadRetries));
        return false;
    }
    if (Options.RetryBaseDelaySeconds < 0 || Options.RetryBaseDelaySeconds > 30)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            *FString::Printf(
                TEXT("Retry Base Delay must be between 0 and 30 seconds, but it is %.3f."),
                Options.RetryBaseDelaySeconds));
        return false;
    }
    if (Options.RetryMaxDelaySeconds < 0 || Options.RetryMaxDelaySeconds > 60)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            *FString::Printf(
                TEXT("Retry Max Delay must be between 0 and 60 seconds, but it is %.3f."),
                Options.RetryMaxDelaySeconds));
        return false;
    }
    if (Options.RetryJitterFraction < 0 || Options.RetryJitterFraction > 1)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            *FString::Printf(
                TEXT("Retry Jitter Fraction must be between 0 and 1, but it is %.3f."),
                Options.RetryJitterFraction));
        return false;
    }
    if (Options.MaxResponseBytes < 1024 ||
        Options.MaxResponseBytes > 64 * 1024 * 1024)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            *FString::Printf(
                TEXT("Max Response Bytes must be between 1024 and 67108864, but it is %lld."),
                static_cast<long long>(Options.MaxResponseBytes)));
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
            Response.ErrorMessage = TEXT("The HTTP transport became unavailable before the request could start.");
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
            Response.ErrorMessage = TEXT("The HTTP response redirected to another origin. Cross-origin redirects are blocked to protect authentication data.");
        }

        const bool bExceededResponseLimit = Response.Body.Num() > Options.MaxResponseBytes;
        if (bExceededResponseLimit)
        {
            const int64 ActualResponseBytes = Response.Body.Num();
            Response = FOpenPocketBaseHttpResponse();
            Response.RequestId = Request.RequestId;
            Response.ErrorMessage = FString::Printf(
                TEXT("The response is %lld bytes, which exceeds Max Response Bytes of %lld. Increase the limit only if this response size is expected."),
                ActualResponseBytes,
                Options.MaxResponseBytes);
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
    EOpenPocketBaseSessionChangeReason LastSessionReason =
        EOpenPocketBaseSessionChangeReason::LoggedOut;
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
        if (bShutdown.load(std::memory_order_acquire))
        {
            OutError = MakeCancelledError();
            return false;
        }
        if (!IsSafePathSegment(TransactionAuthCollection))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Auth Collection is empty or contains an unsafe path character. Choose an auth collection from the current schema."));
            return false;
        }
        if (!IsSafeOAuthValue(Provider.Name, 128))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::Serialization,
                TEXT("PocketBase returned an empty or invalid OAuth provider name. Refresh the auth methods and check the provider configuration."));
            return false;
        }
        if (Provider.Name != Options.Provider)
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("The selected OAuth provider does not match OAuth Start Options. Rebuild the options from the same provider returned by Get Auth Methods."));
            return false;
        }
        if (!IsSafeOAuthValue(Options.RedirectUrl, 8192))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("OAuth Redirect URL is empty, exceeds 8192 characters, or contains a control character."));
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
                TEXT("The OAuth PKCE challenge could not be generated from the provider's code verifier. Refresh the auth methods and restart OAuth instead of reusing this provider response."));
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
                    TEXT("The client already has 16 active OAuth transactions. Finish or cancel one, or wait up to five minutes for an unused transaction to expire."));
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
                    TEXT("The SDK could not allocate a unique OAuth transaction ID after four attempts. Retry the OAuth login."));
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
        if (!IsSafePathSegment(ExpectedAuthCollection))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Auth Collection is empty or contains an unsafe path character. Use the same auth collection that started the OAuth login."));
            return false;
        }
        if (!IsBoundedTransientAuthId(Callback.TransactionId))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("OAuth Transaction ID is empty, exceeds 256 characters, or contains whitespace. Use the callback from the active OAuth authorization."));
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
                TEXT("The OAuth transaction is not active. It may already be completed, cancelled, or unknown to this client. Restart the OAuth login."));
            return false;
        }
        if (Transaction->ExpiresAtMonotonicSeconds <= Clock->MonotonicSeconds())
        {
            OAuthTransactions.Remove(Callback.TransactionId);
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("The OAuth transaction expired after five minutes. Restart the OAuth login."));
            return false;
        }
        if (Transaction->AuthCollection != ExpectedAuthCollection)
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("The OAuth transaction belongs to another auth collection. Complete it through the same auth collection that started it."));
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
        if (bShutdown.load(std::memory_order_acquire))
        {
            OutError = MakeCancelledError();
            return false;
        }
        if (!Config.bEnableAssistedOAuth)
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::Unsupported,
                TEXT("Assisted OAuth is disabled for this client profile. Enable Assisted OAuth or use the manual OAuth flow."));
            return false;
        }
        if (!IsSafePathSegment(TransactionAuthCollection))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Auth Collection is empty or contains an unsafe path character. Choose an auth collection from the current schema."));
            return false;
        }
        if (!IsSafeOAuthValue(Provider.Name, 128))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::Serialization,
                TEXT("PocketBase returned an empty or invalid OAuth provider name. Refresh the auth methods and check the provider configuration."));
            return false;
        }
        if (!IsSafeOAuthValue(Provider.CodeVerifier, 4096))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::Serialization,
                TEXT("PocketBase returned an empty or invalid assisted OAuth code verifier. Restart the assisted OAuth login."));
            return false;
        }
        if (!IsBoundedTransientAuthId(RealtimeClientId))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("Assisted OAuth has no valid realtime client ID. Wait for the realtime connection before starting assisted OAuth."));
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
                TEXT("The client already has 16 active OAuth transactions. Finish or cancel one, or wait up to five minutes for an unused transaction to expire."));
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
                TEXT("The SDK could not allocate a unique OAuth transaction ID after four attempts. Retry the assisted OAuth login."));
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
                    TEXT("The secure session could not be saved, and the secure-store implementation returned no error details. Check secure storage availability and permissions, or use Memory Only persistence."));
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
            LastSessionReason = bUserSwitched
                ? EOpenPocketBaseSessionChangeReason::UserSwitched
                : RequestedReason;
            Snapshot.Reason = LastSessionReason;
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
            LastSessionReason = EOpenPocketBaseSessionChangeReason::RecordUpdated;
            Snapshot.Reason = LastSessionReason;
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
            LastSessionReason = EOpenPocketBaseSessionChangeReason::RecordUpdated;
            Snapshot.Reason = LastSessionReason;
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
            LastSessionReason = EOpenPocketBaseSessionChangeReason::LoggedOut;
            Snapshot.Reason = LastSessionReason;
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
                Batch.Entries[Index].GetCollectionName() != CurrentCollection)
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
            if (OperationResult.bHasReturnedRecord && OperationResult.Record.Id == CurrentRecordId)
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
            LastSessionReason = EOpenPocketBaseSessionChangeReason::LoggedOut;
            Snapshot.Reason = LastSessionReason;
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
            LastSessionReason = EOpenPocketBaseSessionChangeReason::Refreshed;
            Snapshot.Reason = LastSessionReason;
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
            LastSessionReason = EOpenPocketBaseSessionChangeReason::Restored;
            Snapshot.Reason = LastSessionReason;
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
        SetChildHandle(Client->DynamicCollection(AuthCollection).ListAuthMethods(
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
                TEXT("OAuth2 is disabled for this auth collection in PocketBase. Enable at least one OAuth2 provider or choose another login method.")));
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
                TEXT("The requested OAuth provider is not available on this auth collection. Choose one returned by Get Auth Methods.")));
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

        FOpenPocketBaseSubscriptionResult SubscriptionResult = Client->DynamicSubscribe(
            TEXT("@oauth2"),
            MoveTemp(Callbacks),
            {});
        if (!SubscriptionResult.IsSuccess())
        {
            FinishFailure(SubscriptionResult.GetError());
            return;
        }
        FOpenPocketBaseSubscriptionHandle NewSubscription =
            SubscriptionResult.TakeValue();
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
                    TEXT("The assisted OAuth realtime connection stopped before the provider result arrived. Restart the login and keep the client running until it completes.")));
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
                TEXT("The realtime connection is active but has no PocketBase client ID for assisted OAuth. Restart the realtime connection, then try the login again.")));
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
                TEXT("The assisted OAuth handoff state does not match the active login. The result was rejected. Restart the login.")));
            return;
        }
        if (bProviderRejected)
        {
            FinishFailure(MakeLocalError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("The OAuth provider returned an error or no valid authorization code. Restart the login and check provider permissions and redirect settings.")));
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
        SetChildHandle(Client->DynamicCollection(AuthCollection).CompleteOAuth2(
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
        Sanitized.Message = TEXT("The assisted OAuth realtime handoff failed before a provider result arrived. Restart the login and check realtime connectivity.");
        Sanitized.Code.Reset();
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
                TEXT("The realtime stream reported a gap during assisted OAuth, so the provider result may be missing. Restart the login instead of reusing this transaction.")));
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
            PinnedClient->DynamicCollection(Collection).GetList(
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
    FOpenPocketBaseAuthCollectionRef AuthCollection,
    const FOpenPocketBaseRecord& AuthRecord,
    FOpenPocketBaseClientDependencies Dependencies)
{
    FOpenPocketBaseAuthCollectionRef Current;
    if (!AuthCollection.ResolveCurrentAs(Current))
    {
        return FOpenPocketBaseClientResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Auth Collection is missing, stale, or is not an auth collection. Choose an auth collection from the current schema.")));
    }
    return CreateDynamicEphemeralAuthenticated(
        Config,
        MoveTemp(Token),
        MoveTemp(Current.Name),
        AuthRecord,
        MoveTemp(Dependencies));
}

FOpenPocketBaseClientResult
FOpenPocketBaseClient::CreateDynamicEphemeralAuthenticated(
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
    if (Token.IsEmpty() || Token.Len() > 8192)
    {
        Client->Shutdown();
        return FOpenPocketBaseClientResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Ephemeral Auth Token is empty or exceeds 8192 characters. Pass the token returned by a trusted PocketBase auth flow.")));
    }
    if (!IsSafePathSegment(AuthCollection))
    {
        Client->Shutdown();
        return FOpenPocketBaseClientResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Ephemeral Auth Collection is empty or contains an unsafe path character. Use the collection that issued the token.")));
    }
    if (AuthRecord.Id.IsEmpty() || AuthRecord.Id.Len() > 255)
    {
        Client->Shutdown();
        return FOpenPocketBaseClientResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Ephemeral Auth Record has an empty ID or an ID longer than 255 characters. Pass the auth record that belongs to the token.")));
    }
    for (const TCHAR Character : Token)
    {
        if (FChar::IsControl(Character) || FChar::IsWhitespace(Character))
        {
            Client->Shutdown();
            return FOpenPocketBaseClientResult::Failure(MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Ephemeral Auth Token contains whitespace or a control character. Pass the token exactly as returned by PocketBase.")));
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
            FString::Printf(
                TEXT("Auth Refresh Lead Time Seconds is %g. Use a finite value from 0 to 3600 seconds."),
                Config.AuthRefreshLeadTimeSeconds));
        return nullptr;
    }

    if (Config.SessionPersistence == EOpenPocketBaseSessionPersistence::RequireSecureStorage)
    {
        FString UnavailableReason;
        if (!SecureStore->IsAvailable(UnavailableReason))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::SecureStorage,
                TEXT("This client requires secure session storage, but no secure store is available. Use Memory Only persistence or add platform secure-store support."));
            if (!UnavailableReason.IsEmpty())
            {
                OutError.Message += TEXT(" ") + UnavailableReason;
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

FOpenPocketBaseCollectionService FOpenPocketBaseClient::Collection(
    FOpenPocketBaseCollectionRef CollectionReference)
{
    return FOpenPocketBaseCollectionService(AsShared(), MoveTemp(CollectionReference));
}

FOpenPocketBaseWritableCollectionService FOpenPocketBaseClient::WritableCollection(
    FOpenPocketBaseWritableCollectionRef CollectionReference)
{
    return FOpenPocketBaseWritableCollectionService(AsShared(), MoveTemp(CollectionReference));
}

FOpenPocketBaseWritableCollectionService FOpenPocketBaseClient::WritableCollection(
    FOpenPocketBaseCollectionRef CollectionReference)
{
    return FOpenPocketBaseWritableCollectionService(AsShared(), MoveTemp(CollectionReference));
}

FOpenPocketBaseAuthCollectionService FOpenPocketBaseClient::AuthCollection(
    FOpenPocketBaseAuthCollectionRef CollectionReference)
{
    return FOpenPocketBaseAuthCollectionService(AsShared(), MoveTemp(CollectionReference));
}

FOpenPocketBaseAuthCollectionService FOpenPocketBaseClient::AuthCollection(
    FOpenPocketBaseCollectionRef CollectionReference)
{
    return FOpenPocketBaseAuthCollectionService(AsShared(), MoveTemp(CollectionReference));
}

FOpenPocketBaseDynamicCollectionService FOpenPocketBaseClient::DynamicCollection(
    FString CollectionName)
{
    return FOpenPocketBaseDynamicCollectionService(AsShared(), MoveTemp(CollectionName));
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
                            TEXT("PocketBase health response must be a JSON object with an integer code and a one-line message up to 1024 characters.")));
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
    if (Method == nullptr)
    {
        ValidationError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Custom Route Method is not supported. Choose GET, POST, PUT, PATCH, or DELETE."));
    }
    else if (ValidateRequestOptions(CustomRequest.Options, ValidationError) &&
             TryBuildCustomRoutePath(
                 CustomRequest.Path, CustomRequest.Query, Path, ValidationError))
    {
        if (CustomRequest.MaxRequestBytes < 0 ||
            CustomRequest.MaxRequestBytes > 64LL * 1024 * 1024)
        {
            ValidationError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                FString::Printf(
                    TEXT("Custom Route Max Request Bytes is %lld. Use a value from 0 to 67108864 bytes."),
                    CustomRequest.MaxRequestBytes));
        }
        else if (CustomRequest.Method == EOpenPocketBaseCustomRouteMethod::Get &&
                 CustomRequest.BodyFormat != EOpenPocketBaseCustomBodyFormat::None)
        {
            ValidationError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Custom Route GET requests cannot contain a body. Set Body Format to None or choose another HTTP method."));
        }
    }
    if (ValidationError.IsSet())
    {
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

    if (!bBodyValid)
    {
        if (!ValidationError.IsSet())
        {
            FString BodyMessage;
            switch (CustomRequest.BodyFormat)
            {
            case EOpenPocketBaseCustomBodyFormat::None:
                BodyMessage = TEXT("Body Format is None, but body data, form fields, files, or Content Type was supplied. Clear those inputs or choose the matching body format.");
                break;
            case EOpenPocketBaseCustomBodyFormat::Json:
                BodyMessage = TEXT("Body Format is JSON. Supply a valid Json Body only, and leave form fields, files, raw body, and Content Type empty.");
                break;
            case EOpenPocketBaseCustomBodyFormat::Form:
                BodyMessage = TEXT("Body Format is Form. Supply at least one Form Field only, and leave files, raw body, and Content Type empty.");
                break;
            case EOpenPocketBaseCustomBodyFormat::Multipart:
                BodyMessage = TEXT("Body Format is Multipart. Supply at least one Form Field or File, and leave raw Body and Content Type empty.");
                break;
            case EOpenPocketBaseCustomBodyFormat::Raw:
            case EOpenPocketBaseCustomBodyFormat::Binary:
                BodyMessage = TEXT("Raw and Binary body formats require a valid Content Type and cannot include Form Fields or Files.");
                break;
            default:
                BodyMessage = TEXT("Body Format is not supported. Choose None, JSON, Form, Multipart, Raw, or Binary.");
                break;
            }
            ValidationError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                BodyMessage);
        }
        DispatchFailure<FOpenPocketBaseCustomRouteResponse>(
            MoveTemp(OnComplete), MoveTemp(ValidationError));
        return {};
    }
    const int64 ActualBodyBytes = BodyStream.IsValid() ? BodyLength : Body.Num();
    if (ActualBodyBytes > CustomRequest.MaxRequestBytes)
    {
        ValidationError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            FString::Printf(
                TEXT("Custom Route body is %lld bytes, but Max Request Bytes is %lld. Reduce the body or raise the limit."),
                ActualBodyBytes,
                CustomRequest.MaxRequestBytes));
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
                                TEXT("The custom route declared a JSON Content Type, but its response body is not valid JSON. Fix the route response or return the correct non-JSON Content Type.")));
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
                : TEXT("No platform secure store is available. Use Memory Only persistence or provide a secure-store implementation for this target.");
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
    OutSession.Reason = Impl->LastSessionReason;
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
                *FString::Printf(
                    TEXT("Serialized batch body is %d bytes, but Max Body Bytes is %lld."),
                    Body.Num(),
                    static_cast<long long>(Options.MaxBodyBytes))));
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

FOpenPocketBaseSubscriptionResult FOpenPocketBaseClient::DynamicSubscribe(
    FString Topic,
    FOpenPocketBaseRealtimeCallbacks Callbacks,
    FOpenPocketBaseRealtimeOptions Options)
{
    if (IsShutdown() || !Impl->Realtime.IsValid())
    {
        return FOpenPocketBaseSubscriptionResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::Cancelled,
            TEXT("The PocketBase client has shut down, so it cannot create a realtime subscription. Create or retrieve an active client first.")));
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
                TEXT("The realtime subscription did not start and no lower-level error was returned. Check the topic, options, and client state."));
        }
        return FOpenPocketBaseSubscriptionResult::Failure(MoveTemp(Error));
    }
    return FOpenPocketBaseSubscriptionResult::Success(MoveTemp(Subscription));
}

void FOpenPocketBaseClient::UnsubscribeDynamicTopic(const FString& Topic)
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
    FOpenPocketBaseCollectionRef InCollection,
    FString RecordId,
    FString FileName,
    FOpenPocketBaseFileUrlOptions Options) const
{
    FOpenPocketBaseCollectionRef Current;
    if (!InCollection.ResolveCurrent(Current))
    {
        return FOpenPocketBaseFileUrlResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Collection is missing or stale. Choose a collection from the current imported schema before building a file URL.")));
    }
    return DynamicBuildUrl(
        MoveTemp(Current.Name),
        MoveTemp(RecordId),
        MoveTemp(FileName),
        MoveTemp(Options));
}

FOpenPocketBaseFileUrlResult FOpenPocketBaseFileService::DynamicBuildUrl(
    FString InCollection,
    FString RecordId,
    FString FileName,
    FOpenPocketBaseFileUrlOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        return FOpenPocketBaseFileUrlResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before building a file URL.")));
    }
    if (!IsSafePathSegment(InCollection))
    {
        return FOpenPocketBaseFileUrlResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Collection Name is empty or contains an unsafe path character. Choose a collection from the current schema.")));
    }
    if (!IsSafePathSegment(RecordId))
    {
        return FOpenPocketBaseFileUrlResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Record ID is empty or contains an unsafe path character. Pass the ID of the record that owns the file.")));
    }
    if (!IsSafePathSegment(FileName))
    {
        return FOpenPocketBaseFileUrlResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("File Name is empty or contains an unsafe path character. Pass the stored PocketBase file name returned on the record.")));
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
            FString::Printf(
                TEXT("Thumbnail Width and Height are %d by %d. Each value must be from 0 to 16384. Use mode None only with 0 by 0, and choose a resize mode when either dimension is set."),
                Options.Thumbnail.Width,
                Options.Thumbnail.Height)));
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
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        DispatchFailure<FOpenPocketBaseFileToken>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before requesting a protected-file token.")));
        return {};
    }
    if (!PinnedClient->IsAuthenticated())
    {
        DispatchFailure<FOpenPocketBaseFileToken>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("A protected-file token requires an authenticated session. Log in before requesting the token.")));
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
                            TEXT("PocketBase returned an empty protected-file token response. Check the PocketBase version and server endpoint.")));
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
                            TEXT("PocketBase file token response must be a JSON object with a non-empty token no longer than 4096 characters.")));
                }
                for (const TCHAR Character : TokenValue)
                {
                    if (FChar::IsControl(Character))
                    {
                        return TOpenPocketBaseResult<FOpenPocketBaseFileToken>::Failure(
                            MakeLocalError(
                                EOpenPocketBaseErrorKind::Serialization,
                                TEXT("PocketBase returned a file token containing a control character. Refuse the token and check the server response.")));
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
    FOpenPocketBaseCollectionRef InCollection,
    FString RecordId,
    FString FileName,
    FOpenPocketBaseFileDownloadOptions Options,
    FOpenPocketBaseFileDownloadCallback OnComplete,
    FOpenPocketBaseFileToken Token,
    FOpenPocketBaseTransferProgressCallback OnProgress) const
{
    FOpenPocketBaseCollectionRef Current;
    if (!InCollection.ResolveCurrent(Current))
    {
        DispatchFailure<FOpenPocketBaseFileDownloadResult>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Collection is missing or stale. Choose a collection from the current imported schema before downloading a file.")));
        return {};
    }
    return DynamicDownload(
        MoveTemp(Current.Name),
        MoveTemp(RecordId),
        MoveTemp(FileName),
        MoveTemp(Options),
        MoveTemp(OnComplete),
        MoveTemp(Token),
        MoveTemp(OnProgress));
}

FOpenPocketBaseRequestHandle FOpenPocketBaseFileService::DynamicDownload(
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
                TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before downloading a file.")));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options.RequestOptions, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseFileDownloadResult>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    FOpenPocketBaseFileUrlResult UrlResult = DynamicBuildUrl(
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
                    TEXT("Protected File Token exceeds 4096 characters or contains a control character. Use the token returned by Get File Token without modifying it.")));
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
    FOpenPocketBaseCollectionRef InCollection)
    : Client(MoveTemp(InClient))
{
    if (InCollection.IsSet())
    {
        InCollection.ResolveCurrent(Reference);
        Collection = Reference.Name;
    }
    else
    {
        Collection = MoveTemp(InCollection.Name);
    }
}

FOpenPocketBaseWritableCollectionService::FOpenPocketBaseWritableCollectionService(
    TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient,
    FOpenPocketBaseCollectionRef InCollection)
    : FOpenPocketBaseCollectionService(MoveTemp(InClient), MoveTemp(InCollection))
{
}

FOpenPocketBaseWritableCollectionService::FOpenPocketBaseWritableCollectionService(
    TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient,
    FOpenPocketBaseWritableCollectionRef InCollection)
    : FOpenPocketBaseCollectionService(MoveTemp(InClient), MoveTemp(InCollection))
{
}

FOpenPocketBaseWritableCollectionService::FOpenPocketBaseWritableCollectionService(
    TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient,
    FString InCollection)
    : FOpenPocketBaseCollectionService(MoveTemp(InClient), MoveTemp(InCollection))
{
}

FOpenPocketBaseAuthCollectionService::FOpenPocketBaseAuthCollectionService(
    TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient,
    FOpenPocketBaseAuthCollectionRef InCollection)
    : FOpenPocketBaseWritableCollectionService(MoveTemp(InClient), MoveTemp(InCollection))
{
}

FOpenPocketBaseAuthCollectionService::FOpenPocketBaseAuthCollectionService(
    TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient,
    FOpenPocketBaseCollectionRef InCollection)
    : FOpenPocketBaseWritableCollectionService(MoveTemp(InClient), MoveTemp(InCollection))
{
}

FOpenPocketBaseAuthCollectionService::FOpenPocketBaseAuthCollectionService(
    TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient,
    FString InCollection)
    : FOpenPocketBaseWritableCollectionService(MoveTemp(InClient), MoveTemp(InCollection))
{
}

FOpenPocketBaseDynamicCollectionService::FOpenPocketBaseDynamicCollectionService(
    TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient,
    FString InCollection)
    : FOpenPocketBaseAuthCollectionService(MoveTemp(InClient), MoveTemp(InCollection))
{
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
    return IsSafePathSegment(Collection) && Client.IsValid() &&
        (Reference.Name.IsEmpty() || Reference.IsSet());
}

bool FOpenPocketBaseWritableCollectionService::IsValid() const
{
    return FOpenPocketBaseCollectionService::IsValid() &&
        (Reference.Name.IsEmpty() || FOpenPocketBaseWritableCollectionRef::Accepts(Reference));
}

bool FOpenPocketBaseAuthCollectionService::IsValid() const
{
    return FOpenPocketBaseCollectionService::IsValid() &&
        (Reference.Name.IsEmpty() || FOpenPocketBaseAuthCollectionRef::Accepts(Reference));
}

bool FOpenPocketBaseCollectionService::ValidateRecordOptions(
    const FOpenPocketBaseRecordOptions& Options,
    FOpenPocketBaseError& OutError) const
{
    for (int32 ExpandIndex = 0; ExpandIndex < Options.Expand.Num(); ++ExpandIndex)
    {
        if (!Options.Expand[ExpandIndex].IsSet())
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                FString::Printf(
                    TEXT("Record Options Expand entry %d is empty. Remove it or choose a relation field."),
                    ExpandIndex + 1));
            return false;
        }
    }
    for (int32 FieldIndex = 0; FieldIndex < Options.Fields.Num(); ++FieldIndex)
    {
        if (!Options.Fields[FieldIndex].IsSet())
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                FString::Printf(
                    TEXT("Record Options Fields entry %d is empty. Remove it or choose a field."),
                    FieldIndex + 1));
            return false;
        }
    }
    if (Reference.IsSet())
    {
        for (int32 ExpandIndex = 0; ExpandIndex < Options.Expand.Num(); ++ExpandIndex)
        {
            if (!Options.Expand[ExpandIndex].BelongsTo(Reference))
            {
                OutError = MakeLocalError(
                    EOpenPocketBaseErrorKind::InvalidArgument,
                    FString::Printf(
                        TEXT("Record Options Expand entry %d belongs to another collection. Choose it again from the selected collection."),
                        ExpandIndex + 1));
                return false;
            }
        }
        for (int32 FieldIndex = 0; FieldIndex < Options.Fields.Num(); ++FieldIndex)
        {
            if (!Options.Fields[FieldIndex].BelongsTo(Reference))
            {
                OutError = MakeLocalError(
                    EOpenPocketBaseErrorKind::InvalidArgument,
                    FString::Printf(
                        TEXT("Record Options Fields entry %d belongs to another collection. Choose it again from the selected collection."),
                        FieldIndex + 1));
                return false;
            }
        }
    }
    return true;
}

bool FOpenPocketBaseCollectionService::ValidateListOptions(
    const FOpenPocketBaseListOptions& Options,
    FOpenPocketBaseError& OutError) const
{
    if (!Options.Filter.IsValid())
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            Options.Filter.ErrorMessage.IsEmpty()
                ? TEXT("List Options Filter is invalid. Rebuild it with the schema-driven filter nodes.")
                : FString::Printf(TEXT("List Options Filter is invalid. %s"), *Options.Filter.ErrorMessage));
        return false;
    }
    for (int32 SortIndex = 0; SortIndex < Options.Sort.Num(); ++SortIndex)
    {
        if (!Options.Sort[SortIndex].IsSet())
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                FString::Printf(TEXT("List Options Sort entry %d is empty. Remove it or choose a field."), SortIndex + 1));
            return false;
        }
    }
    for (int32 ExpandIndex = 0; ExpandIndex < Options.Expand.Num(); ++ExpandIndex)
    {
        if (!Options.Expand[ExpandIndex].IsSet())
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                FString::Printf(TEXT("List Options Expand entry %d is empty. Remove it or choose a relation field."), ExpandIndex + 1));
            return false;
        }
    }
    for (int32 FieldIndex = 0; FieldIndex < Options.Fields.Num(); ++FieldIndex)
    {
        if (!Options.Fields[FieldIndex].IsSet())
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                FString::Printf(TEXT("List Options Fields entry %d is empty. Remove it or choose a field."), FieldIndex + 1));
            return false;
        }
    }
    if (Reference.IsSet() && !Options.Filter.BelongsTo(Reference))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("List Options Filter was built for another collection. Rebuild it from the selected collection."));
        return false;
    }
    if (Reference.IsSet())
    {
        for (int32 SortIndex = 0; SortIndex < Options.Sort.Num(); ++SortIndex)
        {
            if (!Options.Sort[SortIndex].BelongsTo(Reference))
            {
                OutError = MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                    FString::Printf(TEXT("List Options Sort entry %d belongs to another collection. Choose it again from the selected collection."), SortIndex + 1));
                return false;
            }
        }
        for (int32 ExpandIndex = 0; ExpandIndex < Options.Expand.Num(); ++ExpandIndex)
        {
            if (!Options.Expand[ExpandIndex].BelongsTo(Reference))
            {
                OutError = MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                    FString::Printf(TEXT("List Options Expand entry %d belongs to another collection. Choose it again from the selected collection."), ExpandIndex + 1));
                return false;
            }
        }
        for (int32 FieldIndex = 0; FieldIndex < Options.Fields.Num(); ++FieldIndex)
        {
            if (!Options.Fields[FieldIndex].BelongsTo(Reference))
            {
                OutError = MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                    FString::Printf(TEXT("List Options Fields entry %d belongs to another collection. Choose it again from the selected collection."), FieldIndex + 1));
                return false;
            }
        }
    }
    return true;
}

bool FOpenPocketBaseWritableCollectionService::ValidateBody(
    const FOpenPocketBaseRecordBody& Body,
    FOpenPocketBaseError& OutError) const
{
    if (!Body.IsValid())
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            Body.ErrorMessage.IsEmpty()
                ? TEXT("Record Body is invalid. Start with New Record Body and add fields from the selected collection.")
                : Body.ErrorMessage);
        return false;
    }
    if (Reference.IsSet() && !Body.BelongsTo(Reference))
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Record Body was built for another collection. Rebuild it from the selected collection."));
        return false;
    }
    return true;
}

bool FOpenPocketBaseWritableCollectionService::ValidateFiles(
    const TArray<FOpenPocketBaseFileInput>& Files,
    FOpenPocketBaseError& OutError) const
{
    TMap<FString, int32> FieldCounts;
    for (int32 FileIndex = 0; FileIndex < Files.Num(); ++FileIndex)
    {
        const FOpenPocketBaseFileInput& File = Files[FileIndex];
        if (!File.IsValid() || (Reference.IsSet() && !File.BelongsTo(Reference)))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                FString::Printf(
                    TEXT("File %d has no valid file field from the selected collection. Choose its field again."),
                    FileIndex + 1));
            return false;
        }

        FOpenPocketBaseFileFieldRef CurrentField;
        if (!File.Field.ResolveCurrentAs(CurrentField))
        {
            continue;
        }
        const bool bMimeAllowed = CurrentField.MimeTypes.IsEmpty() ||
            CurrentField.MimeTypes.Contains(File.ContentType) ||
            CurrentField.MimeTypes.ContainsByPredicate(
                [&File](const FString& Allowed)
                {
                    return Allowed.EndsWith(TEXT("/*")) &&
                        File.ContentType.StartsWith(Allowed.LeftChop(1));
                });
        const int64 FileSize = File.bUseFilePath
            ? IFileManager::Get().FileSize(*File.FilePath)
            : File.Bytes.Num();
        if (!bMimeAllowed)
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                FString::Printf(
                    TEXT("File %d uses Content Type '%s', which field '%s' does not accept."),
                    FileIndex + 1,
                    *File.ContentType,
                    *CurrentField.Name));
            return false;
        }
        if (CurrentField.MaxSizeBytes > 0 && FileSize >= 0 &&
            FileSize > CurrentField.MaxSizeBytes)
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                FString::Printf(
                    TEXT("File %d is %lld bytes, but field '%s' accepts at most %lld bytes."),
                    FileIndex + 1,
                    FileSize,
                    *CurrentField.Name,
                    CurrentField.MaxSizeBytes));
            return false;
        }
        if (!CurrentField.bMultiple &&
            File.Modifier != EOpenPocketBaseFieldModifier::Replace)
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                FString::Printf(
                    TEXT("File %d uses append or remove for single-file field '%s'. Use Replace instead."),
                    FileIndex + 1,
                    *CurrentField.Name));
            return false;
        }
        ++FieldCounts.FindOrAdd(CurrentField.FieldId);
        if (CurrentField.MaxSelect > 0 &&
            FieldCounts.FindRef(CurrentField.FieldId) > CurrentField.MaxSelect)
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                *FString::Printf(
                    TEXT("Field '%s' received too many files. It accepts at most %d files."),
                    *CurrentField.Name,
                    CurrentField.MaxSelect));
            return false;
        }
    }
    return true;
}

bool FOpenPocketBaseWritableCollectionService::ValidateCreateBody(
    const FOpenPocketBaseRecordBody& Body,
    const TArray<FOpenPocketBaseFileInput>* Files,
    FOpenPocketBaseError& OutError) const
{
    if (Reference.Schema.IsNull())
    {
        return true;
    }
    UOpenPocketBaseSchema* Schema = Reference.Schema.LoadSynchronous();
    const FOpenPocketBaseSchemaCollection* SchemaCollection = nullptr;
    if (Schema == nullptr || !Schema->ResolveCollection(Reference, SchemaCollection) ||
        SchemaCollection == nullptr)
    {
        OutError = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("The selected collection no longer exists in the imported schema."));
        return false;
    }

    TArray<FString> Missing;
    for (const FOpenPocketBaseSchemaField& Field : SchemaCollection->Fields)
    {
        if (!Field.bRequired || Field.bReadOnly ||
            (Field.bSystem && Field.bHidden) ||
            Field.Storage != EOpenPocketBaseFieldStorage::Data)
        {
            continue;
        }

        bool bPresent = Body.Data.JsonObject.IsValid() &&
            Body.Data.JsonObject->HasField(Field.Name);
        if (!bPresent && Field.Type == EOpenPocketBaseFieldType::File && Files != nullptr)
        {
            bPresent = Files->ContainsByPredicate(
                [&Field](const FOpenPocketBaseFileInput& File)
                {
                    return File.GetFieldName() == Field.Name;
                });
        }
        if (!bPresent)
        {
            Missing.Add(Field.Name);
        }
    }
    if (Missing.IsEmpty())
    {
        return true;
    }

    OutError = MakeLocalError(
        EOpenPocketBaseErrorKind::InvalidArgument,
        *FString::Printf(
            TEXT("Create Record is missing required fields: %s. Add them to the record body or file inputs."),
            *FString::Join(Missing, TEXT(", "))));
    return false;
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::GetOne(
    FString RecordId,
    FOpenPocketBaseRecordCallback OnComplete,
    FOpenPocketBaseRecordOptions Options) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    FString ValidationMessage;
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        ValidationMessage = TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before getting a record.");
    }
    else if (!IsValid())
    {
        ValidationMessage = TEXT("Collection is missing, stale, or invalid. Choose it again from the current schema.");
    }
    else if (!IsSafePathSegment(RecordId))
    {
        ValidationMessage = TEXT("Record ID is empty or contains an unsafe path character. Pass an ID returned by PocketBase.");
    }
    if (!ValidationMessage.IsEmpty())
    {
        DispatchFailure<FOpenPocketBaseRecord>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument, ValidationMessage));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateRecordOptions(Options, OptionsError) ||
        !ValidateRequestOptions(Options.RequestOptions, OptionsError))
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
    FString ValidationMessage;
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        ValidationMessage = TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before listing records.");
    }
    else if (!IsValid())
    {
        ValidationMessage = TEXT("Collection is missing, stale, or invalid. Choose it again from the current schema.");
    }
    else if (Options.Page < 1)
    {
        ValidationMessage = FString::Printf(TEXT("Page is %d. Use page 1 or greater."), Options.Page);
    }
    else if (Options.PerPage < 1)
    {
        ValidationMessage = FString::Printf(TEXT("Per Page is %d. Use 1 or more records per page."), Options.PerPage);
    }
    if (!ValidationMessage.IsEmpty())
    {
        DispatchFailure<FOpenPocketBaseRecordPage>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                ValidationMessage));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateListOptions(Options, OptionsError) ||
        !ValidateRequestOptions(Options.RequestOptions, OptionsError))
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
    const auto Fail = [&OnComplete](const TCHAR* Message)
    {
        DispatchFailure<FOpenPocketBaseFullListResult>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                Message));
    };
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        Fail(TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before getting a full record list."));
        return {};
    }
    if (!IsValid())
    {
        Fail(TEXT("Collection is missing, stale, or invalid. Choose it again from the current schema before getting a full record list."));
        return {};
    }
    if (Options.ListOptions.Page != 1)
    {
        Fail(TEXT("Start Page must be 1 for a full record list. The SDK walks forward from the first page."));
        return {};
    }
    if (Options.ListOptions.PerPage < 1)
    {
        Fail(*FString::Printf(TEXT("Per Page is %d. Use 1 or more records per request."), Options.ListOptions.PerPage));
        return {};
    }
    if (Options.MaxItems < 0 || Options.MaxItems > 1000000)
    {
        Fail(*FString::Printf(TEXT("Max Items is %d. Use a value from 0 to 1000000."), Options.MaxItems));
        return {};
    }
    if (Options.MaxPages < 0 || Options.MaxPages > 10000)
    {
        Fail(*FString::Printf(TEXT("Max Pages is %d. Use a value from 0 to 10000."), Options.MaxPages));
        return {};
    }
    if (Options.MaxItems == 0 && Options.MaxPages == 0)
    {
        Fail(TEXT("Max Items and Max Pages are both 0. Set at least one limit so full-list traversal has an explicit stopping bound."));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateListOptions(Options.ListOptions, OptionsError) ||
        !ValidateRequestOptions(Options.ListOptions.RequestOptions, OptionsError))
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
                Error.Code = TEXT("404");
                Error.Message = TEXT("No record matched the supplied filter. Check the filter values and collection data.");
                OnComplete(TOpenPocketBaseResult<FOpenPocketBaseRecord>::Failure(MoveTemp(Error)));
                return;
            }
            OnComplete(TOpenPocketBaseResult<FOpenPocketBaseRecord>::Success(Result.GetValue().Items[0]));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseWritableCollectionService::Create(
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordCallback OnComplete,
    FOpenPocketBaseRecordOptions Options) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    FString ValidationMessage;
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        ValidationMessage = TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before creating a record.");
    }
    else if (!IsValid())
    {
        ValidationMessage = TEXT("Collection is missing, stale, or not writable. Choose a writable collection from the current schema.");
    }
    else if (!Body.Data.JsonObject.IsValid())
    {
        ValidationMessage = TEXT("Record Body has no valid JSON data. Start with New Record Body using the selected collection.");
    }
    if (!ValidationMessage.IsEmpty())
    {
        DispatchFailure<FOpenPocketBaseRecord>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                ValidationMessage));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateBody(Body, OptionsError) ||
        !ValidateCreateBody(Body, nullptr, OptionsError) ||
        !ValidateRecordOptions(Options, OptionsError) ||
        !ValidateRequestOptions(Options.RequestOptions, OptionsError))
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

FOpenPocketBaseRequestHandle FOpenPocketBaseWritableCollectionService::CreateWithFiles(
    FOpenPocketBaseRecordBody Body,
    TArray<FOpenPocketBaseFileInput> Files,
    FOpenPocketBaseRecordCallback OnComplete,
    FOpenPocketBaseRecordOptions Options,
    FOpenPocketBaseUploadLimits Limits,
    FOpenPocketBaseTransferProgressCallback OnProgress) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    FString ValidationMessage;
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        ValidationMessage = TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before creating a record with files.");
    }
    else if (!IsValid())
    {
        ValidationMessage = TEXT("Collection is missing, stale, or not writable. Choose a writable collection from the current schema.");
    }
    else if (!Body.Data.JsonObject.IsValid())
    {
        ValidationMessage = TEXT("Record Body has no valid JSON data. Start with New Record Body using the selected collection.");
    }
    else if (Files.IsEmpty())
    {
        ValidationMessage = TEXT("Files is empty. Add at least one file input, or use Create Record when no upload is needed.");
    }
    if (!ValidationMessage.IsEmpty())
    {
        DispatchFailure<FOpenPocketBaseRecord>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                ValidationMessage));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateBody(Body, OptionsError) ||
        !ValidateFiles(Files, OptionsError) ||
        !ValidateCreateBody(Body, &Files, OptionsError) ||
        !ValidateRecordOptions(Options, OptionsError) ||
        !ValidateRequestOptions(Options.RequestOptions, OptionsError))
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

FOpenPocketBaseRequestHandle FOpenPocketBaseWritableCollectionService::Update(
    FString RecordId,
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordCallback OnComplete,
    FOpenPocketBaseRecordOptions Options) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    FString ValidationMessage;
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        ValidationMessage = TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before updating a record.");
    }
    else if (!IsValid())
    {
        ValidationMessage = TEXT("Collection is missing, stale, or not writable. Choose a writable collection from the current schema.");
    }
    else if (!IsSafePathSegment(RecordId))
    {
        ValidationMessage = TEXT("Record ID is empty or contains an unsafe path character. Pass an ID returned by PocketBase.");
    }
    else if (!Body.Data.JsonObject.IsValid())
    {
        ValidationMessage = TEXT("Record Body has no valid JSON data. Start with New Record Body using the selected collection.");
    }
    if (!ValidationMessage.IsEmpty())
    {
        DispatchFailure<FOpenPocketBaseRecord>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                ValidationMessage));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateBody(Body, OptionsError) ||
        !ValidateRecordOptions(Options, OptionsError) ||
        !ValidateRequestOptions(Options.RequestOptions, OptionsError))
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

FOpenPocketBaseRequestHandle FOpenPocketBaseWritableCollectionService::UpdateWithFiles(
    FString RecordId,
    FOpenPocketBaseRecordBody Body,
    TArray<FOpenPocketBaseFileInput> Files,
    FOpenPocketBaseRecordCallback OnComplete,
    FOpenPocketBaseRecordOptions Options,
    FOpenPocketBaseUploadLimits Limits,
    FOpenPocketBaseTransferProgressCallback OnProgress) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    FString ValidationMessage;
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        ValidationMessage = TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before updating a record with files.");
    }
    else if (!IsValid())
    {
        ValidationMessage = TEXT("Collection is missing, stale, or not writable. Choose a writable collection from the current schema.");
    }
    else if (!IsSafePathSegment(RecordId))
    {
        ValidationMessage = TEXT("Record ID is empty or contains an unsafe path character. Pass an ID returned by PocketBase.");
    }
    else if (!Body.Data.JsonObject.IsValid())
    {
        ValidationMessage = TEXT("Record Body has no valid JSON data. Start with New Record Body using the selected collection.");
    }
    else if (Files.IsEmpty())
    {
        ValidationMessage = TEXT("Files is empty. Add at least one file input, or use Update Record when no upload is needed.");
    }
    if (!ValidationMessage.IsEmpty())
    {
        DispatchFailure<FOpenPocketBaseRecord>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                ValidationMessage));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateBody(Body, OptionsError) ||
        !ValidateFiles(Files, OptionsError) ||
        !ValidateRecordOptions(Options, OptionsError) ||
        !ValidateRequestOptions(Options.RequestOptions, OptionsError))
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

FOpenPocketBaseRequestHandle FOpenPocketBaseWritableCollectionService::Delete(
    FString RecordId,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    FString ValidationMessage;
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        ValidationMessage = TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before deleting a record.");
    }
    else if (!IsValid())
    {
        ValidationMessage = TEXT("Collection is missing, stale, or not writable. Choose a writable collection from the current schema.");
    }
    else if (!IsSafePathSegment(RecordId))
    {
        ValidationMessage = TEXT("Record ID is empty or contains an unsafe path character. Pass an ID returned by PocketBase.");
    }
    if (!ValidationMessage.IsEmpty())
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                ValidationMessage));
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

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::ListAuthMethods(
    FOpenPocketBaseAuthMethodsCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        DispatchFailure<FOpenPocketBaseAuthMethods>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before listing auth methods.")));
        return {};
    }
    if (!IsValid())
    {
        DispatchFailure<FOpenPocketBaseAuthMethods>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Auth Collection is missing, stale, or is not an auth collection. Choose it again from the current schema.")));
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

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::RequestOtp(
    FString Email,
    FOpenPocketBaseOtpRequestCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        DispatchFailure<FOpenPocketBaseOtpRequest>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before requesting an OTP.")));
        return {};
    }
    if (!IsValid())
    {
        DispatchFailure<FOpenPocketBaseOtpRequest>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Auth Collection is missing, stale, or is not an auth collection. Choose it again from the current schema.")));
        return {};
    }
    if (!IsSafeOAuthValue(Email, 320))
    {
        DispatchFailure<FOpenPocketBaseOtpRequest>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Email is empty, exceeds 320 characters, or contains a control character.")));
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

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::AuthWithPassword(
    FString Identity,
    FString Password,
    FOpenPocketBaseAuthAttemptCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before logging in.")));
        return {};
    }
    if (!IsValid())
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Auth Collection is missing, stale, or is not an auth collection. Choose it again from the current schema.")));
        return {};
    }
    if (!IsSafeOAuthValue(Identity, 320))
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Identity is empty, exceeds 320 characters, or contains a control character.")));
        return {};
    }
    if (!IsSafeOAuthValue(Password, 4096))
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Password is empty, exceeds 4096 characters, or contains a control character.")));
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

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::AuthWithOtp(
    FString OtpId,
    FString Password,
    FOpenPocketBaseMfaContinuation Mfa,
    FOpenPocketBaseAuthAttemptCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before completing OTP login.")));
        return {};
    }
    if (!IsValid())
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Auth Collection is missing, stale, or is not an auth collection. Choose it again from the current schema.")));
        return {};
    }
    if (!IsBoundedTransientAuthId(OtpId))
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("OTP ID is empty, exceeds 256 characters, or contains whitespace. Use the ID returned by Request OTP.")));
        return {};
    }
    if (!IsSafeOAuthValue(Password, 4096))
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("OTP Password is empty, exceeds 4096 characters, or contains a control character.")));
        return {};
    }
    if (Mfa.IsSet() && !IsBoundedTransientAuthId(Mfa.Id))
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("MFA Continuation ID exceeds 256 characters or contains whitespace. Use the continuation returned by the previous auth attempt.")));
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

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::BeginOAuth2(
    FOpenPocketBaseOAuth2StartOptions Options,
    FOpenPocketBaseOAuth2AuthorizationCallback OnComplete) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        DispatchFailure<FOpenPocketBaseOAuth2Authorization>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before starting OAuth2.")));
        return {};
    }
    if (!IsValid())
    {
        DispatchFailure<FOpenPocketBaseOAuth2Authorization>(MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Auth Collection is missing, stale, or is not an auth collection. Choose it again from the current schema.")));
        return {};
    }
    if (!IsSafeOAuthValue(Options.Provider, 128))
    {
        DispatchFailure<FOpenPocketBaseOAuth2Authorization>(MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("OAuth Provider is empty, exceeds 128 characters, or contains a control character. Choose a provider returned by Get Auth Methods.")));
        return {};
    }
    if (!IsSafeOAuthValue(Options.RedirectUrl, 8192))
    {
        DispatchFailure<FOpenPocketBaseOAuth2Authorization>(MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("OAuth Redirect URL is empty, exceeds 8192 characters, or contains a control character.")));
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
                            TEXT("OAuth2 is disabled for this auth collection in PocketBase. Enable at least one OAuth2 provider or choose another login method."))));
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
                            TEXT("The requested OAuth provider is not available on this auth collection. Choose one returned by Get Auth Methods."))));
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

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::CompleteOAuth2(
    FOpenPocketBaseOAuth2Callback Callback,
    FOpenPocketBaseAuthAttemptCallback OnComplete) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(MoveTemp(OnComplete), MakeCancelledError());
        return {};
    }
    FString ValidationMessage;
    if (!IsValid())
    {
        ValidationMessage = TEXT("Auth Collection is missing, stale, or is not an auth collection. Use the same collection that started OAuth2.");
    }
    else if (!IsBoundedTransientAuthId(Callback.TransactionId))
    {
        ValidationMessage = TEXT("OAuth Transaction ID is empty, exceeds 256 characters, or contains whitespace. Use the callback for the active OAuth transaction.");
    }
    else if (Callback.CallbackUrl.IsEmpty() || Callback.CallbackUrl.Len() > 8192)
    {
        ValidationMessage = TEXT("OAuth Callback URL is empty or exceeds 8192 characters. Pass the full URL returned by the OAuth provider.");
    }
    else if (Callback.Mfa.IsSet() && !IsBoundedTransientAuthId(Callback.Mfa.Id))
    {
        ValidationMessage = TEXT("MFA Continuation ID exceeds 256 characters or contains whitespace. Use the continuation returned by the previous auth attempt.");
    }
    if (!ValidationMessage.IsEmpty())
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                ValidationMessage));
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
    const int32 CreateDataBytes = bHasCreateData
        ? OpenPocketBase::Json::SerializeObject(
            Callback.CreateData.Data.JsonObject.ToSharedRef()).Num()
        : 0;
    if (CreateDataBytes > 64 * 1024)
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                FString::Printf(
                    TEXT("OAuth Create Data is %d bytes. Use at most 65536 bytes."),
                    CreateDataBytes)));
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

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::AuthWithOAuth2(
    FOpenPocketBaseAssistedOAuth2Options Options,
    FOpenPocketBaseAuthAttemptCallback OnComplete) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    FString ValidationMessage;
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        ValidationMessage = TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before starting assisted OAuth2.");
    }
    else if (!IsValid())
    {
        ValidationMessage = TEXT("Auth Collection is missing, stale, or is not an auth collection. Choose it again from the current schema.");
    }
    else if (!IsSafeOAuthValue(Options.Provider, 128))
    {
        ValidationMessage = TEXT("OAuth Provider is empty, exceeds 128 characters, or contains a control character. Choose a provider returned by Get Auth Methods.");
    }
    else if (Options.Mfa.IsSet() && !IsBoundedTransientAuthId(Options.Mfa.Id))
    {
        ValidationMessage = TEXT("MFA Continuation ID exceeds 256 characters or contains whitespace. Use the continuation returned by the previous auth attempt.");
    }
    if (!ValidationMessage.IsEmpty())
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                ValidationMessage));
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
                FString::Printf(
                    TEXT("OAuth Scopes contains %d entries. Use at most 32 scopes."),
                    Options.Scopes.Num())));
        return {};
    }
    for (int32 ScopeIndex = 0; ScopeIndex < Options.Scopes.Num(); ++ScopeIndex)
    {
        const FString& Scope = Options.Scopes[ScopeIndex];
        if (!IsSafeOAuthValue(Scope, 256) || Scope.Contains(TEXT(" ")))
        {
            DispatchFailure<FOpenPocketBaseAuthAttempt>(
                MoveTemp(OnComplete),
                MakeLocalError(
                    EOpenPocketBaseErrorKind::InvalidArgument,
                    FString::Printf(
                        TEXT("OAuth Scope %d is empty, exceeds 256 characters, contains a space, or contains a control character."),
                        ScopeIndex + 1)));
            return {};
        }
    }
    const int32 CreateDataBytes = Options.CreateData.Data.JsonObject.IsValid() &&
        !Options.CreateData.Data.JsonObject->Values.IsEmpty()
        ? OpenPocketBase::Json::SerializeObject(
            Options.CreateData.Data.JsonObject.ToSharedRef()).Num()
        : 0;
    if (CreateDataBytes > 64 * 1024)
    {
        DispatchFailure<FOpenPocketBaseAuthAttempt>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                FString::Printf(
                    TEXT("OAuth Create Data is %d bytes. Use at most 65536 bytes."),
                    CreateDataBytes)));
        return {};
    }

    const FOpenPocketBaseCapabilityInfo Capability =
        PinnedClient->GetCapability(EOpenPocketBaseCapability::OAuthCallback);
    if (!Capability.IsSupported())
    {
        FOpenPocketBaseError Error = MakeLocalError(
            EOpenPocketBaseErrorKind::Unsupported,
            TEXT("Assisted OAuth is unavailable for this client."));
        if (!Capability.Reason.IsEmpty())
        {
            Error.Message += TEXT(" ") + Capability.Reason;
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

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::SendAccountPost(
    FString Route,
    TMap<FString, FString> BodyFields,
    const bool bUseAuth,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options,
    TUniqueFunction<bool(FOpenPocketBaseError&)> OnSucceeded) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before sending this account request.")));
        return {};
    }
    if (!IsValid())
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Auth Collection is missing, stale, or is not an auth collection. Choose it again from the current schema.")));
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

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::RequestPasswordReset(
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
                TEXT("Email is empty, exceeds 320 characters, or contains a control character.")));
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

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::ConfirmPasswordReset(
    FString Token,
    FString Password,
    FString PasswordConfirm,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    FString ValidationMessage;
    FString ValidationField;
    FString ValidationCode;
    FString ValidationFieldMessage;
    if (!IsSafeOAuthValue(Token, 8192))
    {
        ValidationMessage = TEXT("Password Reset Token is empty, exceeds 8192 characters, or contains a control character. Use the token from the reset link.");
        ValidationField = TEXT("token");
        ValidationCode = TEXT("validation_invalid");
        ValidationFieldMessage = TEXT("Use the token from the password-reset link.");
    }
    else if (!IsSafeOAuthValue(Password, 4096))
    {
        ValidationMessage = TEXT("New Password is empty, exceeds 4096 characters, or contains a control character.");
        ValidationField = TEXT("password");
        ValidationCode = TEXT("validation_invalid");
        ValidationFieldMessage = TEXT("Enter a non-empty password without control characters.");
    }
    else if (PasswordConfirm.IsEmpty())
    {
        ValidationMessage = TEXT("Password Confirm is empty. Enter the new password again.");
        ValidationField = TEXT("passwordConfirm");
        ValidationCode = TEXT("validation_required");
        ValidationFieldMessage = TEXT("Enter the new password again.");
    }
    else if (Password != PasswordConfirm)
    {
        ValidationMessage = TEXT("New Password and Password Confirm do not match.");
        ValidationField = TEXT("passwordConfirm");
        ValidationCode = TEXT("validation_mismatch");
        ValidationFieldMessage = TEXT("Must match New Password.");
    }
    if (!ValidationMessage.IsEmpty())
    {
        FOpenPocketBaseError Error = MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            ValidationMessage);
        FOpenPocketBaseFieldError FieldError;
        FieldError.Code = MoveTemp(ValidationCode);
        FieldError.Message = MoveTemp(ValidationFieldMessage);
        Error.FieldErrors.Add(MoveTemp(ValidationField), MoveTemp(FieldError));
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MoveTemp(Error));
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

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::RequestVerification(
    FString Email,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    if (!IsSafeOAuthValue(Email, 320))
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Email is empty, exceeds 320 characters, or contains a control character.")));
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

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::ConfirmVerification(
    FString Token,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    if (!IsSafeOAuthValue(Token, 8192))
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Verification Token is empty, exceeds 8192 characters, or contains a control character. Use the token from the verification link.")));
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

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::RequestEmailChange(
    FString NewEmail,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    if (!IsSafeOAuthValue(NewEmail, 320))
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("New Email is empty, exceeds 320 characters, or contains a control character.")));
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

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::ConfirmEmailChange(
    FString Token,
    FString Password,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    FString ValidationMessage;
    if (!IsSafeOAuthValue(Token, 8192))
    {
        ValidationMessage = TEXT("Email Change Token is empty, exceeds 8192 characters, or contains a control character. Use the token from the email-change link.");
    }
    else if (!IsSafeOAuthValue(Password, 4096))
    {
        ValidationMessage = TEXT("Password is empty, exceeds 4096 characters, or contains a control character.");
    }
    if (!ValidationMessage.IsEmpty())
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                ValidationMessage));
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

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::ListExternalAuths(
    FString RecordId,
    FOpenPocketBaseExternalAuthsCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    FString ValidationMessage;
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        ValidationMessage = TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before listing linked external auths.");
    }
    else if (!IsValid())
    {
        ValidationMessage = TEXT("Auth Collection is missing, stale, or is not an auth collection. Choose it again from the current schema.");
    }
    else if (!IsSafePathSegment(RecordId))
    {
        ValidationMessage = TEXT("Record ID is empty or contains an unsafe path character. Pass the auth record ID returned by PocketBase.");
    }
    if (!ValidationMessage.IsEmpty())
    {
        DispatchFailure<TArray<FOpenPocketBaseExternalAuth>>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                ValidationMessage));
        return {};
    }
    FOpenPocketBaseDynamicFilterParams Params;
    Params.AddString(TEXT("id"), RecordId);
    FOpenPocketBaseFilter Filter;
    FOpenPocketBaseError FilterError;
    if (!FOpenPocketBaseFilter::TryBindDynamic(
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
    return PinnedClient->DynamicCollection(TEXT("_externalAuths")).GetList(
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
                            TEXT("PocketBase returned a linked external-auth record without the required provider, provider ID, or record reference fields."))));
                    return;
                }
                ExternalAuths.Add(MoveTemp(ExternalAuth));
            }
            OnComplete(TOpenPocketBaseResult<TArray<FOpenPocketBaseExternalAuth>>::Success(
                MoveTemp(ExternalAuths)));
        });
}

FOpenPocketBaseRequestHandle FOpenPocketBaseAuthCollectionService::UnlinkExternalAuth(
    FString RecordId,
    FString Provider,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    FString ValidationMessage;
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        ValidationMessage = TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before unlinking an external auth.");
    }
    else if (!IsValid())
    {
        ValidationMessage = TEXT("Auth Collection is missing, stale, or is not an auth collection. Choose it again from the current schema.");
    }
    else if (!IsSafePathSegment(RecordId))
    {
        ValidationMessage = TEXT("Record ID is empty or contains an unsafe path character. Pass the auth record ID returned by PocketBase.");
    }
    else if (!IsSafeOAuthValue(Provider, 128))
    {
        ValidationMessage = TEXT("Provider is empty, exceeds 128 characters, or contains a control character. Use the provider name returned by List External Auths.");
    }
    if (!ValidationMessage.IsEmpty())
    {
        DispatchFailure<bool>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument,
                ValidationMessage));
        return {};
    }
    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options, OptionsError))
    {
        DispatchFailure<bool>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }
    FOpenPocketBaseDynamicFilterParams Params;
    Params.AddString(TEXT("recordId"), RecordId);
    Params.AddString(TEXT("provider"), Provider);
    FOpenPocketBaseFilter Filter;
    FOpenPocketBaseError FilterError;
    if (!FOpenPocketBaseFilter::TryBindDynamic(
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
        PinnedClient->DynamicCollection(TEXT("_externalAuths")).GetFirstListItem(
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
                    PinnedClient->DynamicCollection(TEXT("_externalAuths")).Delete(
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
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        return FOpenPocketBaseSubscriptionResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before subscribing to records.")));
    }
    if (!IsValid())
    {
        return FOpenPocketBaseSubscriptionResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Collection is missing, stale, or invalid. Choose it again from the current schema before subscribing.")));
    }
    if (Options.IsValid() && Reference.IsSet() && !Options.BelongsTo(Reference))
    {
        return FOpenPocketBaseSubscriptionResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Realtime Options contains a filter, field, or expand from another collection. Rebuild the options from the selected collection.")));
    }
    return PinnedClient->DynamicSubscribe(
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
    if (!PinnedClient.IsValid() || PinnedClient->IsShutdown())
    {
        return FOpenPocketBaseSubscriptionResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before subscribing to a record.")));
    }
    if (!IsValid())
    {
        return FOpenPocketBaseSubscriptionResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Collection is missing, stale, or invalid. Choose it again from the current schema before subscribing.")));
    }
    if (!IsSafePathSegment(RecordId))
    {
        return FOpenPocketBaseSubscriptionResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Record ID is empty or contains an unsafe path character. Pass an ID returned by PocketBase.")));
    }
    if (Options.IsValid() && Reference.IsSet() && !Options.BelongsTo(Reference))
    {
        return FOpenPocketBaseSubscriptionResult::Failure(MakeLocalError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Realtime Options contains a filter, field, or expand from another collection. Rebuild the options from the selected collection.")));
    }
    return PinnedClient->DynamicSubscribe(
        Collection + TEXT("/") + MoveTemp(RecordId),
        MoveTemp(Callbacks),
        MoveTemp(Options));
}
