#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"
#include "Transport/OpenPocketBaseTransport.h"

namespace
{
TArray<uint8> LifecycleToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

class FPendingTransport final : public IOpenPocketBaseTransport
{
public:
    int32 CancelCount = 0;

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        Completion = MoveTemp(OnComplete);
        return FOpenPocketBaseTransportHandle(
            [this]()
            {
                ++CancelCount;
            });
    }

private:
    FOpenPocketBaseHttpCompleteCallback Completion;
};

struct FCancellationState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FPendingTransport, ESPMode::ThreadSafe> Transport;
    int32 CallbackCount = 0;
    EOpenPocketBaseErrorKind ErrorKind = EOpenPocketBaseErrorKind::None;
    bool bRanOnGameThread = false;
};

class FVerifyCancellation final : public IAutomationLatentCommand
{
public:
    FVerifyCancellation(
        const TSharedRef<FCancellationState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (State->CallbackCount == 0)
        {
            return false;
        }

        Test->TestEqual(TEXT("Cancellation completes once"), State->CallbackCount, 1);
        Test->TestEqual(TEXT("The transport is cancelled once"), State->Transport->CancelCount, 1);
        Test->TestEqual(TEXT("Cancellation is typed"), State->ErrorKind, EOpenPocketBaseErrorKind::Cancelled);
        Test->TestTrue(TEXT("Cancellation completes on the game thread"), State->bRanOnGameThread);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FCancellationState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FAuthTransport final : public IOpenPocketBaseTransport
{
public:
    FOpenPocketBaseHttpRequest LastRequest;

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        LastRequest = MoveTemp(Request);
        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = 200;
        Response.Body = LifecycleToUtf8(
            TEXT("{\"token\":\"header.payload.signature\",\"record\":{"
                 "\"id\":\"user123\",\"collectionId\":\"users_id\",\"collectionName\":\"users\","
                 "\"created\":\"2026-08-22 10:00:00.000Z\",\"updated\":\"2026-08-22 10:00:00.000Z\","
                 "\"email\":\"player@example.com\"}}"));
        OnComplete(MoveTemp(Response));
        return {};
    }
};

struct FAuthState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FAuthTransport, ESPMode::ThreadSafe> Transport;
    bool bCompleted = false;
    bool bSucceeded = false;
};

class FVerifyAuth final : public IAutomationLatentCommand
{
public:
    FVerifyAuth(
        const TSharedRef<FAuthState, ESPMode::ThreadSafe>& InState,
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

        FOpenPocketBaseRecord CurrentRecord;
        Test->TestTrue(TEXT("Password auth succeeds"), State->bSucceeded);
        Test->TestTrue(TEXT("The auth store becomes authenticated"), State->Client->IsAuthenticated());
        Test->TestTrue(TEXT("The auth record is available"), State->Client->GetCurrentAuthRecord(CurrentRecord));
        Test->TestEqual(TEXT("The auth record is current"), CurrentRecord.Id, FString(TEXT("user123")));
        Test->TestEqual(
            TEXT("The auth endpoint is collection scoped"),
            State->Transport->LastRequest.Url,
            FString(TEXT("https://pb.example.com/api/collections/users/auth-with-password")));
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAuthState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseExplicitCancellationTest,
    "OpenPocketBase.Client.Lifecycle.CancellationIsExactlyOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseExplicitCancellationTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FCancellationState, ESPMode::ThreadSafe> State =
        MakeShared<FCancellationState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FPendingTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    const FOpenPocketBaseRequestHandle Handle = State->Client->Collection(TEXT("tasks")).GetOne(
        TEXT("task123"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            ++State->CallbackCount;
            State->bRanOnGameThread = IsInGameThread();
            if (!Result.IsSuccess())
            {
                State->ErrorKind = Result.GetError().Kind;
            }
        });
    TestTrue(TEXT("The pending handle is active"), Handle.IsActive());
    Handle.Cancel();
    Handle.Cancel();
    TestFalse(TEXT("The cancelled handle is inactive"), Handle.IsActive());

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyCancellation(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBasePasswordAuthTest,
    "OpenPocketBase.Client.Authentication.PasswordUpdatesAuthStore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBasePasswordAuthTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAuthState, ESPMode::ThreadSafe> State = MakeShared<FAuthState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FAuthTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("secret-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result)
        {
            State->bSucceeded = Result.IsSuccess();
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyAuth(State, this));
    return true;
}

#endif
