#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"
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

class FRequestKeyTransport final : public IOpenPocketBaseTransport
{
public:
    int32 SendCount = 0;
    int32 CancelCount = 0;

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        ++SendCount;
        Completions.Add(MoveTemp(OnComplete));
        return FOpenPocketBaseTransportHandle(
            [this]()
            {
                ++CancelCount;
            });
    }

private:
    TArray<FOpenPocketBaseHttpCompleteCallback> Completions;
};

struct FRetryPolicyState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FRetryPolicyTransport, ESPMode::ThreadSafe> Transport;
    bool bCompleted = false;
    bool bSucceeded = false;
    bool bMayRetry = false;
    int32 HttpStatus = 0;
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

class FVerifyResponseLimitPolicy final : public IAutomationLatentCommand
{
public:
    FVerifyResponseLimitPolicy(
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

        Test->TestFalse(TEXT("The oversized request fails"), State->bSucceeded);
        Test->TestEqual(TEXT("The oversized response is not retried"), State->Transport->SendCount, 1);
        Test->TestEqual(
            TEXT("The response-size failure is classified as serialization"),
            State->ErrorKind,
            EOpenPocketBaseErrorKind::Serialization);
        Test->TestFalse(TEXT("Retrying cannot change the configured response limit"), State->bMayRetry);
        Test->TestEqual(TEXT("The original HTTP status is preserved"), State->HttpStatus, 200);
        Test->TestTrue(
            TEXT("The error identifies the observed and configured byte counts"),
            State->ErrorMessage.Contains(
                TEXT("The response is 2048 bytes, which exceeds Max Response Bytes of 1024")));
        Test->TestFalse(
            TEXT("The error does not claim that PocketBase returned no response"),
            State->ErrorMessage.Contains(TEXT("did not return a response")));
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FRetryPolicyState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

struct FRequestKeyState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FRequestKeyTransport, ESPMode::ThreadSafe> Transport;
    FOpenPocketBaseRequestHandle RemainingHandle;
    bool bFirstCompleted = false;
    EOpenPocketBaseErrorKind FirstErrorKind = EOpenPocketBaseErrorKind::None;
};

class FVerifyRequestKeyPolicy final : public IAutomationLatentCommand
{
public:
    FVerifyRequestKeyPolicy(
        const TSharedRef<FRequestKeyState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bFirstCompleted)
        {
            return false;
        }

        Test->TestEqual(TEXT("Both keyed requests reach transport"), State->Transport->SendCount, 2);
        Test->TestEqual(TEXT("Only the previous keyed request is cancelled"), State->Transport->CancelCount, 1);
        Test->TestEqual(
            TEXT("The previous request reports cancellation"),
            State->FirstErrorKind,
            EOpenPocketBaseErrorKind::Cancelled);
        Test->TestTrue(TEXT("The replacement request remains active"), State->RemainingHandle.IsActive());
        State->RemainingHandle.Cancel();
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FRequestKeyState, ESPMode::ThreadSafe> State;
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
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRequestOptions Options;
    Options.MaxReadRetries = 1;
    Options.RetryBaseDelaySeconds = 0;
    Options.RetryMaxDelaySeconds = 0;
    Options.RetryJitterFraction = 0;
    State->Client->DynamicCollection(TEXT("tasks")).GetOne(
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
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRequestOptions Options;
    Options.MaxReadRetries = 5;
    Options.RetryBaseDelaySeconds = 0;
    Options.RetryMaxDelaySeconds = 0;
    State->Client->DynamicCollection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
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
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRequestOptions Options;
    Options.MaxResponseBytes = 1024;
    State->Client->DynamicCollection(TEXT("tasks")).GetOne(
        TEXT("large123"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bSucceeded = Result.IsSuccess();
            if (!Result.IsSuccess())
            {
                State->ErrorKind = Result.GetError().Kind;
                State->ErrorMessage = Result.GetError().Message;
                State->bMayRetry = Result.GetError().bMayRetry;
                State->HttpStatus = Result.GetError().HttpStatus;
            }
            State->bCompleted = true;
        },
        Options);

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyResponseLimitPolicy(State, this));
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
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRequestOptions Options;
    Options.MaxReadRetries = 1;
    Options.RetryBaseDelaySeconds = 30;
    Options.RetryMaxDelaySeconds = 30;
    const FOpenPocketBaseRequestHandle Handle = State->Client->DynamicCollection(TEXT("tasks")).GetOne(
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRequestKeyPolicyTest,
    "OpenPocketBase.Client.RequestPolicy.OptsIntoRequestKeyCancellation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRequestKeyPolicyTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FRequestKeyState, ESPMode::ThreadSafe> State =
        MakeShared<FRequestKeyState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FRequestKeyTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRequestOptions Options;
    Options.RequestKey = TEXT("loadout");
    Options.bCancelPreviousRequestWithSameKey = true;
    State->Client->DynamicCollection(TEXT("tasks")).GetOne(
        TEXT("first123"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->FirstErrorKind = Result.GetError().Kind;
            }
            State->bFirstCompleted = true;
        },
        Options);
    State->RemainingHandle = State->Client->DynamicCollection(TEXT("tasks")).GetOne(
        TEXT("second123"),
        [](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
        },
        Options);

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyRequestKeyPolicy(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRequestKeyDefaultPolicyTest,
    "OpenPocketBase.Client.RequestPolicy.RequestKeysAreSafeByDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRequestKeyDefaultPolicyTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FRequestKeyTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FRequestKeyTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        CreateOpenPocketBaseTestClient(Config, Transport, Error);
    if (!TestNotNull(TEXT("The client is created"), Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRequestOptions Options;
    Options.RequestKey = TEXT("shared-key");
    const FOpenPocketBaseRequestHandle First = Client->DynamicCollection(TEXT("tasks")).GetOne(
        TEXT("first123"),
        [](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
        },
        Options);
    const FOpenPocketBaseRequestHandle Second = Client->DynamicCollection(TEXT("tasks")).GetOne(
        TEXT("second123"),
        [](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
        },
        Options);

    TestEqual(TEXT("A request key does nothing without opt-in"), Transport->CancelCount, 0);
    TestTrue(TEXT("The first unkeyed request remains active"), First.IsActive());
    TestTrue(TEXT("The second unkeyed request remains active"), Second.IsActive());
    First.Cancel();
    Second.Cancel();
    Client->Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseMutationRequestKeyPolicyTest,
    "OpenPocketBase.Client.RequestPolicy.RequestKeysDoNotCancelMutations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseMutationRequestKeyPolicyTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FRequestKeyTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FRequestKeyTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        CreateOpenPocketBaseTestClient(Config, Transport, Error);
    if (!TestNotNull(TEXT("The client is created"), Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRequestOptions Options;
    Options.RequestKey = TEXT("login");
    Options.bCancelPreviousRequestWithSameKey = true;
    const FOpenPocketBaseRequestHandle First = Client->DynamicCollection(TEXT("users")).AuthWithPassword(
        TEXT("first@example.com"),
        TEXT("private-password"),
        [](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
        },
        Options);
    const FOpenPocketBaseRequestHandle Second = Client->DynamicCollection(TEXT("users")).AuthWithPassword(
        TEXT("second@example.com"),
        TEXT("private-password"),
        [](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
        },
        Options);

    TestEqual(TEXT("A mutation request key never cancels implicitly"), Transport->CancelCount, 0);
    TestTrue(TEXT("The first mutation remains active"), First.IsActive());
    TestTrue(TEXT("The second mutation remains active"), Second.IsActive());
    First.Cancel();
    Second.Cancel();
    Client->Shutdown();
    return true;
}

#endif
