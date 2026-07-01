#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "Transport/OpenPocketBaseTransport.h"

namespace
{
TArray<uint8> PolicyToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

FOpenPocketBaseHttpResponse MakePolicyRecordResponse(const int32 Status)
{
    FOpenPocketBaseHttpResponse Response;
    Response.bTransportSucceeded = true;
    Response.HttpStatus = Status;
    if (Status == 200)
    {
        Response.Body = PolicyToUtf8(
            TEXT("{\"id\":\"retry123\",\"collectionId\":\"tasks_id\",\"collectionName\":\"tasks\","
                 "\"created\":\"2026-08-22 10:00:00.000Z\",\"updated\":\"2026-08-22 10:00:00.000Z\"}"));
    }
    return Response;
}

class FRetryPolicyTransport final : public IOpenPocketBaseTransport
{
public:
    TArray<FOpenPocketBaseHttpResponse> Responses;
    int32 SendCount = 0;

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        const int32 ResponseIndex = SendCount++;
        OnComplete(MoveTemp(Responses[ResponseIndex]));
        return {};
    }
};

struct FRetryPolicyState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FRetryPolicyTransport, ESPMode::ThreadSafe> Transport;
    bool bCompleted = false;
    bool bSucceeded = false;
    EOpenPocketBaseErrorKind ErrorKind = EOpenPocketBaseErrorKind::None;
    FString ErrorMessage;
};

class FVerifyRetryPolicy final : public IAutomationLatentCommand
{
public:
    FVerifyRetryPolicy(
        const TSharedRef<FRetryPolicyState, ESPMode::ThreadSafe>& InState,
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

        Test->TestTrue(TEXT("The read succeeds after retry"), State->bSucceeded);
        Test->TestEqual(TEXT("One eligible retry is attempted"), State->Transport->SendCount, 2);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FRetryPolicyState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyNonRetryPolicy final : public IAutomationLatentCommand
{
public:
    FVerifyNonRetryPolicy(
        const TSharedRef<FRetryPolicyState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest,
        const int32 InExpectedSendCount,
        const TCHAR* InReason)
        : State(InState)
        , Test(InTest)
        , ExpectedSendCount(InExpectedSendCount)
        , Reason(InReason)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted)
        {
            return false;
        }

        Test->TestFalse(TEXT("The request fails"), State->bSucceeded);
        Test->TestEqual(Reason, State->Transport->SendCount, ExpectedSendCount);
        Test->TestEqual(
            TEXT("The failure is classified as transport"),
            State->ErrorKind,
            EOpenPocketBaseErrorKind::Transport);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FRetryPolicyState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
    int32 ExpectedSendCount;
    FString Reason;
};

class FVerifyCancelledRetry final : public IAutomationLatentCommand
{
public:
    FVerifyCancelledRetry(
        const TSharedRef<FRetryPolicyState, ESPMode::ThreadSafe>& InState,
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

        Test->TestEqual(TEXT("The retry timer is cancelled"), State->Transport->SendCount, 1);
        Test->TestEqual(
            TEXT("Cancellation remains the terminal result"),
            State->ErrorKind,
            EOpenPocketBaseErrorKind::Cancelled);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FRetryPolicyState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseReadRetryPolicyTest,
    "OpenPocketBase.Client.RequestPolicy.RetriesEligibleReads",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseReadRetryPolicyTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FRetryPolicyState, ESPMode::ThreadSafe> State =
        MakeShared<FRetryPolicyState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FRetryPolicyTransport, ESPMode::ThreadSafe>();
    State->Transport->Responses.Add(MakePolicyRecordResponse(503));
    State->Transport->Responses.Add(MakePolicyRecordResponse(200));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRequestOptions Options;
    Options.MaxReadRetries = 1;
    Options.RetryBaseDelaySeconds = 0;
    Options.RetryMaxDelaySeconds = 0;
    Options.RetryJitterFraction = 0;
    State->Client->Collection(TEXT("tasks")).GetOne(
        TEXT("retry123"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bSucceeded = Result.IsSuccess();
            State->bCompleted = true;
        },
        Options);

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyRetryPolicy(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseMutationRetryPolicyTest,
    "OpenPocketBase.Client.RequestPolicy.DoesNotRetryPasswordAuth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseMutationRetryPolicyTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FRetryPolicyState, ESPMode::ThreadSafe> State =
        MakeShared<FRetryPolicyState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FRetryPolicyTransport, ESPMode::ThreadSafe>();
    State->Transport->Responses.Add(FOpenPocketBaseHttpResponse());

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRequestOptions Options;
    Options.MaxReadRetries = 5;
    Options.RetryBaseDelaySeconds = 0;
    Options.RetryMaxDelaySeconds = 0;
    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result)
        {
            State->bSucceeded = Result.IsSuccess();
            if (!Result.IsSuccess())
            {
                State->ErrorKind = Result.GetError().Kind;
            }
            State->bCompleted = true;
        },
        Options);

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyNonRetryPolicy(
        State,
        this,
        1,
        TEXT("Password authentication is attempted only once")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseResponseLimitPolicyTest,
    "OpenPocketBase.Client.RequestPolicy.BoundsResponseBytes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseResponseLimitPolicyTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FRetryPolicyState, ESPMode::ThreadSafe> State =
        MakeShared<FRetryPolicyState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FRetryPolicyTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseHttpResponse Response = MakePolicyRecordResponse(200);
    Response.Body.SetNumZeroed(2048);
    State->Transport->Responses.Add(MoveTemp(Response));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRequestOptions Options;
    Options.MaxResponseBytes = 1024;
    State->Client->Collection(TEXT("tasks")).GetOne(
        TEXT("large123"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bSucceeded = Result.IsSuccess();
            if (!Result.IsSuccess())
            {
                State->ErrorKind = Result.GetError().Kind;
                State->ErrorMessage = Result.GetError().ServerMessage;
            }
            State->bCompleted = true;
        },
        Options);

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyNonRetryPolicy(
        State,
        this,
        1,
        TEXT("An oversized response is not retried")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRetryCancellationPolicyTest,
    "OpenPocketBase.Client.RequestPolicy.CancelsWaitingRetry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRetryCancellationPolicyTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FRetryPolicyState, ESPMode::ThreadSafe> State =
        MakeShared<FRetryPolicyState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FRetryPolicyTransport, ESPMode::ThreadSafe>();
    State->Transport->Responses.Add(MakePolicyRecordResponse(503));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRequestOptions Options;
    Options.MaxReadRetries = 1;
    Options.RetryBaseDelaySeconds = 30;
    Options.RetryMaxDelaySeconds = 30;
    const FOpenPocketBaseRequestHandle Handle = State->Client->Collection(TEXT("tasks")).GetOne(
        TEXT("retry123"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->ErrorKind = Result.GetError().Kind;
            }
            State->bCompleted = true;
        },
        Options);
    Handle.Cancel();

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyCancelledRetry(State, this));
    return true;
}

#endif
