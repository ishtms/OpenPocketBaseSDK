#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseScriptedTransport.h"

namespace
{
TArray<uint8> ScriptedToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

struct FScriptedLifecycleState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport;
    int32 CallbackCount = 0;
    EOpenPocketBaseErrorKind ErrorKind = EOpenPocketBaseErrorKind::None;
};

struct FScriptedErrorState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport;
    int32 CallbackCount = 0;
    EOpenPocketBaseErrorKind TransportErrorKind = EOpenPocketBaseErrorKind::None;
    EOpenPocketBaseErrorKind TimeoutErrorKind = EOpenPocketBaseErrorKind::None;
    bool bRanOnGameThread = true;
};

class FVerifyScriptedLifecycle final : public IAutomationLatentCommand
{
public:
    FVerifyScriptedLifecycle(
        const TSharedRef<FScriptedLifecycleState, ESPMode::ThreadSafe>& InState,
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

        Test->TestEqual(TEXT("A late transport callback cannot complete twice"), State->CallbackCount, 1);
        Test->TestEqual(
            TEXT("Explicit cancellation wins the race"),
            State->ErrorKind,
            EOpenPocketBaseErrorKind::Cancelled);
        Test->TestEqual(TEXT("The scripted transport observes one cancellation"), State->Transport->GetCancelCount(), 1);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FScriptedLifecycleState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyScriptedErrors final : public IAutomationLatentCommand
{
public:
    FVerifyScriptedErrors(
        const TSharedRef<FScriptedErrorState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (State->CallbackCount < 2)
        {
            return false;
        }

        Test->TestEqual(TEXT("Both failures complete exactly once"), State->CallbackCount, 2);
        Test->TestEqual(
            TEXT("A transport failure keeps its portable kind"),
            State->TransportErrorKind,
            EOpenPocketBaseErrorKind::Transport);
        Test->TestEqual(
            TEXT("A timeout keeps its portable kind"),
            State->TimeoutErrorKind,
            EOpenPocketBaseErrorKind::Timeout);
        Test->TestTrue(TEXT("Scripted failures reach consumers on the game thread"), State->bRanOnGameThread);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FScriptedErrorState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyScriptedTeardown final : public IAutomationLatentCommand
{
public:
    FVerifyScriptedTeardown(
        const TSharedRef<FScriptedLifecycleState, ESPMode::ThreadSafe>& InState,
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

        Test->TestEqual(TEXT("Teardown completes the request once"), State->CallbackCount, 1);
        Test->TestEqual(
            TEXT("Teardown reports cancellation"),
            State->ErrorKind,
            EOpenPocketBaseErrorKind::Cancelled);
        Test->TestEqual(TEXT("Teardown cancels the transport once"), State->Transport->GetCancelCount(), 1);
        Test->TestFalse(TEXT("Cancelled held work is discarded"), State->Transport->CompleteNextHeld());
        return true;
    }

private:
    TSharedRef<FScriptedLifecycleState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseScriptedChunksTest,
    "OpenPocketBase.Editor.Mock.ScriptsChunksAndSuccess",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseScriptedChunksTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseTransportScript Step;
    Step.Chunks.Add(ScriptedToUtf8(TEXT("first")));
    Step.Chunks.Add(ScriptedToUtf8(TEXT("second")));
    Step.Response.bTransportSucceeded = true;
    Step.Response.HttpStatus = 204;
    Transport->Enqueue(MoveTemp(Step));

    TArray<uint8> ReceivedBytes;
    bool bCompleted = false;
    FOpenPocketBaseHttpRequest Request;
    Request.Method = TEXT("GET");
    Request.Url = TEXT("https://pb.example.com/api/realtime");
    Request.bStreamResponse = true;
    Transport->Send(
        MoveTemp(Request),
        [&ReceivedBytes](TArrayView<const uint8> Chunk)
        {
            ReceivedBytes.Append(Chunk.GetData(), Chunk.Num());
        },
        [&bCompleted](FOpenPocketBaseHttpResponse&& Response)
        {
            bCompleted = Response.HttpStatus == 204;
        });

    TestEqual(TEXT("Both chunks are delivered in order"), ReceivedBytes, ScriptedToUtf8(TEXT("firstsecond")));
    TestTrue(TEXT("The scripted response completes"), bCompleted);
    TestEqual(TEXT("The request is captured"), Transport->GetRequestCount(), 1);
    FOpenPocketBaseHttpRequest CapturedRequest;
    TestTrue(TEXT("The captured request is available"), Transport->TryGetRequest(0, CapturedRequest));
    TestEqual(TEXT("The captured method is retained"), CapturedRequest.Method, FString(TEXT("GET")));
    TestEqual(
        TEXT("The captured URL is retained"),
        CapturedRequest.Url,
        FString(TEXT("https://pb.example.com/api/realtime")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseScriptedErrorsTest,
    "OpenPocketBase.Editor.Mock.ScriptsTransportErrorAndTimeout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseScriptedErrorsTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FScriptedErrorState, ESPMode::ThreadSafe> State =
        MakeShared<FScriptedErrorState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseTransportScript TransportFailure;
    TransportFailure.Response.ErrorMessage = TEXT("Connection failed.");
    State->Transport->Enqueue(MoveTemp(TransportFailure));

    FOpenPocketBaseTransportScript Timeout;
    Timeout.Response.bTimedOut = true;
    Timeout.Response.ErrorMessage = TEXT("Request timed out.");
    State->Transport->Enqueue(MoveTemp(Timeout));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRequestOptions Options;
    Options.bRetryEligibleReads = false;
    State->Client->Collection(TEXT("tasks")).GetOne(
        TEXT("transport-error"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            ++State->CallbackCount;
            State->bRanOnGameThread &= IsInGameThread();
            if (!Result.IsSuccess())
            {
                State->TransportErrorKind = Result.GetError().Kind;
            }
        },
        Options);
    State->Client->Collection(TEXT("tasks")).GetOne(
        TEXT("timeout"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            ++State->CallbackCount;
            State->bRanOnGameThread &= IsInGameThread();
            if (!Result.IsSuccess())
            {
                State->TimeoutErrorKind = Result.GetError().Kind;
            }
        },
        Options);

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyScriptedErrors(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseScriptedLateCallbackTest,
    "OpenPocketBase.Editor.Mock.ScriptsCancelAndLateCompletion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseScriptedLateCallbackTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FScriptedLifecycleState, ESPMode::ThreadSafe> State =
        MakeShared<FScriptedLifecycleState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseTransportScript Step;
    Step.bHoldCompletion = true;
    Step.bCompleteAfterCancel = true;
    Step.Response.bTransportSucceeded = true;
    Step.Response.HttpStatus = 200;
    Step.Response.Body = ScriptedToUtf8(
        TEXT("{\"id\":\"late123\",\"collectionId\":\"tasks_id\",\"collectionName\":\"tasks\"}"));
    State->Transport->Enqueue(MoveTemp(Step));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    const FOpenPocketBaseRequestHandle Handle = State->Client->Collection(TEXT("tasks")).GetOne(
        TEXT("late123"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            ++State->CallbackCount;
            if (!Result.IsSuccess())
            {
                State->ErrorKind = Result.GetError().Kind;
            }
        });
    Handle.Cancel();
    TestTrue(TEXT("The held callback is available"), State->Transport->CompleteNextHeld());

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyScriptedLifecycle(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseScriptedTeardownTest,
    "OpenPocketBase.Editor.Mock.ScriptsClientTeardown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseScriptedTeardownTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FScriptedLifecycleState, ESPMode::ThreadSafe> State =
        MakeShared<FScriptedLifecycleState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseTransportScript Step;
    Step.bHoldCompletion = true;
    Step.Response.bTransportSucceeded = true;
    Step.Response.HttpStatus = 200;
    State->Transport->Enqueue(MoveTemp(Step));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    State->Client->Collection(TEXT("tasks")).GetOne(
        TEXT("shutdown"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            ++State->CallbackCount;
            if (!Result.IsSuccess())
            {
                State->ErrorKind = Result.GetError().Kind;
            }
        });
    State->Client->Shutdown();

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyScriptedTeardown(State, this));
    return true;
}

#endif
