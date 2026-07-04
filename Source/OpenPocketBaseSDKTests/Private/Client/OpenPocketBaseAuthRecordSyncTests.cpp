#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseScriptedTransport.h"
#include "SecureStorage/OpenPocketBaseSecureStore.h"

namespace
{
TArray<uint8> AuthRecordToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

struct FAuthRecordSyncState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport;
    TArray<FOpenPocketBaseSessionSnapshot> Events;
    EOpenPocketBaseErrorKind ErrorKind = EOpenPocketBaseErrorKind::None;
    int32 Callbacks = 0;
    bool bUpdateSucceeded = false;
    bool bRefreshSucceeded = true;
    bool bCompleted = false;
};

class FFailingUpdateSecureStore final : public IOpenPocketBaseSecureStore
{
public:
    virtual bool IsAvailable(FString& OutReason) const override
    {
        OutReason.Reset();
        return true;
    }

    virtual bool Save(
        const FString& Key,
        TConstArrayView<uint8> Value,
        FOpenPocketBaseError& OutError) override
    {
        ++SaveCalls;
        if (SaveCalls > 1)
        {
            OutError = FOpenPocketBaseError();
            OutError.Kind = EOpenPocketBaseErrorKind::SecureStorage;
            OutError.ServerMessage = TEXT("The injected store rejected the auth-record update.");
            return false;
        }
        Stored.Reset(Value.Num());
        Stored.Append(Value.GetData(), Value.Num());
        OutError = FOpenPocketBaseError();
        return true;
    }

    virtual bool Load(
        const FString& Key,
        TArray<uint8>& OutValue,
        bool& bOutFound,
        FOpenPocketBaseError& OutError) override
    {
        OutValue = Stored;
        bOutFound = !Stored.IsEmpty();
        OutError = FOpenPocketBaseError();
        return true;
    }

    virtual bool Delete(const FString& Key, FOpenPocketBaseError& OutError) override
    {
        Stored.Reset();
        OutError = FOpenPocketBaseError();
        return true;
    }

    TArray<uint8> Stored;
    int32 SaveCalls = 0;
};

class FVerifyAuthRecordSync final : public IAutomationLatentCommand
{
public:
    FVerifyAuthRecordSync(
        const TSharedRef<FAuthRecordSyncState, ESPMode::ThreadSafe>& InState,
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
        if (State->Events.Num() < 2)
        {
            if (++WaitFrames < 5)
            {
                return false;
            }
            Test->AddError(TEXT("The authoritative auth-record update did not publish its session event."));
            State->Client->Shutdown();
            return true;
        }
        Test->TestTrue(TEXT("The auth-record update request succeeds"), State->bUpdateSucceeded);
        Test->TestEqual(TEXT("Login and record update publish two events"), State->Events.Num(), 2);
        Test->TestEqual(TEXT("The second event is Record Updated"), State->Events[1].Reason, EOpenPocketBaseSessionChangeReason::RecordUpdated);
        Test->TestEqual(TEXT("The authoritative update advances generation"), State->Events[1].AuthGeneration, static_cast<int64>(2));
        FOpenPocketBaseRecord CurrentRecord;
        Test->TestTrue(TEXT("The current auth record remains available"), State->Client->GetCurrentAuthRecord(CurrentRecord));
        FString DisplayName;
        Test->TestTrue(
            TEXT("The authoritative field is retained"),
            CurrentRecord.Data.JsonObject.IsValid() &&
                CurrentRecord.Data.JsonObject->TryGetStringField(TEXT("displayName"), DisplayName));
        Test->TestEqual(TEXT("The auth store has the updated value"), DisplayName, FString(TEXT("Updated Player")));
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAuthRecordSyncState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
    int32 WaitFrames = 0;
};

class FVerifyAuthRecordPersistenceFailure final : public IAutomationLatentCommand
{
public:
    FVerifyAuthRecordPersistenceFailure(
        const TSharedRef<FAuthRecordSyncState, ESPMode::ThreadSafe>& InState,
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
        Test->TestFalse(TEXT("A failed secure write fails the update result"), State->bUpdateSucceeded);
        Test->TestEqual(TEXT("The failure is classified as secure storage"), State->ErrorKind, EOpenPocketBaseErrorKind::SecureStorage);
        Test->TestEqual(TEXT("Only login publishes a session event"), State->Events.Num(), 1);
        FOpenPocketBaseSessionSnapshot Session;
        Test->TestTrue(TEXT("The prior authenticated session remains"), State->Client->GetCurrentSession(Session));
        Test->TestEqual(TEXT("A failed transaction keeps generation one"), Session.AuthGeneration, static_cast<int64>(1));
        FString DisplayName;
        Test->TestTrue(
            TEXT("The prior auth record remains readable"),
            Session.AuthRecord.Data.JsonObject.IsValid() &&
                Session.AuthRecord.Data.JsonObject->TryGetStringField(TEXT("displayName"), DisplayName));
        Test->TestEqual(TEXT("The failed update does not alter memory"), DisplayName, FString(TEXT("Player")));
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAuthRecordSyncState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyAuthRecordRefreshRace final : public IAutomationLatentCommand
{
public:
    FVerifyAuthRecordRefreshRace(
        const TSharedRef<FAuthRecordSyncState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted || State->Events.Num() < 2)
        {
            return false;
        }
        Test->TestTrue(TEXT("The authoritative record update succeeds"), State->bUpdateSucceeded);
        Test->TestFalse(TEXT("The older refresh is rejected"), State->bRefreshSucceeded);
        Test->TestEqual(TEXT("The older refresh reports an auth-generation change"), State->ErrorKind, EOpenPocketBaseErrorKind::Authentication);
        Test->TestEqual(TEXT("Only login and record update publish"), State->Events.Num(), 2);
        Test->TestEqual(TEXT("Record update owns generation two"), State->Events[1].Reason, EOpenPocketBaseSessionChangeReason::RecordUpdated);
        Test->TestEqual(TEXT("The flow sends login, refresh, and update"), State->Transport->GetRequestCount(), 3);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAuthRecordSyncState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyAuthRecordDelete final : public IAutomationLatentCommand
{
public:
    FVerifyAuthRecordDelete(
        const TSharedRef<FAuthRecordSyncState, ESPMode::ThreadSafe>& InState,
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
        if (State->Events.Num() < 2)
        {
            if (++WaitFrames < 5)
            {
                return false;
            }
            Test->AddError(TEXT("Deleting the current auth record did not publish logout."));
            State->Client->Shutdown();
            return true;
        }
        Test->TestTrue(TEXT("The auth-record delete request succeeds"), State->bUpdateSucceeded);
        Test->TestFalse(TEXT("Deleting the current auth record clears authentication"), State->Client->IsAuthenticated());
        Test->TestEqual(TEXT("The authoritative delete publishes logout"), State->Events[1].Reason, EOpenPocketBaseSessionChangeReason::LoggedOut);
        Test->TestEqual(TEXT("The delete advances auth generation"), State->Events[1].AuthGeneration, static_cast<int64>(2));
        Test->TestEqual(TEXT("The flow sends login and delete"), State->Transport->GetRequestCount(), 2);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAuthRecordSyncState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
    int32 WaitFrames = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAuthRecordSyncTest,
    "OpenPocketBase.Client.Session.AuthRecordUpdateIsAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAuthRecordSyncTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAuthRecordSyncState, ESPMode::ThreadSafe> State =
        MakeShared<FAuthRecordSyncState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseTransportScript Login;
    Login.Response.bTransportSucceeded = true;
    Login.Response.HttpStatus = 200;
    Login.Response.Body = AuthRecordToUtf8(
        TEXT("{\"token\":\"header.payload.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\",\"displayName\":\"Player\"}}"));
    State->Transport->Enqueue(MoveTemp(Login));

    FOpenPocketBaseTransportScript Update;
    Update.Response.bTransportSucceeded = true;
    Update.Response.HttpStatus = 200;
    Update.Response.Body = AuthRecordToUtf8(
        TEXT("{\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\",\"displayName\":\"Updated Player\"}"));
    State->Transport->Enqueue(MoveTemp(Update));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    State->Client->OnSessionChanged().AddLambda(
        [State](const FOpenPocketBaseSessionSnapshot& Snapshot)
        {
            State->Events.Add(Snapshot);
        });
    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& LoginResult)
        {
            if (!LoginResult.IsSuccess())
            {
                State->bCompleted = true;
                return;
            }
            FOpenPocketBaseRecordBody Body;
            Body.SetStringField(TEXT("displayName"), TEXT("Updated Player"));
            State->Client->Collection(TEXT("users")).Update(
                TEXT("user00000000001"),
                MoveTemp(Body),
                [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& UpdateResult)
                {
                    State->bUpdateSucceeded = UpdateResult.IsSuccess();
                    State->bCompleted = true;
                });
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyAuthRecordSync(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAuthRecordPersistenceFailureTest,
    "OpenPocketBase.Client.Session.AuthRecordPersistenceFailureIsAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAuthRecordPersistenceFailureTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAuthRecordSyncState, ESPMode::ThreadSafe> State =
        MakeShared<FAuthRecordSyncState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    const TSharedRef<FFailingUpdateSecureStore, ESPMode::ThreadSafe> SecureStore =
        MakeShared<FFailingUpdateSecureStore, ESPMode::ThreadSafe>();

    FOpenPocketBaseTransportScript Login;
    Login.Response.bTransportSucceeded = true;
    Login.Response.HttpStatus = 200;
    Login.Response.Body = AuthRecordToUtf8(
        TEXT("{\"token\":\"header.payload.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\",\"displayName\":\"Player\"}}"));
    State->Transport->Enqueue(MoveTemp(Login));

    FOpenPocketBaseTransportScript Update;
    Update.Response.bTransportSucceeded = true;
    Update.Response.HttpStatus = 200;
    Update.Response.Body = AuthRecordToUtf8(
        TEXT("{\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\",\"displayName\":\"Updated Player\"}"));
    State->Transport->Enqueue(MoveTemp(Update));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::RequireSecureStorage;
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(
        Config,
        State->Transport.ToSharedRef(),
        SecureStore,
        Error);
    State->Client->OnSessionChanged().AddLambda(
        [State](const FOpenPocketBaseSessionSnapshot& Snapshot)
        {
            State->Events.Add(Snapshot);
        });
    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& LoginResult)
        {
            if (!LoginResult.IsSuccess())
            {
                State->ErrorKind = LoginResult.GetError().Kind;
                State->bCompleted = true;
                return;
            }
            FOpenPocketBaseRecordBody Body;
            Body.SetStringField(TEXT("displayName"), TEXT("Updated Player"));
            State->Client->Collection(TEXT("users")).Update(
                TEXT("user00000000001"),
                MoveTemp(Body),
                [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& UpdateResult)
                {
                    State->bUpdateSucceeded = UpdateResult.IsSuccess();
                    if (!UpdateResult.IsSuccess())
                    {
                        State->ErrorKind = UpdateResult.GetError().Kind;
                    }
                    State->bCompleted = true;
                });
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyAuthRecordPersistenceFailure(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAuthRecordRefreshRaceTest,
    "OpenPocketBase.Client.Session.AuthRecordUpdateInvalidatesOlderRefresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAuthRecordRefreshRaceTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAuthRecordSyncState, ESPMode::ThreadSafe> State =
        MakeShared<FAuthRecordSyncState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseTransportScript Login;
    Login.Response.bTransportSucceeded = true;
    Login.Response.HttpStatus = 200;
    Login.Response.Body = AuthRecordToUtf8(
        TEXT("{\"token\":\"header.payload.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\",\"displayName\":\"Player\"}}"));
    State->Transport->Enqueue(MoveTemp(Login));

    FOpenPocketBaseTransportScript Refresh;
    Refresh.bHoldCompletion = true;
    Refresh.Response.bTransportSucceeded = true;
    Refresh.Response.HttpStatus = 200;
    Refresh.Response.Body = AuthRecordToUtf8(
        TEXT("{\"token\":\"refreshed.token.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\",\"displayName\":\"Stale Player\"}}"));
    State->Transport->Enqueue(MoveTemp(Refresh));

    FOpenPocketBaseTransportScript Update;
    Update.Response.bTransportSucceeded = true;
    Update.Response.HttpStatus = 200;
    Update.Response.Body = AuthRecordToUtf8(
        TEXT("{\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\",\"displayName\":\"Updated Player\"}"));
    State->Transport->Enqueue(MoveTemp(Update));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    State->Client->OnSessionChanged().AddLambda(
        [State](const FOpenPocketBaseSessionSnapshot& Snapshot)
        {
            State->Events.Add(Snapshot);
        });
    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& LoginResult)
        {
            if (!LoginResult.IsSuccess())
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
                        State->ErrorKind = RefreshResult.GetError().Kind;
                    }
                    State->bCompleted = ++State->Callbacks == 2;
                });

            FOpenPocketBaseRecordBody Body;
            Body.SetStringField(TEXT("displayName"), TEXT("Updated Player"));
            State->Client->Collection(TEXT("users")).Update(
                TEXT("user00000000001"),
                MoveTemp(Body),
                [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& UpdateResult)
                {
                    State->bUpdateSucceeded = UpdateResult.IsSuccess();
                    State->bCompleted = ++State->Callbacks == 2;
                });
            State->Transport->CompleteNextHeld();
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyAuthRecordRefreshRace(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAuthRecordDeleteTest,
    "OpenPocketBase.Client.Session.AuthRecordDeleteClearsSession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAuthRecordDeleteTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAuthRecordSyncState, ESPMode::ThreadSafe> State =
        MakeShared<FAuthRecordSyncState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseTransportScript Login;
    Login.Response.bTransportSucceeded = true;
    Login.Response.HttpStatus = 200;
    Login.Response.Body = AuthRecordToUtf8(
        TEXT("{\"token\":\"header.payload.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(Login));

    FOpenPocketBaseTransportScript Delete;
    Delete.Response.bTransportSucceeded = true;
    Delete.Response.HttpStatus = 204;
    State->Transport->Enqueue(MoveTemp(Delete));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    State->Client->OnSessionChanged().AddLambda(
        [State](const FOpenPocketBaseSessionSnapshot& Snapshot)
        {
            State->Events.Add(Snapshot);
        });
    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& LoginResult)
        {
            if (!LoginResult.IsSuccess())
            {
                State->bCompleted = true;
                return;
            }
            State->Client->Collection(TEXT("users")).Delete(
                TEXT("user00000000001"),
                [State](TOpenPocketBaseResult<bool>&& DeleteResult)
                {
                    State->bUpdateSucceeded = DeleteResult.IsSuccess();
                    State->bCompleted = true;
                });
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyAuthRecordDelete(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBatchAuthRecordSyncTest,
    "OpenPocketBase.Client.Session.AuthRecordBatchUpdateIsAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBatchAuthRecordSyncTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAuthRecordSyncState, ESPMode::ThreadSafe> State =
        MakeShared<FAuthRecordSyncState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseTransportScript Login;
    Login.Response.bTransportSucceeded = true;
    Login.Response.HttpStatus = 200;
    Login.Response.Body = AuthRecordToUtf8(
        TEXT("{\"token\":\"header.payload.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\",\"displayName\":\"Player\"}}"));
    State->Transport->Enqueue(MoveTemp(Login));

    FOpenPocketBaseTransportScript Batch;
    Batch.Response.bTransportSucceeded = true;
    Batch.Response.HttpStatus = 200;
    Batch.Response.Body = AuthRecordToUtf8(
        TEXT("[{\"status\":200,\"body\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\",\"displayName\":\"Updated Player\"}}]"));
    State->Transport->Enqueue(MoveTemp(Batch));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    State->Client->OnSessionChanged().AddLambda(
        [State](const FOpenPocketBaseSessionSnapshot& Snapshot)
        {
            State->Events.Add(Snapshot);
        });
    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& LoginResult)
        {
            if (!LoginResult.IsSuccess())
            {
                State->bCompleted = true;
                return;
            }
            FOpenPocketBaseRecordBody Body;
            Body.SetStringField(TEXT("displayName"), TEXT("Updated Player"));
            FOpenPocketBaseBatchRequest Request;
            Request.AddUpdate(
                TEXT("users"),
                TEXT("user00000000001"),
                MoveTemp(Body));
            State->Client->SendBatch(
                MoveTemp(Request),
                [State](TOpenPocketBaseResult<FOpenPocketBaseBatchResult>&& BatchResult)
                {
                    State->bUpdateSucceeded = BatchResult.IsSuccess();
                    State->bCompleted = true;
                });
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyAuthRecordSync(State, this));
    return true;
}

#endif
