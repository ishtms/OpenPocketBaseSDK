#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"
#include "Misc/CoreDelegates.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "SecureStorage/OpenPocketBaseSecureStore.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <atomic>

namespace
{
class FRealtimeHandshakeTransport final : public IOpenPocketBaseTransport
{
public:
    virtual bool IsIncrementalResponseStreamingAvailable(FString& OutReason) const override
    {
        OutReason = TEXT("The deterministic realtime transport supports streaming.");
        return true;
    }

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        Requests.Add(Request);
        if (Request.Method == TEXT("GET"))
        {
            StreamChunk = MoveTemp(OnChunk);
            StreamComplete = MoveTemp(OnComplete);
            return FOpenPocketBaseTransportHandle([]() {});
        }

        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = 204;
        Response.RequestId = Request.RequestId;
        Response.EffectiveUrl = Request.Url;
        OnComplete(MoveTemp(Response));
        return FOpenPocketBaseTransportHandle([]() {});
    }

    void EmitConnect()
    {
        static const ANSICHAR Payload[] = "id: client-one\nevent: PB_CONNECT\ndata: {}\n\n";
        StreamChunk(TArrayView<const uint8>(
            reinterpret_cast<const uint8*>(Payload),
            UE_ARRAY_COUNT(Payload) - 1));
    }

    TArray<FOpenPocketBaseHttpRequest> Requests;
    FOpenPocketBaseHttpChunkCallback StreamChunk;
    FOpenPocketBaseHttpCompleteCallback StreamComplete;
};

class FControlledRealtimeTransport final : public IOpenPocketBaseTransport
{
public:
    struct FPending
    {
        int32 RequestIndex = INDEX_NONE;
        FOpenPocketBaseHttpChunkCallback OnChunk;
        FOpenPocketBaseHttpCompleteCallback OnComplete;
        TSharedRef<std::atomic<bool>, ESPMode::ThreadSafe> Cancelled =
            MakeShared<std::atomic<bool>, ESPMode::ThreadSafe>(false);
    };

    virtual bool IsIncrementalResponseStreamingAvailable(FString& OutReason) const override
    {
        OutReason = TEXT("The controlled realtime transport supports streaming.");
        return true;
    }

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        const int32 RequestIndex = Requests.Add(Request);
        FPending Pending;
        Pending.RequestIndex = RequestIndex;
        Pending.OnChunk = MoveTemp(OnChunk);
        Pending.OnComplete = MoveTemp(OnComplete);
        const TSharedRef<std::atomic<bool>, ESPMode::ThreadSafe> Cancelled = Pending.Cancelled;
        if (Request.Method == TEXT("GET"))
        {
            Streams.Add(MoveTemp(Pending));
        }
        else
        {
            Posts.Add(MoveTemp(Pending));
        }
        return FOpenPocketBaseTransportHandle([Cancelled]()
        {
            Cancelled->store(true, std::memory_order_release);
        });
    }

    void EmitStream(const int32 StreamIndex, const FString& Payload)
    {
        FTCHARToUTF8 Utf8(*Payload);
        Streams[StreamIndex].OnChunk(TArrayView<const uint8>(
            reinterpret_cast<const uint8*>(Utf8.Get()),
            Utf8.Length()));
    }

    void EmitConnect(const int32 StreamIndex, const FString& ClientId)
    {
        EmitStream(
            StreamIndex,
            FString::Printf(TEXT("id: %s\nevent: PB_CONNECT\ndata: {}\n\n"), *ClientId));
    }

    void CompletePost(const int32 PostIndex, const int32 HttpStatus = 204)
    {
        FPending& Pending = Posts[PostIndex];
        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = HttpStatus;
        Response.RequestId = Requests[Pending.RequestIndex].RequestId;
        Response.EffectiveUrl = Requests[Pending.RequestIndex].Url;
        FOpenPocketBaseHttpCompleteCallback Completion = MoveTemp(Pending.OnComplete);
        Completion(MoveTemp(Response));
    }

    void CompleteStream(const int32 StreamIndex)
    {
        FPending& Pending = Streams[StreamIndex];
        FOpenPocketBaseHttpResponse Response;
        Response.RequestId = Requests[Pending.RequestIndex].RequestId;
        Response.EffectiveUrl = Requests[Pending.RequestIndex].Url;
        Response.ErrorMessage = TEXT("Synthetic disconnect.");
        FOpenPocketBaseHttpCompleteCallback Completion = MoveTemp(Pending.OnComplete);
        Completion(MoveTemp(Response));
    }

    TSet<FString> GetPostedSubscriptions(const int32 PostIndex) const
    {
        const FPending& Pending = Posts[PostIndex];
        const FOpenPocketBaseHttpRequest& Request = Requests[Pending.RequestIndex];
        const FUTF8ToTCHAR Converted(
            reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()),
            Request.Body.Num());
        const FString Json(Converted.Length(), Converted.Get());
        TSharedPtr<FJsonObject> Object;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
        {
            return {};
        }
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(TEXT("subscriptions"), Values) || Values == nullptr)
        {
            return {};
        }
        TSet<FString> Result;
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            Result.Add(Value->AsString());
        }
        return Result;
    }

    TArray<FOpenPocketBaseHttpRequest> Requests;
    TArray<FPending> Streams;
    TArray<FPending> Posts;
};

class FRealtimeTestClock final : public IOpenPocketBaseClock
{
public:
    struct FScheduled
    {
        double DelaySeconds = 0;
        TUniqueFunction<void()> Callback;
        TSharedRef<std::atomic<bool>, ESPMode::ThreadSafe> Active =
            MakeShared<std::atomic<bool>, ESPMode::ThreadSafe>(true);
    };

    virtual FDateTime UtcNow() const override
    {
        return FDateTime::FromUnixTimestamp(1000);
    }

    virtual double MonotonicSeconds() const override
    {
        return 1000;
    }

    virtual FOpenPocketBaseClockHandle Schedule(
        const double DelaySeconds,
        TUniqueFunction<void()> Callback) override
    {
        FScheduled Entry;
        Entry.DelaySeconds = DelaySeconds;
        Entry.Callback = MoveTemp(Callback);
        const TSharedRef<std::atomic<bool>, ESPMode::ThreadSafe> Active = Entry.Active;
        Scheduled.Add(MoveTemp(Entry));
        return FOpenPocketBaseClockHandle([Active]()
        {
            Active->store(false, std::memory_order_release);
        });
    }

    bool RunNextActive()
    {
        while (!Scheduled.IsEmpty())
        {
            FScheduled Entry = MoveTemp(Scheduled[0]);
            Scheduled.RemoveAt(0, EAllowShrinking::No);
            if (!Entry.Active->exchange(false, std::memory_order_acq_rel))
            {
                continue;
            }
            Entry.Callback();
            return true;
        }
        return false;
    }

    TOptional<double> GetNextActiveDelay() const
    {
        for (const FScheduled& Entry : Scheduled)
        {
            if (Entry.Active->load(std::memory_order_acquire))
            {
                return Entry.DelaySeconds;
            }
        }
        return {};
    }

    TArray<FScheduled> Scheduled;
};

struct FRealtimeDeliveryState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FControlledRealtimeTransport, ESPMode::ThreadSafe> Transport;
    FOpenPocketBaseSubscriptionHandle Handle;
    TArray<FString> Actions;
    int32 ResyncCount = 0;
};

class FVerifyRealtimeDelivery final : public IAutomationLatentCommand
{
public:
    FVerifyRealtimeDelivery(
        const TSharedRef<FRealtimeDeliveryState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest,
        const int32 InExpectedEvents)
        : State(InState)
        , Test(InTest)
        , ExpectedEvents(InExpectedEvents)
    {
    }

    virtual bool Update() override
    {
        if (State->Actions.Num() < ExpectedEvents || State->ResyncCount == 0)
        {
            return false;
        }
        Test->TestEqual(TEXT("The bounded queue preserves the expected event count"),
            State->Actions.Num(), ExpectedEvents);
        for (int32 Index = 0; Index < State->Actions.Num(); ++Index)
        {
            Test->TestEqual(
                TEXT("Realtime delivery preserves source order"),
                State->Actions[Index],
                FString::FromInt(Index));
        }
        Test->TestEqual(TEXT("Overflow reports one resynchronization gap"), State->ResyncCount, 1);
        State->Handle.Unsubscribe();
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FRealtimeDeliveryState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
    int32 ExpectedEvents = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRealtimeHandshakeTest,
    "OpenPocketBase.Realtime.Manager.HandshakesBeforePostingSubscriptions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRealtimeHandshakeTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::MemoryOnly;

    const TSharedRef<FRealtimeHandshakeTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FRealtimeHandshakeTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseError Error;
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        CreateOpenPocketBaseTestClient(Config, Transport, Error);
    TestTrue(TEXT("The client is created"), Client.IsValid());

    FOpenPocketBaseRealtimeCallbacks Callbacks;
    FOpenPocketBaseSubscriptionHandle Handle = Client->Subscribe(
        TEXT("messages/*"),
        MoveTemp(Callbacks),
        {},
        Error);

    TestFalse(TEXT("The subscription is accepted"), Error.IsSet());
    TestTrue(TEXT("The subscription handle is active"), Handle.IsActive());
    TestEqual(TEXT("Only the stream opens before PB_CONNECT"), Transport->Requests.Num(), 1);
    TestEqual(TEXT("The first request is GET"), Transport->Requests[0].Method, FString(TEXT("GET")));

    Transport->EmitConnect();

    TestEqual(TEXT("PB_CONNECT causes one subscription POST"), Transport->Requests.Num(), 2);
    TestEqual(TEXT("The second request is POST"), Transport->Requests[1].Method, FString(TEXT("POST")));
    const FUTF8ToTCHAR Converted(
        reinterpret_cast<const ANSICHAR*>(Transport->Requests[1].Body.GetData()),
        Transport->Requests[1].Body.Num());
    const FString Body(Converted.Length(), Converted.Get());
    TestTrue(TEXT("The post contains the current client ID"), Body.Contains(TEXT("client-one")));
    TestTrue(TEXT("The post contains the desired topic"), Body.Contains(TEXT("messages/*")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRealtimeLatestSetTest,
    "OpenPocketBase.Realtime.Manager.PostsLatestSetAndReferenceCountsListeners",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRealtimeLatestSetTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::MemoryOnly;

    const TSharedRef<FControlledRealtimeTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FControlledRealtimeTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseError Error;
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        CreateOpenPocketBaseTestClient(Config, Transport, Error);
    TestTrue(TEXT("The client is created"), Client.IsValid());

    FOpenPocketBaseSubscriptionHandle FirstMessages = Client->Subscribe(
        TEXT("messages/*"), {}, {}, Error);
    Transport->EmitConnect(0, TEXT("client-latest"));
    TestEqual(TEXT("The initial desired set is posted once"), Transport->Posts.Num(), 1);
    TestEqual(
        TEXT("The initial post contains one topic"),
        Transport->GetPostedSubscriptions(0).Num(),
        1);

    FOpenPocketBaseSubscriptionHandle SecondMessages = Client->Subscribe(
        TEXT("messages/*"), {}, {}, Error);
    FOpenPocketBaseSubscriptionHandle Presence = Client->Subscribe(
        TEXT("presence"), {}, {}, Error);
    TestEqual(TEXT("Edits wait behind the in-flight full-set post"), Transport->Posts.Num(), 1);

    Transport->CompletePost(0);
    TestEqual(TEXT("Completion posts the latest full set"), Transport->Posts.Num(), 2);
    const TSet<FString> Latest = Transport->GetPostedSubscriptions(1);
    TestEqual(TEXT("The latest post contains two unique topics"), Latest.Num(), 2);
    TestTrue(TEXT("The latest set contains messages"), Latest.Contains(TEXT("messages/*")));
    TestTrue(TEXT("The latest set contains presence"), Latest.Contains(TEXT("presence")));

    FirstMessages.Unsubscribe();
    TestEqual(
        TEXT("Removing one shared listener does not change the server set"),
        Transport->Posts.Num(),
        2);
    Transport->CompletePost(1);

    Client->UnsubscribeTopic(TEXT("messages/*"));
    TestEqual(TEXT("Removing a topic posts the reduced set"), Transport->Posts.Num(), 3);
    const TSet<FString> Reduced = Transport->GetPostedSubscriptions(2);
    TestEqual(TEXT("The reduced set contains one topic"), Reduced.Num(), 1);
    TestTrue(TEXT("The unrelated topic remains"), Reduced.Contains(TEXT("presence")));
    Transport->CompletePost(2);

    Client->UnsubscribeAllRealtime();
    TestTrue(TEXT("Removing all remaining listeners closes the shared stream"),
        Transport->Streams[0].Cancelled->load(std::memory_order_acquire));
    TestEqual(TEXT("No empty-set post is sent after closing the stream"), Transport->Posts.Num(), 3);
    TestFalse(TEXT("The first handle is inactive"), FirstMessages.IsActive());
    TestFalse(TEXT("The second handle is inactive"), SecondMessages.IsActive());
    TestFalse(TEXT("The final handle is inactive"), Presence.IsActive());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRealtimeTopicOptionsTest,
    "OpenPocketBase.Realtime.Manager.SerializesApprovedTopicOptions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRealtimeTopicOptionsTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    const TSharedRef<FControlledRealtimeTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FControlledRealtimeTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseError Error;
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        CreateOpenPocketBaseTestClient(Config, Transport, Error);
    TestTrue(TEXT("The client is created"), Client.IsValid());

    FOpenPocketBaseRealtimeOptions Options;
    Options.Filter = FOpenPocketBaseFilter::Boolean(
        TEXT("done"),
        EOpenPocketBaseBooleanComparison::Equals,
        false);
    Options.Expand = {TEXT("owner")};
    Options.Fields = {TEXT("id"), TEXT("title")};
    Options.QueryParameters.Add(TEXT("locale"), TEXT("en-GB"));
    Options.Headers.Add(TEXT("X-Tenant"), TEXT("game-one"));
    FOpenPocketBaseSubscriptionHandle Handle = Client->Collection(TEXT("sdk_tasks"))
        .SubscribeToRecords({}, Options, Error);
    TestTrue(TEXT("Approved options are accepted"), Handle.IsActive());
    Transport->EmitConnect(0, TEXT("options-client"));

    const TSet<FString> Posted = Transport->GetPostedSubscriptions(0);
    TestEqual(TEXT("One option-bearing topic is posted"), Posted.Num(), 1);
    const FString WireTopic = Posted.Array()[0];
    TestTrue(TEXT("The collection helper uses the collection wildcard topic"),
        WireTopic.StartsWith(TEXT("sdk_tasks/*?options=")));
    FString EncodedOptions;
    TestTrue(TEXT("The serialized options are present"),
        WireTopic.Split(TEXT("?options="), nullptr, &EncodedOptions));
    const FString DecodedOptions = FGenericPlatformHttp::UrlDecode(EncodedOptions);
    TestTrue(TEXT("The server option envelope contains the filter"),
        DecodedOptions.Contains(TEXT("done = false")));
    TestTrue(TEXT("The server option envelope contains expand"),
        DecodedOptions.Contains(TEXT("owner")));
    TestTrue(TEXT("The server option envelope contains fields"),
        DecodedOptions.Contains(TEXT("id,title")));
    TestTrue(TEXT("The server option envelope contains custom query values"),
        DecodedOptions.Contains(TEXT("en-GB")));
    TestTrue(TEXT("The server option envelope contains approved headers"),
        DecodedOptions.Contains(TEXT("X-Tenant")) && DecodedOptions.Contains(TEXT("game-one")));

    FOpenPocketBaseRealtimeOptions UnsafeOptions;
    UnsafeOptions.Headers.Add(TEXT("Authorization"), TEXT("caller-token"));
    FOpenPocketBaseSubscriptionHandle Unsafe = Client->Subscribe(
        TEXT("unsafe"), {}, UnsafeOptions, Error);
    TestFalse(TEXT("A per-topic auth override is rejected"), Unsafe.IsActive());
    TestEqual(TEXT("The rejected option is a local argument error"),
        Error.Kind, EOpenPocketBaseErrorKind::InvalidArgument);

    Handle.Unsubscribe();
    Client->Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRealtimeReconnectGenerationTest,
    "OpenPocketBase.Realtime.Manager.DropsLateGenerationAndResubscribesLatestSet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRealtimeReconnectGenerationTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::MemoryOnly;

    const TSharedRef<FControlledRealtimeTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FControlledRealtimeTransport, ESPMode::ThreadSafe>();
    const TSharedRef<FRealtimeTestClock, ESPMode::ThreadSafe> Clock =
        MakeShared<FRealtimeTestClock, ESPMode::ThreadSafe>();
    FOpenPocketBaseError Error;
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        CreateOpenPocketBaseTestClient(
            Config,
            Transport,
            CreateOpenPocketBaseSecureStore(),
            Clock,
            Error);
    TestTrue(TEXT("The client is created"), Client.IsValid());

    FOpenPocketBaseSubscriptionHandle Messages = Client->Subscribe(
        TEXT("messages/*"), {}, {}, Error);
    Transport->EmitConnect(0, TEXT("first-client"));
    Transport->CompletePost(0);
    Transport->CompleteStream(0);
    const TOptional<double> InitialReconnectDelay = Clock->GetNextActiveDelay();
    TestTrue(TEXT("A disconnect schedules a clock-driven reconnect"),
        InitialReconnectDelay.IsSet());
    if (InitialReconnectDelay.IsSet())
    {
        TestTrue(TEXT("The first jittered reconnect remains near the 0.5 second base"),
            InitialReconnectDelay.GetValue() >= 0.4 && InitialReconnectDelay.GetValue() <= 0.6);
    }

    FOpenPocketBaseSubscriptionHandle Presence = Client->Subscribe(
        TEXT("presence"), {}, {}, Error);
    TestEqual(TEXT("No stale subscription post is sent while reconnecting"), Transport->Posts.Num(), 1);
    TestTrue(TEXT("The injected clock owns the reconnect"), Clock->RunNextActive());
    TestEqual(TEXT("One replacement stream is opened"), Transport->Streams.Num(), 2);

    Transport->EmitConnect(0, TEXT("late-client"));
    TestEqual(TEXT("A late old-generation chunk cannot post"), Transport->Posts.Num(), 1);

    Transport->EmitConnect(1, TEXT("current-client"));
    TestEqual(TEXT("The current generation posts once"), Transport->Posts.Num(), 2);
    const TSet<FString> Latest = Transport->GetPostedSubscriptions(1);
    TestEqual(TEXT("Reconnect posts the exact latest set"), Latest.Num(), 2);
    TestTrue(TEXT("Reconnect retains messages"), Latest.Contains(TEXT("messages/*")));
    TestTrue(TEXT("Reconnect includes edits made while offline"), Latest.Contains(TEXT("presence")));
    Transport->CompletePost(1);

    Messages.Unsubscribe();
    Presence.Unsubscribe();
    Client->Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRealtimeRetryHintTest,
    "OpenPocketBase.Realtime.Manager.BoundsServerRetryHint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRealtimeRetryHintTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    const TSharedRef<FControlledRealtimeTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FControlledRealtimeTransport, ESPMode::ThreadSafe>();
    const TSharedRef<FRealtimeTestClock, ESPMode::ThreadSafe> Clock =
        MakeShared<FRealtimeTestClock, ESPMode::ThreadSafe>();
    FOpenPocketBaseError Error;
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        CreateOpenPocketBaseTestClient(
            Config,
            Transport,
            CreateOpenPocketBaseSecureStore(),
            Clock,
            Error);
    TestTrue(TEXT("The client is created"), Client.IsValid());

    FOpenPocketBaseSubscriptionHandle Handle = Client->Subscribe(
        TEXT("messages/*"), {}, {}, Error);
    Transport->EmitConnect(0, TEXT("retry-client"));
    Transport->CompletePost(0);
    Transport->EmitStream(
        0,
        TEXT("retry: 45000\nevent: ignored\ndata: {}\n\n"));
    Transport->CompleteStream(0);

    const TOptional<double> Delay = Clock->GetNextActiveDelay();
    TestTrue(TEXT("The server hint schedules a reconnect"), Delay.IsSet());
    if (Delay.IsSet())
    {
        TestEqual(TEXT("The 45 second hint is capped at 30 seconds"),
            Delay.GetValue(), 30.0);
    }
    Handle.Unsubscribe();
    Client->Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRealtimeStableBackoffTest,
    "OpenPocketBase.Realtime.Manager.ResetsBackoffOnlyAfterStableConnection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRealtimeStableBackoffTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    const TSharedRef<FControlledRealtimeTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FControlledRealtimeTransport, ESPMode::ThreadSafe>();
    const TSharedRef<FRealtimeTestClock, ESPMode::ThreadSafe> Clock =
        MakeShared<FRealtimeTestClock, ESPMode::ThreadSafe>();
    FOpenPocketBaseError Error;
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        CreateOpenPocketBaseTestClient(
            Config,
            Transport,
            CreateOpenPocketBaseSecureStore(),
            Clock,
            Error);
    TestTrue(TEXT("The client is created"), Client.IsValid());

    FOpenPocketBaseSubscriptionHandle Handle = Client->Subscribe(
        TEXT("messages/*"), {}, {}, Error);
    Transport->EmitConnect(0, TEXT("backoff-zero"));
    Transport->CompletePost(0);
    Transport->CompleteStream(0);
    TestTrue(TEXT("The first reconnect timer runs"), Clock->RunNextActive());

    Transport->EmitConnect(1, TEXT("backoff-one"));
    Transport->CompletePost(1);
    Transport->CompleteStream(1);
    const TOptional<double> EscalatedDelay = Clock->GetNextActiveDelay();
    TestTrue(TEXT("An immediately unstable connection preserves backoff"),
        EscalatedDelay.IsSet());
    if (EscalatedDelay.IsSet())
    {
        TestTrue(TEXT("The second jittered retry remains near one second"),
            EscalatedDelay.GetValue() >= 0.8 && EscalatedDelay.GetValue() <= 1.2);
    }
    TestTrue(TEXT("The second reconnect timer runs"), Clock->RunNextActive());

    Transport->EmitConnect(2, TEXT("backoff-two"));
    Transport->CompletePost(2);
    const TOptional<double> StableDelay = Clock->GetNextActiveDelay();
    TestTrue(TEXT("A successful reconnect schedules the stability window"),
        StableDelay.IsSet());
    if (StableDelay.IsSet())
    {
        TestEqual(TEXT("The stability window is ten seconds"),
            StableDelay.GetValue(), 10.0);
    }
    TestTrue(TEXT("The stability window completes through the injected clock"),
        Clock->RunNextActive());

    Transport->CompleteStream(2);
    const TOptional<double> ResetDelay = Clock->GetNextActiveDelay();
    TestTrue(TEXT("A later disconnect schedules another reconnect"), ResetDelay.IsSet());
    if (ResetDelay.IsSet())
    {
        TestTrue(TEXT("The stable connection resets backoff to the 0.5 second base"),
            ResetDelay.GetValue() >= 0.4 && ResetDelay.GetValue() <= 0.6);
    }
    Handle.Unsubscribe();
    Client->Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRealtimeLifecycleTest,
    "OpenPocketBase.Realtime.Manager.PausesForLifecycleAndNetworkHints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRealtimeLifecycleTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::MemoryOnly;
    const TSharedRef<FControlledRealtimeTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FControlledRealtimeTransport, ESPMode::ThreadSafe>();
    const TSharedRef<FRealtimeTestClock, ESPMode::ThreadSafe> Clock =
        MakeShared<FRealtimeTestClock, ESPMode::ThreadSafe>();
    FOpenPocketBaseError Error;
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        CreateOpenPocketBaseTestClient(
            Config,
            Transport,
            CreateOpenPocketBaseSecureStore(),
            Clock,
            Error);
    TestTrue(TEXT("The client is created"), Client.IsValid());

    FOpenPocketBaseSubscriptionHandle Handle = Client->Subscribe(
        TEXT("messages/*"), {}, {}, Error);
    Transport->EmitConnect(0, TEXT("lifecycle-one"));
    Transport->CompletePost(0);

    FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
    TestTrue(TEXT("Backgrounding cancels the live stream"),
        Transport->Streams[0].Cancelled->load(std::memory_order_acquire));
    TestTrue(TEXT("The listener remains desired while paused"), Handle.IsActive());

    FCoreDelegates::OnNetworkConnectionStatusChanged.Broadcast(
        ENetworkConnectionStatus::Connected,
        ENetworkConnectionStatus::Disabled);
    FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
    TestEqual(TEXT("Foreground waits while the network is unavailable"), Transport->Streams.Num(), 1);

    FCoreDelegates::OnNetworkConnectionStatusChanged.Broadcast(
        ENetworkConnectionStatus::Disabled,
        ENetworkConnectionStatus::Connected);
    TestEqual(TEXT("A restored network opens one replacement stream"), Transport->Streams.Num(), 2);
    Transport->EmitConnect(1, TEXT("lifecycle-two"));
    TestEqual(TEXT("The replacement identity posts the desired set"), Transport->Posts.Num(), 2);
    TestTrue(TEXT("The replacement post retains the topic"),
        Transport->GetPostedSubscriptions(1).Contains(TEXT("messages/*")));
    Transport->CompletePost(1);

    Handle.Unsubscribe();
    Client->Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRealtimeBoundedDeliveryTest,
    "OpenPocketBase.Realtime.Manager.BoundsOrderedDeliveryAndSignalsOverflow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRealtimeBoundedDeliveryTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FRealtimeDeliveryState, ESPMode::ThreadSafe> State =
        MakeShared<FRealtimeDeliveryState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FControlledRealtimeTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::MemoryOnly;
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    TestTrue(TEXT("The client is created"), State->Client.IsValid());

    FOpenPocketBaseRealtimeCallbacks Callbacks;
    Callbacks.OnEvent = [State](const FOpenPocketBaseRealtimeEvent& Event)
    {
        State->Actions.Add(Event.ActionName);
    };
    Callbacks.OnResyncRequired = [State]()
    {
        ++State->ResyncCount;
    };
    State->Handle = State->Client->Subscribe(
        TEXT("messages/*"), MoveTemp(Callbacks), {}, Error);
    State->Transport->EmitConnect(0, TEXT("delivery-client"));
    State->Transport->CompletePost(0);

    FString Payload;
    for (int32 Index = 0; Index < 1024; ++Index)
    {
        Payload += FString::Printf(
            TEXT("event: messages/*\ndata: {\"action\":\"%d\"}\n\n"),
            Index);
    }
    State->Transport->EmitStream(0, Payload);
    State->Transport->EmitStream(
        0,
        TEXT("event: messages/*\ndata: {\"action\":\"overflow\"}\n\n"));

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyRealtimeDelivery(State, this, 1024));
    return true;
}

#endif
