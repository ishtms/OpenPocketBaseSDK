#include "OpenPocketBaseClient.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/CriticalSection.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "Request/OpenPocketBaseRequestState.h"
#include "Serialization/OpenPocketBaseJson.h"
#include "Transport/OpenPocketBaseHttpTransport.h"

#include <atomic>

namespace
{
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
}

struct FOpenPocketBaseClient::FImpl
{
    using FResponseHandler = TUniqueFunction<void(
        FOpenPocketBaseHttpResponse&&,
        const TSharedRef<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>&)>;

    FOpenPocketBaseClientConfig Config;
    FString BaseUrl;
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport;
    std::atomic<bool> bShutdown = false;
    std::atomic<uint64> NextRequestId = 1;
    mutable FCriticalSection RequestsMutex;
    TMap<uint64, TWeakPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>> Requests;
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
        FResponseHandler Handler,
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
            return FOpenPocketBaseRequestHandle(State);
        }

        State->MarkSending();
        FOpenPocketBaseTransportHandle TransportHandle = Transport->Send(
            MoveTemp(Request),
            FOpenPocketBaseHttpChunkCallback(),
            [State, Handler = MoveTemp(Handler)](FOpenPocketBaseHttpResponse&& Response) mutable
            {
                Handler(MoveTemp(Response), State);
            });
        State->AttachTransportHandle(MoveTemp(TransportHandle));
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
    return !Collection.IsEmpty() && Client.IsValid();
}

FOpenPocketBaseRequestHandle FOpenPocketBaseCollectionService::GetOne(
    FString RecordId,
    FOpenPocketBaseRecordCallback OnComplete,
    FOpenPocketBaseRequestOptions Options) const
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedClient = Client.Pin();
    if (!PinnedClient.IsValid() || Collection.IsEmpty() || RecordId.IsEmpty())
    {
        DispatchFailure<FOpenPocketBaseRecord>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument, TEXT("Client, collection, and record ID are required.")));
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
    if (!PinnedClient.IsValid() || Collection.IsEmpty() || Options.Page < 1 || Options.PerPage < 1)
    {
        DispatchFailure<FOpenPocketBaseRecordPage>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument, TEXT("Client, collection, page, and per-page values are required.")));
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
    if (!PinnedClient.IsValid() || Collection.IsEmpty() || Identity.IsEmpty() || Password.IsEmpty())
    {
        DispatchFailure<FOpenPocketBaseAuthResult>(
            MoveTemp(OnComplete),
            MakeLocalError(EOpenPocketBaseErrorKind::InvalidArgument, TEXT("Client, collection, identity, and password are required.")));
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
