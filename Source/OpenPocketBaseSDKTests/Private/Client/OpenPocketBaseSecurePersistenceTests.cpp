#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseScriptedTransport.h"
#include "SecureStorage/OpenPocketBaseSecureStore.h"

namespace
{
TArray<uint8> PersistenceToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

class FUnavailableSecureStore final : public IOpenPocketBaseSecureStore
{
public:
    virtual bool IsAvailable(FString& OutReason) const override
    {
        OutReason = TEXT("No test secure store is available.");
        return false;
    }

    virtual bool Save(
        const FString& Key,
        TConstArrayView<uint8> Value,
        FOpenPocketBaseError& OutError) override
    {
        return false;
    }

    virtual bool Load(
        const FString& Key,
        TArray<uint8>& OutValue,
        bool& bOutFound,
        FOpenPocketBaseError& OutError) override
    {
        return false;
    }

    virtual bool Delete(const FString& Key, FOpenPocketBaseError& OutError) override
    {
        return false;
    }
};

class FMemorySecureStore final : public IOpenPocketBaseSecureStore
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
        if (bFailSave)
        {
            OutError.Kind = EOpenPocketBaseErrorKind::SecureStorage;
            OutError.ServerMessage = TEXT("The injected secure store rejected the save.");
            return false;
        }
        StoredKey = Key;
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
        ++DeleteCount;
        OutError = FOpenPocketBaseError();
        return true;
    }

    FString StoredKey;
    TArray<uint8> Stored;
    int32 DeleteCount = 0;
    bool bFailSave = false;
};

struct FPersistenceTestState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport;
    TSharedPtr<FMemorySecureStore, ESPMode::ThreadSafe> SecureStore;
    TArray<FOpenPocketBaseSessionSnapshot> Events;
    EOpenPocketBaseSessionRestoreStatus RestoreStatus = EOpenPocketBaseSessionRestoreStatus::NotFound;
    EOpenPocketBaseErrorKind ErrorKind = EOpenPocketBaseErrorKind::None;
    bool bCompleted = false;
};

class FVerifySecureRoundTrip final : public IAutomationLatentCommand
{
public:
    FVerifySecureRoundTrip(
        const TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted || State->Events.Num() != 1)
        {
            return false;
        }
        Test->TestEqual(TEXT("The secure envelope restores"), State->RestoreStatus, EOpenPocketBaseSessionRestoreStatus::Restored);
        Test->TestTrue(TEXT("The restored client is authenticated"), State->Client->IsAuthenticated());
        FOpenPocketBaseSessionSnapshot Session;
        Test->TestTrue(TEXT("The restored session is available"), State->Client->GetCurrentSession(Session));
        Test->TestEqual(TEXT("Restore advances generation"), Session.AuthGeneration, static_cast<int64>(1));
        Test->TestEqual(TEXT("Restore reports persisted state"), Session.PersistenceState, EOpenPocketBaseSessionPersistenceState::Persisted);
        Test->TestEqual(TEXT("Restore publishes one event"), State->Events[0].Reason, EOpenPocketBaseSessionChangeReason::Restored);
        Test->TestEqual(TEXT("Unverified restore performs no HTTP work"), State->Transport->GetRequestCount(), 0);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifySecureFailure final : public IAutomationLatentCommand
{
public:
    FVerifySecureFailure(
        const TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest,
        const EOpenPocketBaseSessionRestoreStatus InExpectedStatus)
        : State(InState)
        , Test(InTest)
        , ExpectedStatus(InExpectedStatus)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted)
        {
            return false;
        }
        if (ExpectedStatus == EOpenPocketBaseSessionRestoreStatus::NotFound)
        {
            Test->TestEqual(TEXT("Persistence failure uses the secure-storage error"), State->ErrorKind, EOpenPocketBaseErrorKind::SecureStorage);
        }
        else
        {
            Test->TestEqual(TEXT("Restore reports the expected safe status"), State->RestoreStatus, ExpectedStatus);
            Test->TestEqual(TEXT("Invalid persisted state is deleted"), State->SecureStore->DeleteCount, 1);
        }
        Test->TestFalse(TEXT("Failure does not publish an authenticated session"), State->Client->IsAuthenticated());
        Test->TestEqual(TEXT("Failure publishes no session event"), State->Events.Num(), 0);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
    EOpenPocketBaseSessionRestoreStatus ExpectedStatus;
};

class FVerifySecureVerifiedRestore final : public IAutomationLatentCommand
{
public:
    FVerifySecureVerifiedRestore(
        const TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe>& InState,
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
        Test->TestEqual(TEXT("Server verification completes"), State->RestoreStatus, EOpenPocketBaseSessionRestoreStatus::Verified);
        Test->TestEqual(TEXT("Restore then refresh publish in order"), State->Events[0].Reason, EOpenPocketBaseSessionChangeReason::Restored);
        Test->TestEqual(TEXT("Verified refresh is second"), State->Events[1].Reason, EOpenPocketBaseSessionChangeReason::Refreshed);
        FOpenPocketBaseSessionSnapshot Session;
        Test->TestTrue(TEXT("Verified restore remains authenticated"), State->Client->GetCurrentSession(Session));
        Test->TestEqual(TEXT("Restore and refresh each advance generation"), Session.AuthGeneration, static_cast<int64>(2));
        Test->TestEqual(TEXT("Verification sends one HTTP request"), State->Transport->GetRequestCount(), 1);
        FOpenPocketBaseHttpRequest Request;
        Test->TestTrue(TEXT("The verification request is captured"), State->Transport->TryGetRequest(0, Request));
        Test->TestEqual(TEXT("Verification uses Auth Refresh"), Request.Url, FString(TEXT("https://pb.example.com/api/collections/users/auth-refresh")));
        Test->TestEqual(TEXT("Verification uses the restored token"), Request.Headers.FindRef(TEXT("Authorization")), FString(TEXT("header.payload.signature")));
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyExpiredRestore final : public IAutomationLatentCommand
{
public:
    FVerifyExpiredRestore(
        const TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe>& InState,
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
        Test->TestEqual(TEXT("Rejected verification reports expiry"), State->RestoreStatus, EOpenPocketBaseSessionRestoreStatus::Expired);
        Test->TestFalse(TEXT("An expired restore clears authentication"), State->Client->IsAuthenticated());
        Test->TestEqual(TEXT("Restore state is published first"), State->Events[0].Reason, EOpenPocketBaseSessionChangeReason::Restored);
        Test->TestEqual(TEXT("Logout state is published second"), State->Events[1].Reason, EOpenPocketBaseSessionChangeReason::LoggedOut);
        Test->TestEqual(TEXT("Restore and expiry each advance generation"), State->Events[1].AuthGeneration, static_cast<int64>(2));
        Test->TestEqual(TEXT("Expired persisted state is deleted"), State->SecureStore->DeleteCount, 1);
        Test->TestTrue(TEXT("Expired persisted state leaves no bytes"), State->SecureStore->Stored.IsEmpty());
        Test->TestEqual(TEXT("Verification sends one HTTP request"), State->Transport->GetRequestCount(), 1);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseUnavailableSecureStoreTest,
    "OpenPocketBase.Client.Session.PersistenceRejectsUnavailableStore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseUnavailableSecureStoreTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::RequireSecureStorage;
    const TSharedRef<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    const TSharedRef<FUnavailableSecureStore, ESPMode::ThreadSafe> SecureStore =
        MakeShared<FUnavailableSecureStore, ESPMode::ThreadSafe>();
    FOpenPocketBaseError Error;
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        FOpenPocketBaseClient::Create(Config, Transport, SecureStore, Error);

    TestFalse(TEXT("RequireSecureStorage rejects unavailable storage"), Client.IsValid());
    TestEqual(TEXT("The failure is classified as secure storage"), Error.Kind, EOpenPocketBaseErrorKind::SecureStorage);
    TestTrue(TEXT("The failure explains unavailability"), Error.ServerMessage.Contains(TEXT("No test secure store")));
    TestEqual(TEXT("Client creation performs no HTTP work"), Transport->GetRequestCount(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSecureRoundTripTest,
    "OpenPocketBase.Client.Session.PersistenceRoundTripsBoundEnvelope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSecureRoundTripTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe> State =
        MakeShared<FPersistenceTestState, ESPMode::ThreadSafe>();
    State->SecureStore = MakeShared<FMemorySecureStore, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseTransportScript Login;
    Login.Response.bTransportSucceeded = true;
    Login.Response.HttpStatus = 200;
    Login.Response.Body = PersistenceToUtf8(
        TEXT("{\"token\":\"header.payload.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\",\"displayName\":\"Player\"}}"));
    State->Transport->Enqueue(MoveTemp(Login));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    Config.ProfileName = TEXT("secure-test");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::RequireSecureStorage;
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(
        Config,
        State->Transport.ToSharedRef(),
        State->SecureStore.ToSharedRef(),
        Error);
    if (!TestNotNull(TEXT("The persistent client is created"), State->Client.Get()))
    {
        return false;
    }

    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State, Config, Test = this](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->ErrorKind = Result.GetError().Kind;
                State->bCompleted = true;
                return;
            }

            const FUTF8ToTCHAR Converted(
                reinterpret_cast<const ANSICHAR*>(State->SecureStore->Stored.GetData()),
                State->SecureStore->Stored.Num());
            const FString Envelope(Converted.Length(), Converted.Get());
            Test->TestTrue(TEXT("The bounded envelope contains its schema"), Envelope.Contains(TEXT("\"schema\"")));
            Test->TestTrue(TEXT("The bounded envelope binds its origin"), Envelope.Contains(TEXT("https://pb.example.com")));
            Test->TestTrue(TEXT("The bounded envelope contains the auth token"), Envelope.Contains(TEXT("header.payload.signature")));
            Test->TestFalse(TEXT("The envelope never contains the password"), Envelope.Contains(TEXT("private-password")));

            State->Client->Shutdown();
            State->Client.Reset();
            State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
            FOpenPocketBaseError RestoreCreateError;
            State->Client = FOpenPocketBaseClient::Create(
                Config,
                State->Transport.ToSharedRef(),
                State->SecureStore.ToSharedRef(),
                RestoreCreateError);
            if (!State->Client.IsValid())
            {
                State->ErrorKind = RestoreCreateError.Kind;
                State->bCompleted = true;
                return;
            }
            State->Client->OnSessionChanged().AddLambda(
                [State](const FOpenPocketBaseSessionSnapshot& Snapshot)
                {
                    State->Events.Add(Snapshot);
                });
            State->Client->RestoreSession(
                false,
                [State](TOpenPocketBaseResult<FOpenPocketBaseSessionRestoreResult>&& RestoreResult)
                {
                    if (RestoreResult.IsSuccess())
                    {
                        State->RestoreStatus = RestoreResult.GetValue().Status;
                    }
                    else
                    {
                        State->ErrorKind = RestoreResult.GetError().Kind;
                    }
                    State->bCompleted = true;
                });
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifySecureRoundTrip(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSecureSaveFailureTest,
    "OpenPocketBase.Client.Session.PersistenceSaveFailureIsAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSecureSaveFailureTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe> State =
        MakeShared<FPersistenceTestState, ESPMode::ThreadSafe>();
    State->SecureStore = MakeShared<FMemorySecureStore, ESPMode::ThreadSafe>();
    State->SecureStore->bFailSave = true;
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseTransportScript Login;
    Login.Response.bTransportSucceeded = true;
    Login.Response.HttpStatus = 200;
    Login.Response.Body = PersistenceToUtf8(
        TEXT("{\"token\":\"header.payload.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(Login));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::RequireSecureStorage;
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(
        Config,
        State->Transport.ToSharedRef(),
        State->SecureStore.ToSharedRef(),
        Error);
    State->Client->OnSessionChanged().AddLambda(
        [State](const FOpenPocketBaseSessionSnapshot& Snapshot)
        {
            State->Events.Add(Snapshot);
        });
    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->ErrorKind = Result.GetError().Kind;
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifySecureFailure(
        State,
        this,
        EOpenPocketBaseSessionRestoreStatus::NotFound));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseCorruptEnvelopeTest,
    "OpenPocketBase.Client.Session.PersistenceDeletesCorruptEnvelope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseCorruptEnvelopeTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe> State =
        MakeShared<FPersistenceTestState, ESPMode::ThreadSafe>();
    State->SecureStore = MakeShared<FMemorySecureStore, ESPMode::ThreadSafe>();
    State->SecureStore->Stored = PersistenceToUtf8(TEXT("{not-valid-json"));
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::RequireSecureStorage;
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(
        Config,
        State->Transport.ToSharedRef(),
        State->SecureStore.ToSharedRef(),
        Error);
    State->Client->OnSessionChanged().AddLambda(
        [State](const FOpenPocketBaseSessionSnapshot& Snapshot)
        {
            State->Events.Add(Snapshot);
        });
    State->Client->RestoreSession(
        false,
        [State](TOpenPocketBaseResult<FOpenPocketBaseSessionRestoreResult>&& Result)
        {
            if (Result.IsSuccess())
            {
                State->RestoreStatus = Result.GetValue().Status;
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifySecureFailure(
        State,
        this,
        EOpenPocketBaseSessionRestoreStatus::Corrupt));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseWrongOriginEnvelopeTest,
    "OpenPocketBase.Client.Session.PersistenceRejectsWrongOriginEnvelope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseWrongOriginEnvelopeTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe> State =
        MakeShared<FPersistenceTestState, ESPMode::ThreadSafe>();
    State->SecureStore = MakeShared<FMemorySecureStore, ESPMode::ThreadSafe>();
    State->SecureStore->Stored = PersistenceToUtf8(
        TEXT("{\"schema\":1,\"origin\":\"https://other.example.com\",")
        TEXT("\"profile\":\"secure-test\",\"authCollection\":\"users\",")
        TEXT("\"token\":\"header.payload.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    Config.ProfileName = TEXT("secure-test");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::RequireSecureStorage;
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(
        Config,
        State->Transport.ToSharedRef(),
        State->SecureStore.ToSharedRef(),
        Error);
    State->Client->OnSessionChanged().AddLambda(
        [State](const FOpenPocketBaseSessionSnapshot& Snapshot)
        {
            State->Events.Add(Snapshot);
        });
    State->Client->RestoreSession(
        false,
        [State](TOpenPocketBaseResult<FOpenPocketBaseSessionRestoreResult>&& Result)
        {
            if (Result.IsSuccess())
            {
                State->RestoreStatus = Result.GetValue().Status;
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifySecureFailure(
        State,
        this,
        EOpenPocketBaseSessionRestoreStatus::PolicyRejected));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseVersionedEnvelopeTest,
    "OpenPocketBase.Client.Session.PersistenceDeletesUnknownEnvelopeVersion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseVersionedEnvelopeTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe> State =
        MakeShared<FPersistenceTestState, ESPMode::ThreadSafe>();
    State->SecureStore = MakeShared<FMemorySecureStore, ESPMode::ThreadSafe>();
    State->SecureStore->Stored = PersistenceToUtf8(
        TEXT("{\"schema\":2,\"origin\":\"https://pb.example.com\",")
        TEXT("\"profile\":\"secure-test\",\"authCollection\":\"users\",")
        TEXT("\"token\":\"header.payload.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    Config.ProfileName = TEXT("secure-test");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::RequireSecureStorage;
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(
        Config,
        State->Transport.ToSharedRef(),
        State->SecureStore.ToSharedRef(),
        Error);
    State->Client->RestoreSession(
        false,
        [State](TOpenPocketBaseResult<FOpenPocketBaseSessionRestoreResult>&& Result)
        {
            if (Result.IsSuccess())
            {
                State->RestoreStatus = Result.GetValue().Status;
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifySecureFailure(
        State,
        this,
        EOpenPocketBaseSessionRestoreStatus::Corrupt));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseOversizedEnvelopeTest,
    "OpenPocketBase.Client.Session.PersistenceDeletesOversizedEnvelope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseOversizedEnvelopeTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe> State =
        MakeShared<FPersistenceTestState, ESPMode::ThreadSafe>();
    State->SecureStore = MakeShared<FMemorySecureStore, ESPMode::ThreadSafe>();
    State->SecureStore->Stored.Init(static_cast<uint8>('x'), 64 * 1024 + 1);
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::RequireSecureStorage;
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(
        Config,
        State->Transport.ToSharedRef(),
        State->SecureStore.ToSharedRef(),
        Error);
    State->Client->RestoreSession(
        false,
        [State](TOpenPocketBaseResult<FOpenPocketBaseSessionRestoreResult>&& Result)
        {
            if (Result.IsSuccess())
            {
                State->RestoreStatus = Result.GetValue().Status;
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifySecureFailure(
        State,
        this,
        EOpenPocketBaseSessionRestoreStatus::Corrupt));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseVerifiedRestoreTest,
    "OpenPocketBase.Client.Session.PersistenceVerifiesByAuthRefresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseVerifiedRestoreTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe> State =
        MakeShared<FPersistenceTestState, ESPMode::ThreadSafe>();
    State->SecureStore = MakeShared<FMemorySecureStore, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseTransportScript Login;
    Login.Response.bTransportSucceeded = true;
    Login.Response.HttpStatus = 200;
    Login.Response.Body = PersistenceToUtf8(
        TEXT("{\"token\":\"header.payload.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport->Enqueue(MoveTemp(Login));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    Config.ProfileName = TEXT("verified-restore");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::RequireSecureStorage;
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(
        Config,
        State->Transport.ToSharedRef(),
        State->SecureStore.ToSharedRef(),
        Error);
    if (!TestNotNull(TEXT("The persistent client is created"), State->Client.Get()))
    {
        return false;
    }

    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State, Config](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->ErrorKind = Result.GetError().Kind;
                State->bCompleted = true;
                return;
            }

            State->Client->Shutdown();
            State->Client.Reset();
            State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
            FOpenPocketBaseTransportScript Refresh;
            Refresh.Response.bTransportSucceeded = true;
            Refresh.Response.HttpStatus = 200;
            Refresh.Response.Body = PersistenceToUtf8(
                TEXT("{\"token\":\"verified.token.signature\",\"record\":{")
                TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
                TEXT("\"collectionName\":\"users\"}}"));
            State->Transport->Enqueue(MoveTemp(Refresh));
            FOpenPocketBaseError CreateError;
            State->Client = FOpenPocketBaseClient::Create(
                Config,
                State->Transport.ToSharedRef(),
                State->SecureStore.ToSharedRef(),
                CreateError);
            if (!State->Client.IsValid())
            {
                State->ErrorKind = CreateError.Kind;
                State->bCompleted = true;
                return;
            }
            State->Client->OnSessionChanged().AddLambda(
                [State](const FOpenPocketBaseSessionSnapshot& Snapshot)
                {
                    State->Events.Add(Snapshot);
                });
            State->Client->RestoreSession(
                true,
                [State](TOpenPocketBaseResult<FOpenPocketBaseSessionRestoreResult>&& RestoreResult)
                {
                    if (RestoreResult.IsSuccess())
                    {
                        State->RestoreStatus = RestoreResult.GetValue().Status;
                    }
                    else
                    {
                        State->ErrorKind = RestoreResult.GetError().Kind;
                    }
                    State->bCompleted = true;
                });
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifySecureVerifiedRestore(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseExpiredRestoreTest,
    "OpenPocketBase.Client.Session.PersistenceClearsExpiredVerifiedRestore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseExpiredRestoreTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FPersistenceTestState, ESPMode::ThreadSafe> State =
        MakeShared<FPersistenceTestState, ESPMode::ThreadSafe>();
    State->SecureStore = MakeShared<FMemorySecureStore, ESPMode::ThreadSafe>();
    State->SecureStore->Stored = PersistenceToUtf8(
        TEXT("{\"schema\":1,\"origin\":\"https://pb.example.com\",")
        TEXT("\"profile\":\"expired-test\",\"authCollection\":\"users\",")
        TEXT("\"token\":\"header.payload.signature\",\"record\":{")
        TEXT("\"id\":\"user00000000001\",\"collectionId\":\"users_id\",")
        TEXT("\"collectionName\":\"users\"}}"));
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseTransportScript Refresh;
    Refresh.Response.bTransportSucceeded = true;
    Refresh.Response.HttpStatus = 401;
    Refresh.Response.Body = PersistenceToUtf8(
        TEXT("{\"status\":401,\"message\":\"The request requires valid authorization token.\",\"data\":{}}"));
    State->Transport->Enqueue(MoveTemp(Refresh));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    Config.ProfileName = TEXT("expired-test");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::RequireSecureStorage;
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(
        Config,
        State->Transport.ToSharedRef(),
        State->SecureStore.ToSharedRef(),
        Error);
    State->Client->OnSessionChanged().AddLambda(
        [State](const FOpenPocketBaseSessionSnapshot& Snapshot)
        {
            State->Events.Add(Snapshot);
        });
    State->Client->RestoreSession(
        true,
        [State](TOpenPocketBaseResult<FOpenPocketBaseSessionRestoreResult>&& Result)
        {
            if (Result.IsSuccess())
            {
                State->RestoreStatus = Result.GetValue().Status;
            }
            else
            {
                State->ErrorKind = Result.GetError().Kind;
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyExpiredRestore(State, this));
    return true;
}

#if PLATFORM_MAC
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAppleKeychainProbeTest,
    "OpenPocketBase.Client.Session.Platform.AppleKeychainRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAppleKeychainProbeTest::RunTest(const FString& Parameters)
{
    const TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore =
        CreateOpenPocketBaseSecureStore();
    FString UnavailableReason;
    if (!TestTrue(TEXT("Apple Keychain reports available"), SecureStore->IsAvailable(UnavailableReason)))
    {
        AddError(UnavailableReason);
        return false;
    }

    const FString Key = TEXT("openpocketbase.test.") +
        FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const TArray<uint8> Expected = PersistenceToUtf8(TEXT("ephemeral-keychain-probe"));
    FOpenPocketBaseError Error;
    bool bSaved = SecureStore->Save(Key, Expected, Error);
    TestTrue(TEXT("Apple Keychain saves a bounded probe"), bSaved);
    if (!bSaved)
    {
        AddError(Error.ServerMessage);
        return false;
    }

    TArray<uint8> Actual;
    bool bFound = false;
    const bool bLoaded = SecureStore->Load(Key, Actual, bFound, Error);
    TestTrue(TEXT("Apple Keychain loads the probe"), bLoaded);
    TestTrue(TEXT("Apple Keychain finds the probe"), bFound);
    TestTrue(TEXT("Apple Keychain preserves the probe bytes"), Actual == Expected);

    const bool bDeleted = SecureStore->Delete(Key, Error);
    TestTrue(TEXT("Apple Keychain deletes the probe"), bDeleted);
    Actual.Reset();
    bFound = true;
    TestTrue(TEXT("Apple Keychain accepts a post-delete load"), SecureStore->Load(Key, Actual, bFound, Error));
    TestFalse(TEXT("Apple Keychain leaves no probe behind"), bFound);
    return bLoaded && bDeleted;
}
#endif

#endif
