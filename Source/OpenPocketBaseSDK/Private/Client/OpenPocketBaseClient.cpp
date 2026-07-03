#include "OpenPocketBaseClient.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Containers/Ticker.h"
#include "HAL/CriticalSection.h"
#include "Math/RandomStream.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "Request/OpenPocketBaseRequestState.h"
#include "Serialization/OpenPocketBaseJson.h"
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

FString MakeListQuery(const FOpenPocketBaseListOptions& Options)
{
    TArray<FString> Parts;
    Parts.Add(FString::Printf(TEXT("page=%d"), Options.Page));
    Parts.Add(FString::Printf(TEXT("perPage=%d"), Options.PerPage));
    AddQueryValue(Parts, TEXT("filter"), Options.Filter);
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

    if (!bRequestKeyValid || Options.TotalTimeoutSeconds < 0 || Options.ActivityTimeoutSeconds < 0 ||
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
        TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> InState,
        FOpenPocketBaseResponseHandler InHandler)
        : Request(MoveTemp(InRequest))
        , Options(InOptions)
        , bEligibleRead(bInEligibleRead)
        , Transport(InTransport)
        , State(MoveTemp(InState))
        , Handler(MoveTemp(InHandler))
    {
    }

    void Start()
    {
        if (!State->TryMarkSending())
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
        FOpenPocketBaseTransportHandle Handle = PinnedTransport->Send(
            MoveTemp(AttemptRequest),
            {},
            [Self, Generation](FOpenPocketBaseHttpResponse&& Response)
            {
                Self->HandleResponse(MoveTemp(Response), Generation);
            });
        State->AttachTransportHandle(MoveTemp(Handle));
    }

private:
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
        const FTSTicker::FDelegateHandle TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            TEXT("OpenPocketBase.ReadRetry"),
            static_cast<float>(Delay),
            [Self](float)
            {
                Self->Start();
                return false;
            });
        State->AttachRetryHandle(FOpenPocketBaseTransportHandle(
            [TickerHandle]()
            {
                FTSTicker::RemoveTicker(TickerHandle);
            }));
    }

    FOpenPocketBaseHttpRequest Request;
    FOpenPocketBaseRequestOptions Options;
    bool bEligibleRead = false;
    TWeakPtr<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport;
    TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> State;
    FOpenPocketBaseResponseHandler Handler;
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

    FOpenPocketBaseClientConfig Config;
    FString BaseUrl;
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport;
    std::atomic<bool> bShutdown = false;
    std::atomic<uint64> NextRequestId = 1;
    mutable FCriticalSection RequestsMutex;
    TMap<uint64, TWeakPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>> Requests;
    TMap<FString, TWeakPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>> RequestKeys;
    mutable FCriticalSection AuthMutex;
    FString AuthToken;
    FOpenPocketBaseRecord AuthRecord;
    FString AuthCollection;
    int64 AuthGeneration = 0;
    bool bHasAuthRecord = false;
    EOpenPocketBaseSessionPersistenceState PersistenceState =
        EOpenPocketBaseSessionPersistenceState::MemoryOnly;
    TSharedRef<FSessionEventQueue, ESPMode::ThreadSafe> SessionEvents =
        MakeShared<FSessionEventQueue, ESPMode::ThreadSafe>();
    mutable FCriticalSection RefreshMutex;
    TSharedPtr<FRefreshFlight, ESPMode::ThreadSafe> ActiveRefresh;

    FImpl(
        FOpenPocketBaseClientConfig InConfig,
        FString InBaseUrl,
        TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> InTransport)
        : Config(MoveTemp(InConfig))
        , BaseUrl(MoveTemp(InBaseUrl))
        , Transport(MoveTemp(InTransport))
    {
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
        TUniqueFunction<void()> OnCancelled)
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

        const TSharedRef<FOpenPocketBaseRequestAttempts, ESPMode::ThreadSafe> Attempts =
            MakeShared<FOpenPocketBaseRequestAttempts, ESPMode::ThreadSafe>(
                MoveTemp(Request),
                Options,
                bEligibleRead,
                Transport,
                State,
                MoveTemp(Handler));
        Attempts->Start();
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

    void StoreAuth(
        FString Token,
        const FOpenPocketBaseRecord& Record,
        FString Collection,
        const EOpenPocketBaseSessionChangeReason RequestedReason)
    {
        FOpenPocketBaseSessionSnapshot Snapshot;
        {
            FScopeLock Lock(&AuthMutex);
            const bool bUserSwitched = bHasAuthRecord &&
                (!AuthCollection.Equals(Collection, ESearchCase::CaseSensitive) ||
                    AuthRecord.Id != Record.Id);
            AuthToken = MoveTemp(Token);
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
    }

    void ClearAuth()
    {
        FOpenPocketBaseSessionSnapshot Snapshot;
        {
            FScopeLock Lock(&AuthMutex);
            AuthToken.Reset();
            AuthRecord = FOpenPocketBaseRecord();
            AuthCollection.Reset();
            bHasAuthRecord = false;
            ++AuthGeneration;

            Snapshot.AuthGeneration = AuthGeneration;
            Snapshot.PersistenceState = PersistenceState;
            Snapshot.Reason = EOpenPocketBaseSessionChangeReason::LoggedOut;
        }
        SessionEvents->Enqueue(MoveTemp(Snapshot));
    }

    bool TryStoreRefreshedAuth(
        const int64 CapturedGeneration,
        const FString& CapturedToken,
        const FString& Collection,
        FString NewToken,
        const FOpenPocketBaseRecord& Record)
    {
        FOpenPocketBaseSessionSnapshot Snapshot;
        {
            FScopeLock Lock(&AuthMutex);
            if (AuthGeneration != CapturedGeneration || AuthToken != CapturedToken ||
                AuthCollection != Collection || !bHasAuthRecord)
            {
                return false;
            }

            AuthToken = MoveTemp(NewToken);
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

namespace
{
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

TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> FOpenPocketBaseClient::Create(
    const FOpenPocketBaseClientConfig& Config,
    FOpenPocketBaseError& OutError)
{
    return Create(Config, CreateOpenPocketBaseHttpTransport(), OutError);
}

TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> FOpenPocketBaseClient::Create(
    const FOpenPocketBaseClientConfig& Config,
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
    FOpenPocketBaseError& OutError)
{
    FString BaseUrl;
    if (!Config.TryGetNormalizedBaseUrl(BaseUrl, OutError) || !ValidateDefaultHeaders(Config, OutError))
    {
        return nullptr;
    }

    OutError = FOpenPocketBaseError();
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        MakeShareable(new FOpenPocketBaseClient(Config, MoveTemp(BaseUrl), MoveTemp(Transport)));
    return Client;
}

FOpenPocketBaseClient::FOpenPocketBaseClient(
    FOpenPocketBaseClientConfig Config,
    FString NormalizedBaseUrl,
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport)
    : Impl(MakeUnique<FImpl>(MoveTemp(Config), MoveTemp(NormalizedBaseUrl), MoveTemp(Transport)))
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

FString FOpenPocketBaseClient::GetBaseUrl() const
{
    return Impl->BaseUrl;
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
                if (!Client.IsValid() || Client->IsShutdown() ||
                    !Client->Impl->TryStoreRefreshedAuth(
                        Flight->CapturedGeneration,
                        Flight->CapturedToken,
                        Flight->AuthCollection,
                        MoveTemp(Token),
                        Result.GetValue().Record))
                {
                    Result = TOpenPocketBaseResult<FOpenPocketBaseAuthResult>::Failure(
                        MakeLocalError(
                            EOpenPocketBaseErrorKind::Authentication,
                            TEXT("The session changed while Auth Refresh was in flight.")));
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
        });
    {
        FScopeLock Lock(&Impl->RefreshMutex);
        Flight->ChildHandle = MoveTemp(ChildHandle);
    }
    return Impl->MakeRequestHandle(RequestState);
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
    return Impl->Send(
        MoveTemp(Request),
        Options.RequestOptions,
        false,
        [Completion, Batch = MoveTemp(Batch)](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<FOpenPocketBaseBatchResult> Result =
                OpenPocketBase::Json::ParseBatchResponse(Response, Batch);
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
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) || Options.Page < 1 || Options.PerPage < 1)
    {
        DispatchFailure<FOpenPocketBaseRecordPage>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument, TEXT("Client, collection, page, and per-page values are required.")));
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
    FString Filter,
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

    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options,
        false,
        [Completion](
            FOpenPocketBaseHttpResponse&& Response,
            const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>& State)
        {
            TOpenPocketBaseResult<bool> Result = OpenPocketBase::Json::ParseEmptyResponse(Response);
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

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::AuthWithPassword(
    FString Identity,
    FString Password,
    FOpenPocketBaseAuthCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || !IsSafePathSegment(Collection) || Identity.IsEmpty() || Password.IsEmpty())
    {
        DispatchFailure<FOpenPocketBaseAuthResult>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument, TEXT("Client, collection, identity, and password are required.")));
        return {};
    }

    FOpenPocketBaseError OptionsError;
    if (!ValidateRequestOptions(Options, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseAuthResult>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseAuthResult>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseAuthResult>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("identity"), Identity);
    Body->SetStringField(TEXT("password"), Password);

    const FString Path = FString::Printf(
        TEXT("/api/collections/%s/auth-with-password"),
        *EncodeSegment(Collection));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("POST"),
        Path,
        OpenPocketBase::Json::SerializeObject(Body),
        Options,
        false);
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
            TOpenPocketBaseResult<FOpenPocketBaseAuthResult> Result =
                OpenPocketBase::Json::ParseAuthResponse(Response, Token);
            if (Result.IsSuccess())
            {
                if (const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> ClientToUpdate = WeakClient.Pin())
                {
                    if (!ClientToUpdate->IsShutdown())
                    {
                        ClientToUpdate->Impl->StoreAuth(
                            MoveTemp(Token),
                            Result.GetValue().Record,
                            AuthCollection,
                            EOpenPocketBaseSessionChangeReason::LoggedIn);
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
            Completion->Invoke(TOpenPocketBaseResult<FOpenPocketBaseAuthResult>::Failure(MakeCancelledError()));
        });
}
