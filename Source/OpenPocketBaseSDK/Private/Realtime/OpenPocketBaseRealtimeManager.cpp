#include "Realtime/OpenPocketBaseRealtimeManager.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Math/RandomStream.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/OpenPocketBaseJson.h"

namespace
{
FOpenPocketBaseError MakeRealtimeError(
    const EOpenPocketBaseErrorKind Kind,
    const TCHAR* Message,
    const FString& RequestId = {})
{
    FOpenPocketBaseError Error;
    Error.Kind = Kind;
    Error.ServerMessage = Message;
    Error.RequestId = RequestId;
    Error.bMayRetry = Kind == EOpenPocketBaseErrorKind::Transport ||
        Kind == EOpenPocketBaseErrorKind::Timeout || Kind == EOpenPocketBaseErrorKind::Http;
    return Error;
}

FJsonObjectWrapper WrapRealtimeData(const TSharedRef<FJsonObject>& Object)
{
    FJsonObjectWrapper Wrapper;
    Wrapper.JsonObject = MakeShared<FJsonObject>();
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
    {
        if (Field.Key != TEXT("action") && Field.Key != TEXT("record"))
        {
            Wrapper.JsonObject->SetField(Field.Key, Field.Value);
        }
    }
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Wrapper.JsonString);
    FJsonSerializer::Serialize(Wrapper.JsonObject.ToSharedRef(), Writer);
    return Wrapper;
}

bool IsSameOrigin(const FString& BaseUrl, const FString& EffectiveUrl)
{
    if (EffectiveUrl == BaseUrl)
    {
        return true;
    }
    return EffectiveUrl.StartsWith(BaseUrl + TEXT("/"), ESearchCase::IgnoreCase);
}

bool IsValidName(const FString& Value)
{
    if (Value.IsEmpty())
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        const bool bPunctuation = Character == TEXT('!') || Character == TEXT('#') ||
            Character == TEXT('$') || Character == TEXT('%') || Character == TEXT('&') ||
            Character == TEXT('\'') || Character == TEXT('*') || Character == TEXT('+') ||
            Character == TEXT('-') || Character == TEXT('.') || Character == TEXT('^') ||
            Character == TEXT('_') || Character == TEXT('`') || Character == TEXT('|') ||
            Character == TEXT('~');
        if (!FChar::IsAlnum(Character) && !bPunctuation)
        {
            return false;
        }
    }
    return true;
}

bool IsProtectedHeader(const FString& Header)
{
    static const TCHAR* Protected[] = {
        TEXT("Authorization"), TEXT("Cookie"), TEXT("Host"), TEXT("Content-Length"),
        TEXT("Content-Type"), TEXT("Proxy-Authorization"), TEXT("Set-Cookie"),
        TEXT("X-Api-Key")
    };
    for (const TCHAR* Name : Protected)
    {
        if (Header.Equals(Name, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }
    return false;
}

bool IsSafeValue(const FString& Value, const int32 MaxLength)
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

FOpenPocketBaseError ErrorFromResponse(const FOpenPocketBaseHttpResponse& Response)
{
    if (Response.bTimedOut)
    {
        return MakeRealtimeError(
            EOpenPocketBaseErrorKind::Timeout,
            TEXT("The realtime connection timed out."),
            Response.RequestId);
    }
    if (!Response.bTransportSucceeded)
    {
        return MakeRealtimeError(
            EOpenPocketBaseErrorKind::Transport,
            TEXT("The realtime connection was interrupted."),
            Response.RequestId);
    }
    if (Response.HttpStatus >= 200 && Response.HttpStatus < 300)
    {
        return MakeRealtimeError(
            EOpenPocketBaseErrorKind::Transport,
            TEXT("The realtime stream closed and will reconnect."),
            Response.RequestId);
    }

    FOpenPocketBaseError Error = MakeRealtimeError(
        EOpenPocketBaseErrorKind::Http,
        TEXT("PocketBase rejected the realtime operation."),
        Response.RequestId);
    Error.HttpStatus = Response.HttpStatus;
    return Error;
}

EOpenPocketBaseRealtimeAction ParseAction(const FString& Action)
{
    if (Action == TEXT("create"))
    {
        return EOpenPocketBaseRealtimeAction::Create;
    }
    if (Action == TEXT("update"))
    {
        return EOpenPocketBaseRealtimeAction::Update;
    }
    if (Action == TEXT("delete"))
    {
        return EOpenPocketBaseRealtimeAction::Delete;
    }
    return EOpenPocketBaseRealtimeAction::Unknown;
}

bool HaveSameTopics(const TSet<FString>& First, const TSet<FString>& Second)
{
    if (First.Num() != Second.Num())
    {
        return false;
    }
    for (const FString& Topic : First)
    {
        if (!Second.Contains(Topic))
        {
            return false;
        }
    }
    return true;
}
}

FOpenPocketBaseSubscriptionHandle::FOpenPocketBaseSubscriptionHandle(
    TUniqueFunction<void()> InUnsubscribe,
    TUniqueFunction<bool()> InIsActive)
    : UnsubscribeAction(MoveTemp(InUnsubscribe))
    , IsActiveQuery(MoveTemp(InIsActive))
{
}

FOpenPocketBaseSubscriptionHandle::~FOpenPocketBaseSubscriptionHandle()
{
    Unsubscribe();
}

FOpenPocketBaseSubscriptionHandle::FOpenPocketBaseSubscriptionHandle(
    FOpenPocketBaseSubscriptionHandle&& Other) noexcept
    : UnsubscribeAction(MoveTemp(Other.UnsubscribeAction))
    , IsActiveQuery(MoveTemp(Other.IsActiveQuery))
{
}

FOpenPocketBaseSubscriptionHandle& FOpenPocketBaseSubscriptionHandle::operator=(
    FOpenPocketBaseSubscriptionHandle&& Other) noexcept
{
    if (this != &Other)
    {
        Unsubscribe();
        UnsubscribeAction = MoveTemp(Other.UnsubscribeAction);
        IsActiveQuery = MoveTemp(Other.IsActiveQuery);
    }
    return *this;
}

void FOpenPocketBaseSubscriptionHandle::Unsubscribe()
{
    if (UnsubscribeAction)
    {
        TUniqueFunction<void()> Action = MoveTemp(UnsubscribeAction);
        Action();
    }
    IsActiveQuery.Reset();
}

bool FOpenPocketBaseSubscriptionHandle::IsActive() const
{
    return IsActiveQuery && IsActiveQuery();
}

namespace OpenPocketBase::Realtime
{
FConnectionManager::FConnectionManager(
    FString InBaseUrl,
    TMap<FString, FString> InDefaultHeaders,
    FString InAcceptLanguage,
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> InTransport,
    TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> InClock,
    TFunction<FString()> InGetAuthToken,
    TFunction<bool()> InCanReconnect)
    : BaseUrl(MoveTemp(InBaseUrl))
    , DefaultHeaders(MoveTemp(InDefaultHeaders))
    , AcceptLanguage(MoveTemp(InAcceptLanguage))
    , Transport(MoveTemp(InTransport))
    , Clock(MoveTemp(InClock))
    , GetAuthToken(MoveTemp(InGetAuthToken))
    , CanReconnect(MoveTemp(InCanReconnect))
{
    BackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(
        this,
        &FConnectionManager::HandleApplicationBackgrounded);
    ForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(
        this,
        &FConnectionManager::HandleApplicationForegrounded);
    NetworkStatusHandle = FCoreDelegates::OnNetworkConnectionStatusChanged.AddRaw(
        this,
        &FConnectionManager::HandleNetworkStatusChanged);
}

FOpenPocketBaseSubscriptionHandle FConnectionManager::Subscribe(
    FString Topic,
    FOpenPocketBaseRealtimeCallbacks Callbacks,
    FOpenPocketBaseRealtimeOptions Options,
    FOpenPocketBaseError& OutError)
{
    FString WireTopic;
    if (!BuildWireTopic(Topic, Options, WireTopic, OutError))
    {
        return {};
    }

    uint64 ListenerId = 0;
    uint64 Generation = 0;
    bool bOpenConnection = false;
    bool bPostSubscriptions = false;
    FOpenPocketBaseRealtimeCallbacks CallbackCopy = Callbacks;
    {
        FScopeLock Lock(&Mutex);
        if (bShutdown || Listeners.Num() >= 256)
        {
            OutError = MakeRealtimeError(
                bShutdown ? EOpenPocketBaseErrorKind::Cancelled : EOpenPocketBaseErrorKind::InvalidArgument,
                bShutdown
                    ? TEXT("The client has shut down.")
                    : TEXT("The realtime listener limit was reached."));
            return {};
        }

        const bool bWireTopicWasDesired = GetDesiredSubscriptionsLocked().Contains(WireTopic);
        ListenerId = NextListenerId++;
        FListener Listener;
        Listener.Id = ListenerId;
        Listener.Topic = MoveTemp(Topic);
        Listener.WireTopic = MoveTemp(WireTopic);
        Listener.Callbacks = MoveTemp(Callbacks);
        Listeners.Add(ListenerId, MoveTemp(Listener));
        bStopped = false;
        Generation = ConnectionGeneration;
        bOpenConnection = ConnectionState == EOpenPocketBaseRealtimeConnectionState::Stopped;
        bPostSubscriptions = !bOpenConnection && !bWireTopicWasDesired &&
            ConnectionState == EOpenPocketBaseRealtimeConnectionState::Active;
    }

    QueueState(CallbackCopy, EOpenPocketBaseRealtimeConnectionState::Created);
    if (bOpenConnection)
    {
        OpenConnection();
    }
    else if (bPostSubscriptions)
    {
        PostSubscriptions(Generation);
    }
    else
    {
        EOpenPocketBaseRealtimeConnectionState State;
        {
            FScopeLock Lock(&Mutex);
            State = ConnectionState;
        }
        if (State != EOpenPocketBaseRealtimeConnectionState::Stopped)
        {
            QueueState(CallbackCopy, State);
        }
    }

    OutError = FOpenPocketBaseError();
    const TWeakPtr<FConnectionManager, ESPMode::ThreadSafe> WeakManager = AsShared();
    return FOpenPocketBaseSubscriptionHandle(
        [WeakManager, ListenerId]()
        {
            if (const TSharedPtr<FConnectionManager, ESPMode::ThreadSafe> Manager = WeakManager.Pin())
            {
                Manager->UnsubscribeListener(ListenerId);
            }
        },
        [WeakManager, ListenerId]()
        {
            const TSharedPtr<FConnectionManager, ESPMode::ThreadSafe> Manager = WeakManager.Pin();
            return Manager.IsValid() && Manager->IsListenerActive(ListenerId);
        });
}

bool FConnectionManager::IsListenerActive(const uint64 ListenerId) const
{
    FScopeLock Lock(&Mutex);
    return !bShutdown && Listeners.Contains(ListenerId);
}

void FConnectionManager::UnsubscribeListener(const uint64 ListenerId)
{
    FOpenPocketBaseRealtimeCallbacks RemovedCallbacks;
    uint64 Generation = 0;
    bool bStop = false;
    bool bPost = false;
    {
        FScopeLock Lock(&Mutex);
        FListener* Listener = Listeners.Find(ListenerId);
        if (Listener == nullptr)
        {
            return;
        }
        const FString RemovedWireTopic = Listener->WireTopic;
        RemovedCallbacks = Listener->Callbacks;
        Listeners.Remove(ListenerId);
        const TSet<FString> Desired = GetDesiredSubscriptionsLocked();
        bStop = Desired.IsEmpty();
        bPost = !bStop && !Desired.Contains(RemovedWireTopic) &&
            ConnectionState == EOpenPocketBaseRealtimeConnectionState::Active;
        Generation = ConnectionGeneration;
    }

    QueueState(RemovedCallbacks, EOpenPocketBaseRealtimeConnectionState::Stopped);
    if (bStop)
    {
        StopConnection(false);
    }
    else if (bPost)
    {
        PostSubscriptions(Generation);
    }
}

void FConnectionManager::UnsubscribeTopic(const FString& Topic)
{
    TArray<uint64> ListenerIds;
    {
        FScopeLock Lock(&Mutex);
        for (const TPair<uint64, FListener>& Pair : Listeners)
        {
            if (Pair.Value.Topic == Topic)
            {
                ListenerIds.Add(Pair.Key);
            }
        }
    }
    for (const uint64 ListenerId : ListenerIds)
    {
        UnsubscribeListener(ListenerId);
    }
}

void FConnectionManager::UnsubscribeAll()
{
    TArray<uint64> ListenerIds;
    {
        FScopeLock Lock(&Mutex);
        Listeners.GenerateKeyArray(ListenerIds);
    }
    for (const uint64 ListenerId : ListenerIds)
    {
        UnsubscribeListener(ListenerId);
    }
}

bool FConnectionManager::TryGetActiveClientId(FString& OutClientId) const
{
    FScopeLock Lock(&Mutex);
    if (bShutdown || ConnectionState != EOpenPocketBaseRealtimeConnectionState::Active ||
        ClientId.IsEmpty())
    {
        OutClientId.Reset();
        return false;
    }
    OutClientId = ClientId;
    return true;
}

void FConnectionManager::NotifyAuthChanged()
{
    bool bHasListeners = false;
    {
        FScopeLock Lock(&Mutex);
        bHasListeners = !Listeners.IsEmpty() && !bShutdown;
    }
    if (bHasListeners)
    {
        BeginReconnect(
            MakeRealtimeError(
                EOpenPocketBaseErrorKind::Authentication,
                TEXT("The realtime connection is restarting for the changed session.")),
            true);
    }
}

void FConnectionManager::Shutdown()
{
    TArray<FOpenPocketBaseRealtimeCallbacks> CallbackCopies;
    {
        FScopeLock Lock(&Mutex);
        if (bShutdown)
        {
            return;
        }
        bShutdown = true;
        bStopped = true;
        for (const TPair<uint64, FListener>& Pair : Listeners)
        {
            CallbackCopies.Add(Pair.Value.Callbacks);
        }
        Listeners.Reset();
    }
    FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(BackgroundHandle);
    FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ForegroundHandle);
    FCoreDelegates::OnNetworkConnectionStatusChanged.Remove(NetworkStatusHandle);
    BackgroundHandle.Reset();
    ForegroundHandle.Reset();
    NetworkStatusHandle.Reset();
    for (const FOpenPocketBaseRealtimeCallbacks& Callbacks : CallbackCopies)
    {
        QueueState(Callbacks, EOpenPocketBaseRealtimeConnectionState::Stopped);
    }
    StopConnection(false);
}

bool FConnectionManager::BuildWireTopic(
    const FString& Topic,
    const FOpenPocketBaseRealtimeOptions& Options,
    FString& OutWireTopic,
    FOpenPocketBaseError& OutError) const
{
    if (Topic.IsEmpty() || !IsSafeValue(Topic, 2048) || Options.QueryParameters.Num() > 32 ||
        Options.Headers.Num() > 32 || !Options.Filter.IsValid() ||
        !IsSafeValue(Options.Filter.ToString(), 4096))
    {
        OutError = MakeRealtimeError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("The realtime topic or options are invalid."));
        return false;
    }

    TMap<FString, FString> Query = Options.QueryParameters;
    if (!Options.Filter.IsEmpty())
    {
        Query.Add(TEXT("filter"), Options.Filter.ToString());
    }
    if (!Options.Expand.IsEmpty())
    {
        Query.Add(TEXT("expand"), FString::Join(Options.Expand, TEXT(",")));
    }
    if (!Options.Fields.IsEmpty())
    {
        Query.Add(TEXT("fields"), FString::Join(Options.Fields, TEXT(",")));
    }

    const TSharedRef<FJsonObject> QueryObject = MakeShared<FJsonObject>();
    for (const TPair<FString, FString>& Pair : Query)
    {
        if (!IsValidName(Pair.Key) || !IsSafeValue(Pair.Value, 4096))
        {
            OutError = MakeRealtimeError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A realtime query option is invalid."));
            return false;
        }
        QueryObject->SetStringField(Pair.Key, Pair.Value);
    }

    const TSharedRef<FJsonObject> HeaderObject = MakeShared<FJsonObject>();
    for (const TPair<FString, FString>& Pair : Options.Headers)
    {
        if (!IsValidName(Pair.Key) || IsProtectedHeader(Pair.Key) || !IsSafeValue(Pair.Value, 4096))
        {
            OutError = MakeRealtimeError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A realtime per-topic header is invalid or protected."));
            return false;
        }
        HeaderObject->SetStringField(Pair.Key, Pair.Value);
    }

    OutWireTopic = Topic;
    if (!Query.IsEmpty() || !Options.Headers.IsEmpty())
    {
        const TSharedRef<FJsonObject> OptionsObject = MakeShared<FJsonObject>();
        OptionsObject->SetObjectField(TEXT("query"), QueryObject);
        OptionsObject->SetObjectField(TEXT("headers"), HeaderObject);
        const TArray<uint8> JsonBytes = OpenPocketBase::Json::SerializeObject(OptionsObject);
        const FUTF8ToTCHAR Converted(
            reinterpret_cast<const ANSICHAR*>(JsonBytes.GetData()),
            JsonBytes.Num());
        const FString Json(Converted.Length(), Converted.Get());
        OutWireTopic += Topic.Contains(TEXT("?")) ? TEXT("&options=") : TEXT("?options=");
        OutWireTopic += FGenericPlatformHttp::UrlEncode(Json);
    }

    if (OutWireTopic.Len() > 8192)
    {
        OutError = MakeRealtimeError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("The serialized realtime topic exceeds the supported bound."));
        return false;
    }
    OutError = FOpenPocketBaseError();
    return true;
}

void FConnectionManager::OpenConnection()
{
    if (CanReconnect && !CanReconnect())
    {
        StopConnection(true);
        return;
    }

    uint64 Generation = 0;
    {
        FScopeLock Lock(&Mutex);
        if (bShutdown || bStopped || Listeners.IsEmpty() ||
            bBackgrounded || bNetworkUnavailable ||
            ConnectionState != EOpenPocketBaseRealtimeConnectionState::Stopped &&
            ConnectionState != EOpenPocketBaseRealtimeConnectionState::Reconnecting)
        {
            return;
        }
        Generation = ++ConnectionGeneration;
        ClientId.Reset();
        AcknowledgedSubscriptions.Reset();
        Parser = MakeUnique<FSseParser>();
        ConnectionState = EOpenPocketBaseRealtimeConnectionState::Subscribing;
    }
    SetAllListenerStates(EOpenPocketBaseRealtimeConnectionState::Subscribing);

    FOpenPocketBaseHttpRequest Request = MakeRequest(TEXT("GET"));
    Request.bStreamResponse = true;
    Request.TotalTimeoutSeconds = 0;
    Request.ActivityTimeoutSeconds = 30;
    const TSharedRef<FConnectionManager, ESPMode::ThreadSafe> Self = AsShared();
    FOpenPocketBaseTransportHandle NewStreamHandle = Transport->Send(
        MoveTemp(Request),
        [Self, Generation](const TArrayView<const uint8> Chunk)
        {
            Self->HandleChunk(Generation, Chunk);
        },
        [Self, Generation](FOpenPocketBaseHttpResponse&& Response)
        {
            Self->HandleStreamComplete(Generation, MoveTemp(Response));
        });

    FOpenPocketBaseClockHandle NewTimeoutHandle = Clock->Schedule(
        15.0,
        [Self, Generation]()
        {
            Self->HandleConnectTimeout(Generation);
        });

    bool bKeepHandles = false;
    {
        FScopeLock Lock(&Mutex);
        bKeepHandles = Generation == ConnectionGeneration && !bShutdown &&
            ConnectionState == EOpenPocketBaseRealtimeConnectionState::Subscribing;
        if (bKeepHandles)
        {
            StreamHandle = MoveTemp(NewStreamHandle);
            ConnectTimeoutHandle = MoveTemp(NewTimeoutHandle);
        }
    }
    if (!bKeepHandles)
    {
        NewStreamHandle.Cancel();
        NewTimeoutHandle.Cancel();
    }
}

void FConnectionManager::HandleChunk(
    const uint64 Generation,
    const TArrayView<const uint8> Chunk)
{
    TArray<FSseEvent> Events;
    FOpenPocketBaseError Error;
    bool bAccepted = false;
    {
        FScopeLock Lock(&Mutex);
        bAccepted = Generation == ConnectionGeneration && Parser.IsValid() && !bShutdown;
        if (bAccepted && !Parser->Feed(Chunk, Events, Error))
        {
            Parser.Reset();
        }
    }
    if (!bAccepted)
    {
        return;
    }
    if (Error.IsSet())
    {
        BeginReconnect(MoveTemp(Error), true);
        return;
    }
    for (const FSseEvent& Event : Events)
    {
        HandleSseEvent(Generation, Event);
    }
}

void FConnectionManager::HandleStreamComplete(
    const uint64 Generation,
    FOpenPocketBaseHttpResponse&& Response)
{
    FOpenPocketBaseError ParserError;
    bool bCurrent = false;
    {
        FScopeLock Lock(&Mutex);
        bCurrent = Generation == ConnectionGeneration && !bShutdown;
        if (bCurrent && Parser.IsValid())
        {
            Parser->Finish(ParserError);
            Parser.Reset();
        }
    }
    if (!bCurrent)
    {
        return;
    }

    if (!ParserError.IsSet() && !Response.EffectiveUrl.IsEmpty() &&
        !IsSameOrigin(BaseUrl, Response.EffectiveUrl))
    {
        ParserError = MakeRealtimeError(
            EOpenPocketBaseErrorKind::Transport,
            TEXT("The realtime stream used a disallowed redirect origin."),
            Response.RequestId);
    }
    BeginReconnect(
        ParserError.IsSet() ? MoveTemp(ParserError) : ErrorFromResponse(Response),
        true);
}

void FConnectionManager::HandleSseEvent(const uint64 Generation, const FSseEvent& Event)
{
    {
        FScopeLock Lock(&Mutex);
        if (Generation != ConnectionGeneration || bShutdown || bStopped)
        {
            return;
        }
    }

    if (Event.RetryMilliseconds.IsSet())
    {
        FScopeLock Lock(&Mutex);
        if (Generation == ConnectionGeneration)
        {
            ServerRetrySeconds = FMath::Clamp(
                Event.RetryMilliseconds.GetValue() / 1000.0,
                0.5,
                30.0);
        }
    }

    if (Event.Event == TEXT("PB_CONNECT"))
    {
        FString NewClientId = Event.Id;
        if (NewClientId.IsEmpty() && !Event.Data.IsEmpty())
        {
            TSharedPtr<FJsonObject> Object;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Event.Data);
            if (FJsonSerializer::Deserialize(Reader, Object) && Object.IsValid())
            {
                Object->TryGetStringField(TEXT("clientId"), NewClientId);
            }
        }
        if (NewClientId.IsEmpty())
        {
            BeginReconnect(
                MakeRealtimeError(
                    EOpenPocketBaseErrorKind::Serialization,
                    TEXT("PB_CONNECT did not include a client ID.")),
                true);
            return;
        }

        FOpenPocketBaseClockHandle TimeoutHandle;
        bool bCurrent = false;
        {
            FScopeLock Lock(&Mutex);
            bCurrent = Generation == ConnectionGeneration && !bShutdown;
            if (bCurrent)
            {
                ClientId = MoveTemp(NewClientId);
                AcknowledgedSubscriptions.Reset();
                ConnectionState = EOpenPocketBaseRealtimeConnectionState::Subscribing;
                TimeoutHandle = MoveTemp(ConnectTimeoutHandle);
            }
        }
        TimeoutHandle.Cancel();
        if (bCurrent)
        {
            SetAllListenerStates(EOpenPocketBaseRealtimeConnectionState::Subscribing);
            PostSubscriptions(Generation);
        }
        return;
    }

    FOpenPocketBaseRealtimeEvent RealtimeEvent;
    RealtimeEvent.Topic = Event.Event;
    TSharedPtr<FJsonObject> Object;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Event.Data);
    if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
    {
        QueueError(MakeRealtimeError(
            EOpenPocketBaseErrorKind::Serialization,
            TEXT("A realtime event contained malformed JSON.")));
        QueueResyncRequired();
        return;
    }
    RealtimeEvent.Data = WrapRealtimeData(Object.ToSharedRef());
    Object->TryGetStringField(TEXT("action"), RealtimeEvent.ActionName);
    RealtimeEvent.Action = ParseAction(RealtimeEvent.ActionName);
    const TSharedPtr<FJsonObject>* RecordObject = nullptr;
    if (Object->TryGetObjectField(TEXT("record"), RecordObject) && RecordObject != nullptr &&
        RecordObject->IsValid())
    {
        RealtimeEvent.bHasRecord = OpenPocketBase::Json::TryParseRecordObject(
            RecordObject->ToSharedRef(),
            RealtimeEvent.Record);
        if (!RealtimeEvent.bHasRecord)
        {
            QueueError(MakeRealtimeError(
                EOpenPocketBaseErrorKind::Serialization,
                TEXT("A realtime record event contained an invalid record.")));
            QueueResyncRequired();
            return;
        }
    }
    QueueEvent(Event.Event, MoveTemp(RealtimeEvent));
}

void FConnectionManager::PostSubscriptions(const uint64 Generation)
{
    TSet<FString> Desired;
    FString PostingClientId;
    {
        FScopeLock Lock(&Mutex);
        if (Generation != ConnectionGeneration || ClientId.IsEmpty() || bPostInFlight ||
            bShutdown || bStopped || Listeners.IsEmpty())
        {
            return;
        }
        Desired = GetDesiredSubscriptionsLocked();
        if (HaveSameTopics(Desired, AcknowledgedSubscriptions))
        {
            return;
        }
        PostingClientId = ClientId;
        bPostInFlight = true;
    }

    TArray<FString> SortedSubscriptions = Desired.Array();
    SortedSubscriptions.Sort();
    TArray<TSharedPtr<FJsonValue>> SubscriptionValues;
    SubscriptionValues.Reserve(SortedSubscriptions.Num());
    for (const FString& Subscription : SortedSubscriptions)
    {
        SubscriptionValues.Add(MakeShared<FJsonValueString>(Subscription));
    }
    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("clientId"), PostingClientId);
    Body->SetArrayField(TEXT("subscriptions"), MoveTemp(SubscriptionValues));

    FOpenPocketBaseHttpRequest Request = MakeRequest(TEXT("POST"));
    Request.Body = OpenPocketBase::Json::SerializeObject(Body);
    Request.BodyLength = Request.Body.Num();
    Request.Headers.Add(TEXT("Content-Type"), TEXT("application/json"));
    const FString AuthToken = GetAuthToken ? GetAuthToken() : FString();
    if (!AuthToken.IsEmpty())
    {
        Request.Headers.Add(TEXT("Authorization"), AuthToken);
    }

    const TSharedRef<FConnectionManager, ESPMode::ThreadSafe> Self = AsShared();
    FOpenPocketBaseTransportHandle NewPostHandle = Transport->Send(
        MoveTemp(Request),
        {},
        [Self, Generation, PostingClientId, Desired](FOpenPocketBaseHttpResponse&& Response) mutable
        {
            Self->HandlePostComplete(
                Generation,
                MoveTemp(PostingClientId),
                MoveTemp(Desired),
                MoveTemp(Response));
        });

    bool bKeepHandle = false;
    {
        FScopeLock Lock(&Mutex);
        bKeepHandle = Generation == ConnectionGeneration && ClientId == PostingClientId &&
            bPostInFlight && !bShutdown;
        if (bKeepHandle)
        {
            PostHandle = MoveTemp(NewPostHandle);
        }
    }
    if (!bKeepHandle)
    {
        NewPostHandle.Cancel();
    }
}

void FConnectionManager::HandlePostComplete(
    const uint64 Generation,
    FString PostingClientId,
    TSet<FString> PostedSubscriptions,
    FOpenPocketBaseHttpResponse&& Response)
{
    bool bCurrent = false;
    {
        FScopeLock Lock(&Mutex);
        bCurrent = Generation == ConnectionGeneration && ClientId == PostingClientId &&
            bPostInFlight && !bShutdown;
        if (bCurrent)
        {
            bPostInFlight = false;
            PostHandle = FOpenPocketBaseTransportHandle();
        }
    }
    if (!bCurrent)
    {
        return;
    }

    const bool bSameOrigin = Response.EffectiveUrl.IsEmpty() ||
        IsSameOrigin(BaseUrl, Response.EffectiveUrl);
    const bool bSucceeded = Response.bTransportSucceeded && bSameOrigin &&
        Response.HttpStatus >= 200 && Response.HttpStatus < 300;
    if (!bSucceeded)
    {
        FOpenPocketBaseError Error = bSameOrigin
            ? ErrorFromResponse(Response)
            : MakeRealtimeError(
                EOpenPocketBaseErrorKind::Transport,
                TEXT("The realtime subscription post used a disallowed redirect origin."),
                Response.RequestId);
        BeginReconnect(MoveTemp(Error), true);
        return;
    }

    bool bActive = false;
    bool bPostLatest = false;
    bool bNeedsStableReset = false;
    {
        FScopeLock Lock(&Mutex);
        if (Generation != ConnectionGeneration || ClientId != PostingClientId)
        {
            return;
        }
        AcknowledgedSubscriptions = MoveTemp(PostedSubscriptions);
        const TSet<FString> Desired = GetDesiredSubscriptionsLocked();
        bPostLatest = !HaveSameTopics(Desired, AcknowledgedSubscriptions);
        bActive = !bPostLatest;
        if (bActive)
        {
            ConnectionState = EOpenPocketBaseRealtimeConnectionState::Active;
            bNeedsStableReset = ReconnectAttempt > 0 || ServerRetrySeconds.IsSet();
        }
    }
    if (bActive)
    {
        SetAllListenerStates(EOpenPocketBaseRealtimeConnectionState::Active);
        if (bNeedsStableReset)
        {
            const TSharedRef<FConnectionManager, ESPMode::ThreadSafe> Self = AsShared();
            FOpenPocketBaseClockHandle NewStableHandle = Clock->Schedule(
                10.0,
                [Self, Generation]()
                {
                    Self->HandleStableConnection(Generation);
                });
            bool bKeepStableHandle = false;
            {
                FScopeLock Lock(&Mutex);
                bKeepStableHandle = Generation == ConnectionGeneration && !bShutdown &&
                    ConnectionState == EOpenPocketBaseRealtimeConnectionState::Active &&
                    (ReconnectAttempt > 0 || ServerRetrySeconds.IsSet());
                if (bKeepStableHandle)
                {
                    StableConnectionHandle = MoveTemp(NewStableHandle);
                }
            }
            if (!bKeepStableHandle)
            {
                NewStableHandle.Cancel();
            }
        }
    }
    else if (bPostLatest)
    {
        PostSubscriptions(Generation);
    }
}

void FConnectionManager::BeginReconnect(FOpenPocketBaseError Error, const bool bReportGap)
{
    const bool bOwnerAllowsReconnect = !CanReconnect || CanReconnect();
    FOpenPocketBaseTransportHandle OldStream;
    FOpenPocketBaseTransportHandle OldPost;
    FOpenPocketBaseClockHandle OldTimeout;
    FOpenPocketBaseClockHandle OldStable;
    bool bKeepPending = false;
    bool bSchedule = false;
    {
        FScopeLock Lock(&Mutex);
        if (bShutdown)
        {
            return;
        }
        ++ConnectionGeneration;
        OldStream = MoveTemp(StreamHandle);
        OldPost = MoveTemp(PostHandle);
        OldTimeout = MoveTemp(ConnectTimeoutHandle);
        OldStable = MoveTemp(StableConnectionHandle);
        Parser.Reset();
        ClientId.Reset();
        AcknowledgedSubscriptions.Reset();
        bPostInFlight = false;
        bKeepPending = !Listeners.IsEmpty() && !bStopped && bOwnerAllowsReconnect;
        bSchedule = bKeepPending && !bBackgrounded && !bNetworkUnavailable;
        ConnectionState = bKeepPending
            ? EOpenPocketBaseRealtimeConnectionState::Reconnecting
            : EOpenPocketBaseRealtimeConnectionState::Stopped;
    }
    OldStream.Cancel();
    OldPost.Cancel();
    OldTimeout.Cancel();
    OldStable.Cancel();

    QueueError(Error);
    if (bReportGap)
    {
        QueueResyncRequired();
    }
    SetAllListenerStates(bKeepPending
        ? EOpenPocketBaseRealtimeConnectionState::Reconnecting
        : EOpenPocketBaseRealtimeConnectionState::Stopped);
    if (bSchedule)
    {
        ScheduleReconnect();
    }
}

void FConnectionManager::ScheduleReconnect()
{
    uint64 TimerGeneration = 0;
    double Delay = 0;
    {
        FScopeLock Lock(&Mutex);
        if (bShutdown || bStopped || bBackgrounded || bNetworkUnavailable ||
            Listeners.IsEmpty() ||
            ConnectionState != EOpenPocketBaseRealtimeConnectionState::Reconnecting)
        {
            return;
        }
        TimerGeneration = ConnectionGeneration;
        const double Exponential = FMath::Min(30.0, 0.5 * FMath::Pow(2.0, ReconnectAttempt));
        FRandomStream Random(GetTypeHash(BaseUrl) + ReconnectAttempt + TimerGeneration);
        const double Jitter = Exponential * 0.2;
        const double JitteredExponential = FMath::Clamp(
            Exponential + Random.FRandRange(-Jitter, Jitter),
            0.0,
            30.0);
        Delay = FMath::Clamp(
            FMath::Max(JitteredExponential, ServerRetrySeconds.Get(0.0)),
            0.0,
            30.0);
        ++ReconnectAttempt;
    }

    const TSharedRef<FConnectionManager, ESPMode::ThreadSafe> Self = AsShared();
    FOpenPocketBaseClockHandle NewHandle = Clock->Schedule(
        Delay,
        [Self, TimerGeneration]()
        {
            Self->HandleReconnectTimer(TimerGeneration);
        });
    bool bKeep = false;
    {
        FScopeLock Lock(&Mutex);
        bKeep = TimerGeneration == ConnectionGeneration && !bShutdown &&
            ConnectionState == EOpenPocketBaseRealtimeConnectionState::Reconnecting;
        if (bKeep)
        {
            ReconnectHandle = MoveTemp(NewHandle);
        }
    }
    if (!bKeep)
    {
        NewHandle.Cancel();
    }
}

void FConnectionManager::HandleReconnectTimer(const uint64 TimerGeneration)
{
    {
        FScopeLock Lock(&Mutex);
        if (TimerGeneration != ConnectionGeneration || bShutdown || bStopped ||
            Listeners.IsEmpty() ||
            ConnectionState != EOpenPocketBaseRealtimeConnectionState::Reconnecting)
        {
            return;
        }
        ReconnectHandle = FOpenPocketBaseClockHandle();
    }
    OpenConnection();
}

void FConnectionManager::HandleStableConnection(const uint64 StableGeneration)
{
    FScopeLock Lock(&Mutex);
    if (StableGeneration == ConnectionGeneration && !bShutdown &&
        ConnectionState == EOpenPocketBaseRealtimeConnectionState::Active)
    {
        ReconnectAttempt = 0;
        ServerRetrySeconds.Reset();
        StableConnectionHandle = FOpenPocketBaseClockHandle();
    }
}

void FConnectionManager::HandleConnectTimeout(const uint64 TimedGeneration)
{
    bool bTimedOut = false;
    {
        FScopeLock Lock(&Mutex);
        bTimedOut = TimedGeneration == ConnectionGeneration && ClientId.IsEmpty() &&
            !bShutdown && !bStopped;
    }
    if (bTimedOut)
    {
        BeginReconnect(
            MakeRealtimeError(
                EOpenPocketBaseErrorKind::Timeout,
                TEXT("PocketBase did not send PB_CONNECT before the connection deadline.")),
            true);
    }
}

void FConnectionManager::HandleApplicationBackgrounded()
{
    bool bPause = false;
    {
        FScopeLock Lock(&Mutex);
        const bool bWasPaused = bBackgrounded || bNetworkUnavailable;
        bBackgrounded = true;
        bPause = !bWasPaused && !bShutdown;
    }
    if (bPause)
    {
        PauseForLifecycleOrNetwork();
    }
}

void FConnectionManager::HandleApplicationForegrounded()
{
    bool bResume = false;
    {
        FScopeLock Lock(&Mutex);
        const bool bWasPaused = bBackgrounded || bNetworkUnavailable;
        bBackgrounded = false;
        bResume = bWasPaused && !bNetworkUnavailable && !bShutdown;
    }
    if (bResume)
    {
        ResumeAfterLifecycleOrNetwork();
    }
}

void FConnectionManager::HandleNetworkStatusChanged(
    const ENetworkConnectionStatus PreviousStatus,
    const ENetworkConnectionStatus NewStatus)
{
    bool bPause = false;
    bool bResume = false;
    {
        FScopeLock Lock(&Mutex);
        const bool bWasPaused = bBackgrounded || bNetworkUnavailable;
        bNetworkUnavailable = NewStatus == ENetworkConnectionStatus::Disabled;
        const bool bIsPaused = bBackgrounded || bNetworkUnavailable;
        bPause = !bWasPaused && bIsPaused && !bShutdown;
        bResume = bWasPaused && !bIsPaused && !bShutdown;
    }
    if (bPause)
    {
        PauseForLifecycleOrNetwork();
    }
    else if (bResume)
    {
        ResumeAfterLifecycleOrNetwork();
    }
}

void FConnectionManager::PauseForLifecycleOrNetwork()
{
    FOpenPocketBaseTransportHandle OldStream;
    FOpenPocketBaseTransportHandle OldPost;
    FOpenPocketBaseClockHandle OldReconnect;
    FOpenPocketBaseClockHandle OldTimeout;
    FOpenPocketBaseClockHandle OldStable;
    bool bHasListeners = false;
    {
        FScopeLock Lock(&Mutex);
        if (bShutdown)
        {
            return;
        }
        ++ConnectionGeneration;
        OldStream = MoveTemp(StreamHandle);
        OldPost = MoveTemp(PostHandle);
        OldReconnect = MoveTemp(ReconnectHandle);
        OldTimeout = MoveTemp(ConnectTimeoutHandle);
        OldStable = MoveTemp(StableConnectionHandle);
        Parser.Reset();
        ClientId.Reset();
        AcknowledgedSubscriptions.Reset();
        bPostInFlight = false;
        bHasListeners = !Listeners.IsEmpty() && !bStopped;
        ConnectionState = bHasListeners
            ? EOpenPocketBaseRealtimeConnectionState::Reconnecting
            : EOpenPocketBaseRealtimeConnectionState::Stopped;
    }
    OldStream.Cancel();
    OldPost.Cancel();
    OldReconnect.Cancel();
    OldTimeout.Cancel();
    OldStable.Cancel();
    if (bHasListeners)
    {
        QueueResyncRequired();
        SetAllListenerStates(EOpenPocketBaseRealtimeConnectionState::Reconnecting);
    }
}

void FConnectionManager::ResumeAfterLifecycleOrNetwork()
{
    bool bOpen = false;
    {
        FScopeLock Lock(&Mutex);
        bOpen = !bShutdown && !bStopped && !bBackgrounded && !bNetworkUnavailable &&
            !Listeners.IsEmpty() &&
            (ConnectionState == EOpenPocketBaseRealtimeConnectionState::Reconnecting ||
                ConnectionState == EOpenPocketBaseRealtimeConnectionState::Stopped);
    }
    if (bOpen)
    {
        OpenConnection();
    }
}

void FConnectionManager::StopConnection(const bool bNotifyStopped)
{
    FOpenPocketBaseTransportHandle OldStream;
    FOpenPocketBaseTransportHandle OldPost;
    FOpenPocketBaseClockHandle OldReconnect;
    FOpenPocketBaseClockHandle OldTimeout;
    FOpenPocketBaseClockHandle OldStable;
    {
        FScopeLock Lock(&Mutex);
        ++ConnectionGeneration;
        OldStream = MoveTemp(StreamHandle);
        OldPost = MoveTemp(PostHandle);
        OldReconnect = MoveTemp(ReconnectHandle);
        OldTimeout = MoveTemp(ConnectTimeoutHandle);
        OldStable = MoveTemp(StableConnectionHandle);
        Parser.Reset();
        ClientId.Reset();
        AcknowledgedSubscriptions.Reset();
        bPostInFlight = false;
        ReconnectAttempt = 0;
        ServerRetrySeconds.Reset();
        ConnectionState = EOpenPocketBaseRealtimeConnectionState::Stopped;
    }
    OldStream.Cancel();
    OldPost.Cancel();
    OldReconnect.Cancel();
    OldTimeout.Cancel();
    OldStable.Cancel();
    if (bNotifyStopped)
    {
        SetAllListenerStates(EOpenPocketBaseRealtimeConnectionState::Stopped);
    }
}

void FConnectionManager::SetAllListenerStates(
    const EOpenPocketBaseRealtimeConnectionState NewState)
{
    TArray<FOpenPocketBaseRealtimeCallbacks> CallbackCopies;
    {
        FScopeLock Lock(&Mutex);
        for (TPair<uint64, FListener>& Pair : Listeners)
        {
            if (Pair.Value.State != NewState)
            {
                Pair.Value.State = NewState;
                CallbackCopies.Add(Pair.Value.Callbacks);
            }
        }
    }
    for (const FOpenPocketBaseRealtimeCallbacks& Callbacks : CallbackCopies)
    {
        QueueState(Callbacks, NewState);
    }
}

void FConnectionManager::QueueState(
    const FOpenPocketBaseRealtimeCallbacks& Callbacks,
    const EOpenPocketBaseRealtimeConnectionState State)
{
    if (Callbacks.OnConnectionStateChanged)
    {
        EnqueueCallback([Callback = Callbacks.OnConnectionStateChanged, State]()
        {
            Callback(State);
        });
    }
}

void FConnectionManager::QueueError(const FOpenPocketBaseError& Error)
{
    TArray<TFunction<void(const FOpenPocketBaseError&)>> Callbacks;
    {
        FScopeLock Lock(&Mutex);
        for (const TPair<uint64, FListener>& Pair : Listeners)
        {
            if (Pair.Value.Callbacks.OnError)
            {
                Callbacks.Add(Pair.Value.Callbacks.OnError);
            }
        }
    }
    for (const TFunction<void(const FOpenPocketBaseError&)>& Callback : Callbacks)
    {
        EnqueueCallback([Callback, Error]()
        {
            Callback(Error);
        });
    }
}

void FConnectionManager::QueueResyncRequired()
{
    TArray<TFunction<void()>> Callbacks;
    {
        FScopeLock Lock(&Mutex);
        for (const TPair<uint64, FListener>& Pair : Listeners)
        {
            if (Pair.Value.Callbacks.OnResyncRequired)
            {
                Callbacks.Add(Pair.Value.Callbacks.OnResyncRequired);
            }
        }
    }
    for (const TFunction<void()>& Callback : Callbacks)
    {
        EnqueueCallback(Callback);
    }
}

void FConnectionManager::QueueEvent(
    const FString& WireTopic,
    FOpenPocketBaseRealtimeEvent Event)
{
    struct FTarget
    {
        uint64 ListenerId = 0;
        TFunction<void(const FOpenPocketBaseRealtimeEvent&)> Callback;
    };
    TArray<FTarget> Targets;
    {
        FScopeLock Lock(&Mutex);
        for (const TPair<uint64, FListener>& Pair : Listeners)
        {
            if (Pair.Value.WireTopic == WireTopic && Pair.Value.Callbacks.OnEvent)
            {
                Targets.Add({Pair.Key, Pair.Value.Callbacks.OnEvent});
            }
        }
    }
    const TWeakPtr<FConnectionManager, ESPMode::ThreadSafe> WeakManager = AsShared();
    for (const FTarget& Target : Targets)
    {
        EnqueueCallback([WeakManager, ListenerId = Target.ListenerId, Callback = Target.Callback, Event]()
        {
            const TSharedPtr<FConnectionManager, ESPMode::ThreadSafe> Manager = WeakManager.Pin();
            if (Manager.IsValid() && Manager->IsListenerActive(ListenerId))
            {
                Callback(Event);
            }
        }, true);
    }
}

void FConnectionManager::EnqueueCallback(TFunction<void()> Callback, const bool bRealtimeEvent)
{
    if (!Callback)
    {
        return;
    }
    bool bSchedule = false;
    {
        FScopeLock Lock(&QueueMutex);
        if (QueuedCallbacks.Num() >= MaxQueuedCallbacks)
        {
            bOverflowResyncPending = true;
            if (!bRealtimeEvent)
            {
                QueuedCallbacks.RemoveAt(0, 1, EAllowShrinking::No);
                QueuedCallbacks.Add({MoveTemp(Callback)});
            }
        }
        else
        {
            QueuedCallbacks.Add({MoveTemp(Callback)});
        }
        if (!bDrainScheduled)
        {
            bDrainScheduled = true;
            bSchedule = true;
        }
    }
    if (bSchedule)
    {
        ScheduleDrain();
    }
}

void FConnectionManager::ScheduleDrain()
{
    const TSharedRef<FConnectionManager, ESPMode::ThreadSafe> Self = AsShared();
    AsyncTask(ENamedThreads::GameThread, [Self]()
    {
        Self->DrainCallbacks();
    });
}

void FConnectionManager::DrainCallbacks()
{
    TArray<FQueuedCallback> LocalCallbacks;
    bool bMore = false;
    bool bReportOverflow = false;
    {
        FScopeLock Lock(&QueueMutex);
        const int32 Count = FMath::Min(MaxCallbacksPerDrain, QueuedCallbacks.Num());
        LocalCallbacks.Append(QueuedCallbacks.GetData(), Count);
        QueuedCallbacks.RemoveAt(0, Count, EAllowShrinking::No);
        bReportOverflow = bOverflowResyncPending;
        bOverflowResyncPending = false;
        bMore = !QueuedCallbacks.IsEmpty();
        if (!bMore)
        {
            bDrainScheduled = false;
        }
    }

    for (FQueuedCallback& Callback : LocalCallbacks)
    {
        if (Callback.Invoke)
        {
            Callback.Invoke();
        }
    }
    if (bReportOverflow)
    {
        TArray<TFunction<void()>> ResyncCallbacks;
        {
            FScopeLock Lock(&Mutex);
            for (const TPair<uint64, FListener>& Pair : Listeners)
            {
                if (Pair.Value.Callbacks.OnResyncRequired)
                {
                    ResyncCallbacks.Add(Pair.Value.Callbacks.OnResyncRequired);
                }
            }
        }
        for (const TFunction<void()>& Callback : ResyncCallbacks)
        {
            Callback();
        }
    }
    if (bMore)
    {
        ScheduleDrain();
    }
}

TSet<FString> FConnectionManager::GetDesiredSubscriptionsLocked() const
{
    TSet<FString> Desired;
    Desired.Reserve(Listeners.Num());
    for (const TPair<uint64, FListener>& Pair : Listeners)
    {
        Desired.Add(Pair.Value.WireTopic);
    }
    return Desired;
}

FOpenPocketBaseHttpRequest FConnectionManager::MakeRequest(const TCHAR* Method) const
{
    FOpenPocketBaseHttpRequest Request;
    Request.Method = Method;
    Request.Url = BaseUrl + TEXT("/api/realtime");
    Request.RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    Request.Headers = DefaultHeaders;
    Request.Headers.Add(TEXT("X-Request-Id"), Request.RequestId);
    Request.Headers.Add(
        TEXT("Accept"),
        Request.Method == TEXT("GET") ? TEXT("text/event-stream") : TEXT("application/json"));
    if (!AcceptLanguage.IsEmpty())
    {
        Request.Headers.Add(TEXT("Accept-Language"), AcceptLanguage);
    }
    return Request;
}
}
