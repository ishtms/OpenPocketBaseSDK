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
        if (Header.Key.IsEmpty() || Header.Key.Contains(TEXT("\r")) || Header.Key.Contains(TEXT("\n")) ||
            Header.Value.Contains(TEXT("\r")) || Header.Value.Contains(TEXT("\n")))
        {
            OutError = MakeLocalError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("Default headers contain an invalid name or value."));
            return false;
        }

        if (IsHeaderName(Header.Key, TEXT("Authorization")) ||
            IsHeaderName(Header.Key, TEXT("Host")) ||
            IsHeaderName(Header.Key, TEXT("Content-Length")))
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

        const bool bExceededResponseLimit = Response.Body.Num() > Options.MaxResponseBytes;
        if (bExceededResponseLimit)
        {
            Response = FOpenPocketBaseHttpResponse();
            Response.RequestId = Request.RequestId;
            Response.ErrorMessage = TEXT("The response exceeded the configured byte limit.");
        }

        if (!bExceededResponseLimit && bEligibleRead && Options.bRetryEligibleReads &&
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
    bool bHasAuthRecord = false;

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

    void StoreAuth(FString Token, const FOpenPocketBaseRecord& Record)
    {
        FScopeLock Lock(&AuthMutex);
        AuthToken = MoveTemp(Token);
        AuthRecord = Record;
        bHasAuthRecord = true;
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
    FOpenPocketBaseRequestOptions Options) const
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
    if (!ValidateRequestOptions(Options, OptionsError))
    {
        DispatchFailure<FOpenPocketBaseRecord>(MoveTemp(OnComplete), MoveTemp(OptionsError));
        return {};
    }

    const TSharedRef<TCompletionState<FOpenPocketBaseRecord>, ESPMode::ThreadSafe> Completion =
        MakeShared<TCompletionState<FOpenPocketBaseRecord>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const FString Path = FString::Printf(
        TEXT("/api/collections/%s/records/%s"),
        *EncodeSegment(Collection),
        *EncodeSegment(RecordId));
    FOpenPocketBaseHttpRequest Request = PinnedClient->Impl->MakeRequest(
        TEXT("GET"), Path, {}, Options, true);

    return PinnedClient->Impl->Send(
        MoveTemp(Request),
        Options,
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
        [Completion, WeakClient](
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
                        ClientToUpdate->Impl->StoreAuth(MoveTemp(Token), Result.GetValue().Record);
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
