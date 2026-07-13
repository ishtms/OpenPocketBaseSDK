#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Clock/OpenPocketBaseClock.h"
#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"
#include "OpenPocketBaseScriptedTransport.h"
#include "SecureStorage/OpenPocketBaseSecureStore.h"

namespace
{
TArray<uint8> CoordinationToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

class FFixedOpenPocketBaseClock final : public IOpenPocketBaseClock
{
public:
    virtual FDateTime UtcNow() const override
    {
        return FDateTime::FromUnixTimestamp(1000);
    }

    virtual double MonotonicSeconds() const override
    {
        return 1000.0;
    }

    virtual FOpenPocketBaseClockHandle Schedule(
        const double DelaySeconds,
        TUniqueFunction<void()> Callback) override
    {
        Scheduled.Add(MoveTemp(Callback));
        return {};
    }

    bool RunNext()
    {
        if (Scheduled.IsEmpty())
        {
            return false;
        }
        TUniqueFunction<void()> Callback = MoveTemp(Scheduled[0]);
        Scheduled.RemoveAt(0, EAllowShrinking::No);
        if (Callback)
        {
            Callback();
        }
        return true;
    }

    TArray<TUniqueFunction<void()>> Scheduled;
};

struct FAuthCoordinationState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport;
    int32 ReadCallbacks = 0;
    bool bAllReadsSucceeded = true;
};

struct FAuthReplayState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport;
    EOpenPocketBaseErrorKind ErrorKind = EOpenPocketBaseErrorKind::None;
    bool bSucceeded = false;
    bool bCompleted = false;
};

struct FClockRetryState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport;
    TSharedPtr<FFixedOpenPocketBaseClock, ESPMode::ThreadSafe> Clock;
    bool bSucceeded = false;
    bool bCompleted = false;
};

class FVerifyProactiveSingleFlight final : public IAutomationLatentCommand
{
public:
    FVerifyProactiveSingleFlight(
        const TSharedRef<FAuthCoordinationState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (State->ReadCallbacks != 3)
        {
            return false;
        }

        Test->TestTrue(TEXT("Every queued read succeeds"), State->bAllReadsSucceeded);
        Test->TestEqual(TEXT("Three reads share one proactive refresh"), State->Transport->GetRequestCount(), 5);
        FOpenPocketBaseHttpRequest Refresh;
        Test->TestTrue(TEXT("The proactive refresh is captured"), State->Transport->TryGetRequest(1, Refresh));
        Test->TestTrue(TEXT("The proactive request uses Auth Refresh"), Refresh.Url.EndsWith(TEXT("/api/collections/users/auth-refresh")));
        for (int32 Index = 2; Index < 5; ++Index)
        {
            FOpenPocketBaseHttpRequest Read;
            Test->TestTrue(TEXT("The queued read is captured"), State->Transport->TryGetRequest(Index, Read));
            Test->TestEqual(
                TEXT("Queued reads use the refreshed token"),
                Read.Headers.FindRef(TEXT("Authorization")),
                FString(TEXT("header.eyJleHAiOjUwMDB9.signature")));
        }
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAuthCoordinationState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyReadAuthReplay final : public IAutomationLatentCommand
{
public:
    FVerifyReadAuthReplay(
        const TSharedRef<FAuthReplayState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted)
        {
            return false;
        }
        Test->TestTrue(TEXT("The read succeeds after Auth Refresh"), State->bSucceeded);
        Test->TestEqual(TEXT("Login, rejected read, refresh, and one replay are sent"), State->Transport->GetRequestCount(), 4);
        FOpenPocketBaseHttpRequest Refresh;
        FOpenPocketBaseHttpRequest Replay;
        Test->TestTrue(TEXT("The reactive refresh is captured"), State->Transport->TryGetRequest(2, Refresh));
        Test->TestTrue(TEXT("The read replay is captured"), State->Transport->TryGetRequest(3, Replay));
        Test->TestTrue(TEXT("The third request is Auth Refresh"), Refresh.Url.EndsWith(TEXT("/api/collections/users/auth-refresh")));
        Test->TestEqual(
            TEXT("The read replay uses the authoritative refreshed token"),
            Replay.Headers.FindRef(TEXT("Authorization")),
            FString(TEXT("header.eyJleHAiOjYwMDB9.signature")));
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAuthReplayState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyMutationNotReplayed final : public IAutomationLatentCommand
{
public:
    FVerifyMutationNotReplayed(
        const TSharedRef<FAuthReplayState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted)
        {
            return false;
        }
        Test->TestFalse(TEXT("The rejected mutation does not succeed"), State->bSucceeded);
        Test->TestEqual(TEXT("The rejection keeps the PocketBase error"), State->ErrorKind, EOpenPocketBaseErrorKind::PocketBase);
        Test->TestEqual(TEXT("A mutation rejection sends no refresh or replay"), State->Transport->GetRequestCount(), 2);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAuthReplayState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyClockRetry final : public IAutomationLatentCommand
{
public:
    FVerifyClockRetry(
        const TSharedRef<FClockRetryState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted)
        {
            return false;
        }
        Test->TestTrue(TEXT("The clock-released retry succeeds"), State->bSucceeded);
        Test->TestEqual(TEXT("Exactly one retry is sent"), State->Transport->GetRequestCount(), 2);
        Test->TestTrue(TEXT("The fake clock queue is drained"), State->Clock->Scheduled.IsEmpty());
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FClockRetryState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseProactiveSingleFlightTest,
    "OpenPocketBase.Client.Session.ProactiveRefreshIsClockDrivenSingleFlight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseProactiveSingleFlightTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAuthCoordinationState, ESPMode::ThreadSafe> State =
        MakeShared<FAuthCoordinationState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseTransportScript Login;
    Login.Response.bTransportSucceeded = true;
    Login.Response.HttpStatus = 200;
    Login.Response.Body = CoordinationToUtf8(
        TEXT("{\"token\":\"header.eyJleHAiOjEwMTB9.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(Login));

    FOpenPocketBaseTransportScript Refresh;
    Refresh.bHoldCompletion = true;
    Refresh.Response.bTransportSucceeded = true;
    Refresh.Response.HttpStatus = 200;
    Refresh.Response.Body = CoordinationToUtf8(
        TEXT("{\"token\":\"header.eyJleHAiOjUwMDB9.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(Refresh));

    for (int32 Index = 0; Index < 3; ++Index)
    {
        FOpenPocketBaseTransportScript Read;
        Read.Response.bTransportSucceeded = true;
        Read.Response.HttpStatus = 200;
        Read.Response.Body = CoordinationToUtf8(
            TEXT("{\"id\":\"task00000000001\",\"collectionId\":\"tasks_id\",")
            TEXT("\"collectionName\":\"tasks\"}"));
        State->Transport->Enqueue(MoveTemp(Read));
    }

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    Config.AuthRefreshLeadTimeSeconds = 30.0;
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(
        Config,
        State->Transport.ToSharedRef(),
        CreateOpenPocketBaseSecureStore(),
        MakeShared<FFixedOpenPocketBaseClock, ESPMode::ThreadSafe>(),
        Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->bAllReadsSucceeded = false;
                State->ReadCallbacks = 3;
                return;
            }

            const auto StartRead = [State]()
            {
                State->Client->Collection(TEXT("tasks")).GetOne(
                    TEXT("task00000000001"),
                    [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& ReadResult)
                    {
                        State->bAllReadsSucceeded = State->bAllReadsSucceeded && ReadResult.IsSuccess();
                        ++State->ReadCallbacks;
                    });
            };
            StartRead();
            StartRead();
            StartRead();
            State->Transport->CompleteNextHeld();
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyProactiveSingleFlight(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseReadAuthReplayTest,
    "OpenPocketBase.Client.Session.ReadRetriesOnceAfterAuthRefresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseReadAuthReplayTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAuthReplayState, ESPMode::ThreadSafe> State =
        MakeShared<FAuthReplayState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseTransportScript Login;
    Login.Response.bTransportSucceeded = true;
    Login.Response.HttpStatus = 200;
    Login.Response.Body = CoordinationToUtf8(
        TEXT("{\"token\":\"header.eyJleHAiOjUwMDB9.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(Login));

    FOpenPocketBaseTransportScript RejectedRead;
    RejectedRead.Response.bTransportSucceeded = true;
    RejectedRead.Response.HttpStatus = 401;
    RejectedRead.Response.Body = CoordinationToUtf8(
        TEXT("{\"status\":401,\"message\":\"The request requires valid authorization token.\",\"data\":{}}"));
    State->Transport->Enqueue(MoveTemp(RejectedRead));

    FOpenPocketBaseTransportScript Refresh;
    Refresh.Response.bTransportSucceeded = true;
    Refresh.Response.HttpStatus = 200;
    Refresh.Response.Body = CoordinationToUtf8(
        TEXT("{\"token\":\"header.eyJleHAiOjYwMDB9.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(Refresh));

    FOpenPocketBaseTransportScript SuccessfulReplay;
    SuccessfulReplay.Response.bTransportSucceeded = true;
    SuccessfulReplay.Response.HttpStatus = 200;
    SuccessfulReplay.Response.Body = CoordinationToUtf8(
        TEXT("{\"id\":\"task00000000001\",\"collectionId\":\"tasks_id\",")
        TEXT("\"collectionName\":\"tasks\"}"));
    State->Transport->Enqueue(MoveTemp(SuccessfulReplay));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(
        Config,
        State->Transport.ToSharedRef(),
        CreateOpenPocketBaseSecureStore(),
        MakeShared<FFixedOpenPocketBaseClock, ESPMode::ThreadSafe>(),
        Error);
    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& LoginResult)
        {
            if (!LoginResult.IsSuccess())
            {
                State->ErrorKind = LoginResult.GetError().Kind;
                State->bCompleted = true;
                return;
            }
            State->Client->Collection(TEXT("tasks")).GetOne(
                TEXT("task00000000001"),
                [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& ReadResult)
                {
                    State->bSucceeded = ReadResult.IsSuccess();
                    if (!ReadResult.IsSuccess())
                    {
                        State->ErrorKind = ReadResult.GetError().Kind;
                    }
                    State->bCompleted = true;
                });
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyReadAuthReplay(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseMutationNoAuthReplayTest,
    "OpenPocketBase.Client.Session.MutationIsNeverReplayedAfterAuthRejection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseMutationNoAuthReplayTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAuthReplayState, ESPMode::ThreadSafe> State =
        MakeShared<FAuthReplayState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseTransportScript Login;
    Login.Response.bTransportSucceeded = true;
    Login.Response.HttpStatus = 200;
    Login.Response.Body = CoordinationToUtf8(
        TEXT("{\"token\":\"header.eyJleHAiOjUwMDB9.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(Login));

    FOpenPocketBaseTransportScript RejectedMutation;
    RejectedMutation.Response.bTransportSucceeded = true;
    RejectedMutation.Response.HttpStatus = 401;
    RejectedMutation.Response.Body = CoordinationToUtf8(
        TEXT("{\"status\":401,\"message\":\"The request requires valid authorization token.\",\"data\":{}}"));
    State->Transport->Enqueue(MoveTemp(RejectedMutation));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(
        Config,
        State->Transport.ToSharedRef(),
        CreateOpenPocketBaseSecureStore(),
        MakeShared<FFixedOpenPocketBaseClock, ESPMode::ThreadSafe>(),
        Error);
    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& LoginResult)
        {
            if (!LoginResult.IsSuccess())
            {
                State->ErrorKind = LoginResult.GetError().Kind;
                State->bCompleted = true;
                return;
            }
            FOpenPocketBaseRecordBody Body;
            Body.SetStringField(TEXT("title"), TEXT("Must not replay"));
            State->Client->Collection(TEXT("tasks")).Create(
                MoveTemp(Body),
                [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& CreateResult)
                {
                    State->bSucceeded = CreateResult.IsSuccess();
                    if (!CreateResult.IsSuccess())
                    {
                        State->ErrorKind = CreateResult.GetError().Kind;
                    }
                    State->bCompleted = true;
                });
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyMutationNotReplayed(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseInjectedClockRetryTest,
    "OpenPocketBase.Client.Session.ReadRetryUsesInjectedClock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseInjectedClockRetryTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FClockRetryState, ESPMode::ThreadSafe> State =
        MakeShared<FClockRetryState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    State->Clock = MakeShared<FFixedOpenPocketBaseClock, ESPMode::ThreadSafe>();

    FOpenPocketBaseTransportScript Retryable;
    Retryable.Response.bTransportSucceeded = true;
    Retryable.Response.HttpStatus = 503;
    State->Transport->Enqueue(MoveTemp(Retryable));

    FOpenPocketBaseTransportScript Success;
    Success.Response.bTransportSucceeded = true;
    Success.Response.HttpStatus = 200;
    Success.Response.Body = CoordinationToUtf8(
        TEXT("{\"id\":\"task00000000001\",\"collectionId\":\"tasks_id\",")
        TEXT("\"collectionName\":\"tasks\"}"));
    State->Transport->Enqueue(MoveTemp(Success));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(
        Config,
        State->Transport.ToSharedRef(),
        CreateOpenPocketBaseSecureStore(),
        State->Clock.ToSharedRef(),
        Error);
    FOpenPocketBaseRecordOptions Options;
    Options.RequestOptions.MaxReadRetries = 1;
    Options.RequestOptions.RetryBaseDelaySeconds = 1.0;
    Options.RequestOptions.RetryJitterFraction = 0;
    State->Client->Collection(TEXT("tasks")).GetOne(
        TEXT("task00000000001"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bSucceeded = Result.IsSuccess();
            State->bCompleted = true;
        },
        Options);

    TestEqual(TEXT("The first attempt is sent before the clock advances"), State->Transport->GetRequestCount(), 1);
    TestEqual(TEXT("The retry waits in the injected clock"), State->Clock->Scheduled.Num(), 1);
    TestTrue(TEXT("The test clock releases the retry"), State->Clock->RunNext());

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyClockRetry(State, this));
    return true;
}

#endif
