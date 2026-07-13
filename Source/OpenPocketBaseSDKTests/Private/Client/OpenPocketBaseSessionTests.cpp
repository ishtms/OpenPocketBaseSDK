#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"
#include "OpenPocketBaseScriptedTransport.h"
#include "OpenPocketBaseSession.h"

namespace
{
TArray<uint8> SessionToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

struct FSessionTestState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport;
    TArray<FOpenPocketBaseSessionSnapshot> Events;
    bool bCompleted = false;
    int32 RefreshCallbacks = 0;
    bool bRefreshSucceeded = true;
    EOpenPocketBaseErrorKind RefreshErrorKind = EOpenPocketBaseErrorKind::None;
};

class FVerifySessionLifecycle final : public IAutomationLatentCommand
{
public:
    FVerifySessionLifecycle(
        const TSharedRef<FSessionTestState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted || State->Events.Num() != 2)
        {
            return false;
        }

        Test->TestEqual(TEXT("Login and logout publish two ordered events"), State->Events.Num(), 2);
        Test->TestEqual(TEXT("Login is the first change"), State->Events[0].Reason, EOpenPocketBaseSessionChangeReason::LoggedIn);
        Test->TestTrue(TEXT("Login snapshot is authenticated"), State->Events[0].bAuthenticated);
        Test->TestEqual(TEXT("Login stores the auth collection"), State->Events[0].AuthCollection, FString(TEXT("users")));
        Test->TestEqual(TEXT("Login advances generation"), State->Events[0].AuthGeneration, static_cast<int64>(1));
        Test->TestEqual(TEXT("Logout is the second change"), State->Events[1].Reason, EOpenPocketBaseSessionChangeReason::LoggedOut);
        Test->TestFalse(TEXT("Logout snapshot is unauthenticated"), State->Events[1].bAuthenticated);
        Test->TestEqual(TEXT("Logout advances generation"), State->Events[1].AuthGeneration, static_cast<int64>(2));
        Test->TestFalse(TEXT("Logout clears the client synchronously"), State->Client->IsAuthenticated());
        Test->TestEqual(TEXT("Logout does not invent a server endpoint"), State->Transport->GetRequestCount(), 1);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FSessionTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifySingleFlightRefresh final : public IAutomationLatentCommand
{
public:
    FVerifySingleFlightRefresh(
        const TSharedRef<FSessionTestState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted || State->Events.Num() != 2)
        {
            return false;
        }

        Test->TestEqual(TEXT("All refresh waiters complete"), State->RefreshCallbacks, 3);
        Test->TestTrue(TEXT("Every refresh waiter succeeds"), State->bRefreshSucceeded);
        Test->TestEqual(TEXT("Three callers share one refresh request"), State->Transport->GetRequestCount(), 2);
        FOpenPocketBaseHttpRequest RefreshRequest;
        Test->TestTrue(TEXT("The refresh request is captured"), State->Transport->TryGetRequest(1, RefreshRequest));
        Test->TestEqual(TEXT("Auth Refresh uses POST"), RefreshRequest.Method, FString(TEXT("POST")));
        Test->TestEqual(
            TEXT("Auth Refresh uses the authenticated collection route"),
            RefreshRequest.Url,
            FString(TEXT("https://pb.example.com/api/collections/users/auth-refresh")));
        Test->TestEqual(
            TEXT("Auth Refresh sends the current auth token"),
            RefreshRequest.Headers.FindRef(TEXT("Authorization")),
            FString(TEXT("login.token.signature")));
        FOpenPocketBaseSessionSnapshot Session;
        Test->TestTrue(TEXT("The refreshed session remains authenticated"), State->Client->GetCurrentSession(Session));
        Test->TestEqual(TEXT("Refresh advances the generation once"), Session.AuthGeneration, static_cast<int64>(2));
        Test->TestEqual(TEXT("Refresh publishes one ordered change"), State->Events[1].Reason, EOpenPocketBaseSessionChangeReason::Refreshed);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FSessionTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyRefreshLogoutRace final : public IAutomationLatentCommand
{
public:
    FVerifyRefreshLogoutRace(
        const TSharedRef<FSessionTestState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted || State->Events.Num() != 2)
        {
            return false;
        }

        Test->TestFalse(TEXT("A stale refresh does not succeed"), State->bRefreshSucceeded);
        Test->TestEqual(TEXT("A stale refresh reports authentication change"), State->RefreshErrorKind, EOpenPocketBaseErrorKind::Authentication);
        Test->TestFalse(TEXT("A late refresh cannot undo logout"), State->Client->IsAuthenticated());
        FOpenPocketBaseSessionSnapshot Session;
        Test->TestFalse(TEXT("The logged-out session has no record"), State->Client->GetCurrentSession(Session));
        Test->TestEqual(TEXT("Logout owns the current generation"), Session.AuthGeneration, static_cast<int64>(2));
        Test->TestEqual(TEXT("No stale refreshed event is published"), State->Events[1].Reason, EOpenPocketBaseSessionChangeReason::LoggedOut);
        Test->TestEqual(TEXT("The race still uses one refresh request"), State->Transport->GetRequestCount(), 2);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FSessionTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyRefreshUserSwitchRace final : public IAutomationLatentCommand
{
public:
    FVerifyRefreshUserSwitchRace(
        const TSharedRef<FSessionTestState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted || State->Events.Num() != 2)
        {
            return false;
        }

        Test->TestFalse(TEXT("The old user's refresh is rejected"), State->bRefreshSucceeded);
        Test->TestEqual(TEXT("The stale refresh reports an auth change"), State->RefreshErrorKind, EOpenPocketBaseErrorKind::Authentication);
        FOpenPocketBaseRecord Record;
        Test->TestTrue(TEXT("The switched session stays authenticated"), State->Client->GetCurrentAuthRecord(Record));
        Test->TestEqual(TEXT("The late refresh cannot replace the new user"), Record.Id, FString(TEXT("user00000000002")));
        Test->TestEqual(TEXT("User switch is published second"), State->Events[1].Reason, EOpenPocketBaseSessionChangeReason::UserSwitched);
        Test->TestEqual(TEXT("User switch owns generation two"), State->Events[1].AuthGeneration, static_cast<int64>(2));
        Test->TestEqual(TEXT("The flow sends two logins and one refresh"), State->Transport->GetRequestCount(), 3);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FSessionTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSessionLifecycleTest,
    "OpenPocketBase.Client.Session.LoginLogoutOrdering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSessionLifecycleTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FSessionTestState, ESPMode::ThreadSafe> State =
        MakeShared<FSessionTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseTransportScript Script;
    Script.Response.bTransportSucceeded = true;
    Script.Response.HttpStatus = 200;
    Script.Response.Body = SessionToUtf8(
        TEXT("{\"token\":\"header.payload.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(Script));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    State->Client->OnSessionChanged().AddLambda(
        [State](const FOpenPocketBaseSessionSnapshot& Snapshot)
        {
            State->Events.Add(Snapshot);
        });
    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
            if (Result.IsSuccess())
            {
                State->Client->Logout();
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifySessionLifecycle(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSingleFlightRefreshTest,
    "OpenPocketBase.Client.Session.RefreshIsSingleFlight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSingleFlightRefreshTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FSessionTestState, ESPMode::ThreadSafe> State =
        MakeShared<FSessionTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseTransportScript LoginScript;
    LoginScript.Response.bTransportSucceeded = true;
    LoginScript.Response.HttpStatus = 200;
    LoginScript.Response.Body = SessionToUtf8(
        TEXT("{\"token\":\"login.token.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(LoginScript));

    FOpenPocketBaseTransportScript RefreshScript;
    RefreshScript.bHoldCompletion = true;
    RefreshScript.Response.bTransportSucceeded = true;
    RefreshScript.Response.HttpStatus = 200;
    RefreshScript.Response.Body = SessionToUtf8(
        TEXT("{\"token\":\"refreshed.token.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(RefreshScript));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }
    State->Client->OnSessionChanged().AddLambda(
        [State](const FOpenPocketBaseSessionSnapshot& Snapshot)
        {
            State->Events.Add(Snapshot);
        });

    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->bRefreshSucceeded = false;
                State->bCompleted = true;
                return;
            }

            const auto StartWaiter = [State]()
            {
                State->Client->RefreshAuth(
                    [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& RefreshResult)
                    {
                        State->bRefreshSucceeded = State->bRefreshSucceeded && RefreshResult.IsSuccess();
                        ++State->RefreshCallbacks;
                        State->bCompleted = State->RefreshCallbacks == 3;
                    });
            };
            StartWaiter();
            StartWaiter();
            StartWaiter();
            State->Transport->CompleteNextHeld();
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifySingleFlightRefresh(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRefreshLogoutRaceTest,
    "OpenPocketBase.Client.Session.LateRefreshCannotUndoLogout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRefreshLogoutRaceTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FSessionTestState, ESPMode::ThreadSafe> State =
        MakeShared<FSessionTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseTransportScript LoginScript;
    LoginScript.Response.bTransportSucceeded = true;
    LoginScript.Response.HttpStatus = 200;
    LoginScript.Response.Body = SessionToUtf8(
        TEXT("{\"token\":\"login.token.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(LoginScript));

    FOpenPocketBaseTransportScript RefreshScript;
    RefreshScript.bHoldCompletion = true;
    RefreshScript.Response.bTransportSucceeded = true;
    RefreshScript.Response.HttpStatus = 200;
    RefreshScript.Response.Body = SessionToUtf8(
        TEXT("{\"token\":\"late.token.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(RefreshScript));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }
    State->Client->OnSessionChanged().AddLambda(
        [State](const FOpenPocketBaseSessionSnapshot& Snapshot)
        {
            State->Events.Add(Snapshot);
        });

    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->bCompleted = true;
                return;
            }
            State->Client->RefreshAuth(
                [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& RefreshResult)
                {
                    State->bRefreshSucceeded = RefreshResult.IsSuccess();
                    if (!RefreshResult.IsSuccess())
                    {
                        State->RefreshErrorKind = RefreshResult.GetError().Kind;
                    }
                    State->bCompleted = true;
                });
            State->Client->Logout();
            State->Transport->CompleteNextHeld();
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyRefreshLogoutRace(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRefreshUserSwitchRaceTest,
    "OpenPocketBase.Client.Session.LateRefreshCannotUndoUserSwitch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRefreshUserSwitchRaceTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FSessionTestState, ESPMode::ThreadSafe> State =
        MakeShared<FSessionTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseTransportScript FirstLogin;
    FirstLogin.Response.bTransportSucceeded = true;
    FirstLogin.Response.HttpStatus = 200;
    FirstLogin.Response.Body = SessionToUtf8(
        TEXT("{\"token\":\"first.token.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(FirstLogin));

    FOpenPocketBaseTransportScript Refresh;
    Refresh.bHoldCompletion = true;
    Refresh.Response.bTransportSucceeded = true;
    Refresh.Response.HttpStatus = 200;
    Refresh.Response.Body = SessionToUtf8(
        TEXT("{\"token\":\"stale.token.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(Refresh));

    FOpenPocketBaseTransportScript SecondLogin;
    SecondLogin.Response.bTransportSucceeded = true;
    SecondLogin.Response.HttpStatus = 200;
    SecondLogin.Response.Body = SessionToUtf8(
        TEXT("{\"token\":\"second.token.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000002\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(SecondLogin));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }
    State->Client->OnSessionChanged().AddLambda(
        [State](const FOpenPocketBaseSessionSnapshot& Snapshot)
        {
            State->Events.Add(Snapshot);
        });

    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("first@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->bCompleted = true;
                return;
            }

            State->Client->RefreshAuth(
                [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& RefreshResult)
                {
                    State->bRefreshSucceeded = RefreshResult.IsSuccess();
                    if (!RefreshResult.IsSuccess())
                    {
                        State->RefreshErrorKind = RefreshResult.GetError().Kind;
                    }
                    State->bCompleted = true;
                });
            State->Client->Collection(TEXT("users")).AuthWithPassword(
                TEXT("second@example.com"),
                TEXT("private-password"),
                [](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& SecondResult) {});
            State->Transport->CompleteNextHeld();
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyRefreshUserSwitchRace(State, this));
    return true;
}

#endif
