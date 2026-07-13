#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Clock/OpenPocketBaseClock.h"
#include "Dom/JsonObject.h"
#include "Misc/CoreDelegates.h"
#include "OAuth/OpenPocketBaseOAuthBrowser.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"
#include "SecureStorage/OpenPocketBaseSecureStore.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
TArray<uint8> AssistedUtf8(const FString& Value)
{
    const FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

class FAssistedOAuthBrowser final : public IOpenPocketBaseOAuthBrowser
{
public:
    virtual bool IsAvailable(FString& OutReason) const override
    {
        OutReason = TEXT("The deterministic assisted OAuth browser is available.");
        return true;
    }

    virtual bool IsPlatformFlowValidated(FString& OutReason) const override
    {
        OutReason = TEXT("The deterministic assisted OAuth flow is validated.");
        return true;
    }

    virtual bool OpenExternalAuthorizationUrl(
        const FString& Url,
        FOpenPocketBaseError& OutError) override
    {
        OpenedUrl = Url;
        OutError = FOpenPocketBaseError();
        return true;
    }

    FString OpenedUrl;
};

class FAssistedOAuthTransport final : public IOpenPocketBaseTransport
{
public:
    virtual bool IsIncrementalResponseStreamingAvailable(FString& OutReason) const override
    {
        OutReason = TEXT("The deterministic assisted OAuth transport supports streaming.");
        return true;
    }

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        const int32 RequestIndex = Requests.Add(Request);
        if (Request.Method == TEXT("GET") && Request.Url.Contains(TEXT("/auth-methods")))
        {
            CompleteJson(
                RequestIndex,
                MoveTemp(OnComplete),
                200,
                TEXT("{\"mfa\":{\"enabled\":false,\"duration\":0},"
                     "\"otp\":{\"enabled\":false,\"duration\":0},"
                     "\"password\":{\"enabled\":true,\"identityFields\":[\"email\"]},"
                     "\"oauth2\":{\"enabled\":true,\"providers\":[{"
                     "\"name\":\"github\",\"displayName\":\"GitHub\","
                     "\"state\":\"server-state\","
                     "\"authURL\":\"https://github.com/login/oauth/authorize?"
                     "client_id=client-one&redirect_uri=&state=server-state&"
                     "code_challenge=server-challenge&code_challenge_method=S256\","
                     "\"codeVerifier\":\"server-verifier\","
                     "\"codeChallenge\":\"server-challenge\","
                     "\"codeChallengeMethod\":\"S256\"}]}}"));
            return {};
        }
        if (Request.Method == TEXT("GET") && Request.Url.EndsWith(TEXT("/api/realtime")))
        {
            StreamChunk = MoveTemp(OnChunk);
            StreamComplete = MoveTemp(OnComplete);
            return FOpenPocketBaseTransportHandle([this]()
            {
                bStreamCancelled = true;
            });
        }
        if (Request.Method == TEXT("POST") && Request.Url.EndsWith(TEXT("/api/realtime")))
        {
            CompleteJson(RequestIndex, MoveTemp(OnComplete), 204, {});
            return {};
        }
        if (Request.Method == TEXT("POST") && Request.Url.EndsWith(TEXT("/auth-with-oauth2")))
        {
            CompleteJson(
                RequestIndex,
                MoveTemp(OnComplete),
                200,
                TEXT("{\"token\":\"assisted.token.signature\",\"record\":{"
                     "\"id\":\"user00000000001\",\"collectionId\":\"users_id\","
                     "\"collectionName\":\"sdk_users\","
                     "\"created\":\"2026-08-22 10:00:00.000Z\","
                     "\"updated\":\"2026-08-22 10:00:00.000Z\"}}"));
            return {};
        }
        return {};
    }

    void EmitConnect()
    {
        Emit(TEXT("id: assisted-client\nevent: PB_CONNECT\ndata: {}\n\n"));
    }

    void EmitOAuthResult()
    {
        Emit(TEXT("event: @oauth2\ndata: {\"state\":\"assisted-client\","
                  "\"code\":\"assisted-secret-code\"}\n\n"));
    }

    bool HasStream() const
    {
        return static_cast<bool>(StreamChunk);
    }

    TArray<FOpenPocketBaseHttpRequest> Requests;
    bool bStreamCancelled = false;

private:
    void Emit(const FString& Payload)
    {
        const FTCHARToUTF8 Utf8(*Payload);
        StreamChunk(TArrayView<const uint8>(
            reinterpret_cast<const uint8*>(Utf8.Get()),
            Utf8.Length()));
    }

    void CompleteJson(
        const int32 RequestIndex,
        FOpenPocketBaseHttpCompleteCallback OnComplete,
        const int32 Status,
        const FString& Json)
    {
        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = Status;
        Response.RequestId = Requests[RequestIndex].RequestId;
        Response.EffectiveUrl = Requests[RequestIndex].Url;
        Response.Body = AssistedUtf8(Json);
        OnComplete(MoveTemp(Response));
    }

    FOpenPocketBaseHttpChunkCallback StreamChunk;
    FOpenPocketBaseHttpCompleteCallback StreamComplete;
};

FString AssistedRequestBody(const FOpenPocketBaseHttpRequest& Request)
{
    const FUTF8ToTCHAR Converted(
        reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()),
        Request.Body.Num());
    return FString(Converted.Length(), Converted.Get());
}

struct FAssistedOAuthTestState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FAssistedOAuthTransport, ESPMode::ThreadSafe> Transport;
    TSharedPtr<FAssistedOAuthBrowser, ESPMode::ThreadSafe> Browser;
    FOpenPocketBaseRequestHandle Handle;
    bool bConnectEmitted = false;
    bool bHandoffEmitted = false;
    bool bCompleted = false;
    bool bSucceeded = false;
    int32 CompletionCount = 0;
    FOpenPocketBaseError Failure;
};

struct FAssistedOAuthCancellationState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FAssistedOAuthTransport, ESPMode::ThreadSafe> Transport;
    TSharedPtr<FAssistedOAuthBrowser, ESPMode::ThreadSafe> Browser;
    FOpenPocketBaseRequestHandle Handle;
    bool bConnectEmitted = false;
    bool bCancelled = false;
    bool bCompleted = false;
    int32 CompletionCount = 0;
    FOpenPocketBaseError ResultError;
};

class FDriveAssistedOAuth final : public IAutomationLatentCommand
{
public:
    FDriveAssistedOAuth(
        TSharedRef<FAssistedOAuthTestState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bConnectEmitted && State->Transport->HasStream())
        {
            State->bConnectEmitted = true;
            State->Transport->EmitConnect();
            return false;
        }
        if (!State->bHandoffEmitted && !State->Browser->OpenedUrl.IsEmpty())
        {
            State->bHandoffEmitted = true;
            Test->TestTrue(TEXT("The browser URL targets the selected provider"),
                State->Browser->OpenedUrl.StartsWith(
                    TEXT("https://github.com/login/oauth/authorize?")));
            Test->TestTrue(TEXT("The browser URL binds realtime state"),
                State->Browser->OpenedUrl.Contains(TEXT("assisted-client")));
            Test->TestTrue(TEXT("The browser URL redirects through PocketBase"),
                State->Browser->OpenedUrl.Contains(
                    TEXT("https%3A%2F%2Fpb.example.test%2Fapi%2Foauth2-redirect")));
            State->Transport->EmitOAuthResult();
            State->Transport->EmitOAuthResult();
            return false;
        }
        if (!State->bCompleted)
        {
            return false;
        }

        Test->TestTrue(TEXT("The assisted OAuth operation succeeds"), State->bSucceeded);
        Test->TestEqual(TEXT("The assisted OAuth operation completes exactly once"),
            State->CompletionCount, 1);
        Test->TestTrue(TEXT("The one-off realtime stream is cancelled"),
            State->Transport->bStreamCancelled);
        Test->TestEqual(TEXT("Discovery, realtime setup, and one exchange are sent"),
            State->Transport->Requests.Num(), 4);
        if (State->Transport->Requests.Num() == 4)
        {
            const FOpenPocketBaseHttpRequest& Exchange = State->Transport->Requests[3];
            Test->TestEqual(TEXT("The assisted exchange uses the pinned route"),
                Exchange.Url,
                FString(TEXT("https://pb.example.test/api/collections/"
                             "sdk_users/auth-with-oauth2")));
            const FString Body = AssistedRequestBody(Exchange);
            Test->TestTrue(TEXT("The handoff code reaches only the exchange body"),
                Body.Contains(TEXT("assisted-secret-code")));
            Test->TestTrue(TEXT("The server-issued verifier is used"),
                Body.Contains(TEXT("server-verifier")));
            Test->TestTrue(TEXT("The server callback URL is used"),
                Body.Contains(TEXT("https://pb.example.test/api/oauth2-redirect")));
        }
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAssistedOAuthTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FDriveAssistedOAuthCancellation final : public IAutomationLatentCommand
{
public:
    FDriveAssistedOAuthCancellation(
        TSharedRef<FAssistedOAuthCancellationState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bConnectEmitted && State->Transport->HasStream())
        {
            State->bConnectEmitted = true;
            State->Transport->EmitConnect();
            return false;
        }
        if (!State->bCancelled && !State->Browser->OpenedUrl.IsEmpty())
        {
            State->bCancelled = true;
            State->Handle.Cancel();
            State->Transport->EmitOAuthResult();
            State->Transport->EmitOAuthResult();
            return false;
        }
        if (!State->bCompleted)
        {
            return false;
        }

        Test->TestEqual(TEXT("Cancellation completes exactly once"),
            State->CompletionCount, 1);
        Test->TestEqual(TEXT("Cancellation returns the shared cancelled error"),
            State->ResultError.Kind, EOpenPocketBaseErrorKind::Cancelled);
        Test->TestTrue(TEXT("Cancellation stops the one-off realtime stream"),
            State->Transport->bStreamCancelled);
        Test->TestEqual(TEXT("A late handoff cannot start an OAuth exchange"),
            State->Transport->Requests.Num(), 3);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAssistedOAuthCancellationState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

struct FAssistedOAuthPolicyState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FAssistedOAuthTransport, ESPMode::ThreadSafe> Transport;
    TSharedPtr<FAssistedOAuthBrowser, ESPMode::ThreadSafe> Browser;
    bool bCompleted = false;
    int32 CompletionCount = 0;
    FOpenPocketBaseError ResultError;
};

struct FAssistedOAuthBackgroundState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FAssistedOAuthTransport, ESPMode::ThreadSafe> Transport;
    TSharedPtr<FAssistedOAuthBrowser, ESPMode::ThreadSafe> Browser;
    FOpenPocketBaseRequestHandle Handle;
    bool bConnectEmitted = false;
    bool bBackgrounded = false;
    bool bCompleted = false;
    int32 CompletionCount = 0;
    FOpenPocketBaseError ResultError;
};

class FVerifyAssistedOAuthPolicy final : public IAutomationLatentCommand
{
public:
    FVerifyAssistedOAuthPolicy(
        TSharedRef<FAssistedOAuthPolicyState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted)
        {
            return false;
        }

        Test->TestEqual(TEXT("The policy rejection completes exactly once"),
            State->CompletionCount, 1);
        Test->TestEqual(TEXT("The policy rejection is typed as unsupported"),
            State->ResultError.Kind, EOpenPocketBaseErrorKind::Unsupported);
        Test->TestEqual(TEXT("Disabled assisted OAuth sends no requests"),
            State->Transport->Requests.Num(), 0);
        Test->TestTrue(TEXT("Disabled assisted OAuth opens no browser"),
            State->Browser->OpenedUrl.IsEmpty());
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAssistedOAuthPolicyState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FDriveAssistedOAuthBackground final : public IAutomationLatentCommand
{
public:
    FDriveAssistedOAuthBackground(
        TSharedRef<FAssistedOAuthBackgroundState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bConnectEmitted && State->Transport->HasStream())
        {
            State->bConnectEmitted = true;
            State->Transport->EmitConnect();
            return false;
        }
        if (!State->bBackgrounded && !State->Browser->OpenedUrl.IsEmpty())
        {
            State->bBackgrounded = true;
            FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
            return false;
        }
        if (!State->bCompleted)
        {
            return false;
        }

        FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
        Test->TestEqual(TEXT("Background interruption completes exactly once"),
            State->CompletionCount, 1);
        Test->TestEqual(TEXT("Background interruption is a transport failure"),
            State->ResultError.Kind, EOpenPocketBaseErrorKind::Transport);
        Test->TestTrue(TEXT("Background interruption cancels the realtime stream"),
            State->Transport->bStreamCancelled);
        Test->TestEqual(TEXT("Background interruption cannot exchange a code"),
            State->Transport->Requests.Num(), 3);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAssistedOAuthBackgroundState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAssistedOAuthTest,
    "OpenPocketBase.Client.Authentication.AssistedOAuthCoordinatesRealtimeAndBrowser",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAssistedOAuthTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAssistedOAuthTestState, ESPMode::ThreadSafe> State =
        MakeShared<FAssistedOAuthTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FAssistedOAuthTransport, ESPMode::ThreadSafe>();
    State->Browser = MakeShared<FAssistedOAuthBrowser, ESPMode::ThreadSafe>();

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    Config.bEnableAssistedOAuth = true;
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(
        Config,
        State->Transport.ToSharedRef(),
        CreateOpenPocketBaseSecureStore(),
        CreateOpenPocketBaseClock(),
        State->Browser.ToSharedRef(),
        Error);
    if (!TestTrue(TEXT("The assisted OAuth client is created"), State->Client.IsValid()))
    {
        return false;
    }

    FOpenPocketBaseAssistedOAuth2Options Options;
    Options.Provider = TEXT("github");
    Options.Scopes = {TEXT("read:user"), TEXT("user:email")};
    Options.CreateData.SetStringField(TEXT("name"), TEXT("Assisted Player"));
    State->Handle = State->Client->Collection(TEXT("sdk_users")).AuthWithOAuth2(
        MoveTemp(Options),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
            ++State->CompletionCount;
            State->bSucceeded = Result.IsSuccess() &&
                Result.GetValue().Status == EOpenPocketBaseAuthAttemptStatus::Authenticated &&
                State->Client->IsAuthenticated();
            if (!Result.IsSuccess())
            {
                State->Failure = Result.GetError();
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FDriveAssistedOAuth(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAssistedOAuthCancellationTest,
    "OpenPocketBase.Client.Authentication.AssistedOAuthCancellationRejectsLateHandoff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAssistedOAuthCancellationTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAssistedOAuthCancellationState, ESPMode::ThreadSafe> State =
        MakeShared<FAssistedOAuthCancellationState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FAssistedOAuthTransport, ESPMode::ThreadSafe>();
    State->Browser = MakeShared<FAssistedOAuthBrowser, ESPMode::ThreadSafe>();

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    Config.bEnableAssistedOAuth = true;
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(
        Config,
        State->Transport.ToSharedRef(),
        CreateOpenPocketBaseSecureStore(),
        CreateOpenPocketBaseClock(),
        State->Browser.ToSharedRef(),
        Error);
    if (!TestTrue(TEXT("The cancellation client is created"), State->Client.IsValid()))
    {
        return false;
    }

    FOpenPocketBaseAssistedOAuth2Options Options;
    Options.Provider = TEXT("github");
    State->Handle = State->Client->Collection(TEXT("sdk_users")).AuthWithOAuth2(
        MoveTemp(Options),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
            ++State->CompletionCount;
            if (!Result.IsSuccess())
            {
                State->ResultError = Result.GetError();
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FDriveAssistedOAuthCancellation(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAssistedOAuthPolicyTest,
    "OpenPocketBase.Client.Authentication.AssistedOAuthRequiresExplicitPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAssistedOAuthPolicyTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAssistedOAuthPolicyState, ESPMode::ThreadSafe> State =
        MakeShared<FAssistedOAuthPolicyState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FAssistedOAuthTransport, ESPMode::ThreadSafe>();
    State->Browser = MakeShared<FAssistedOAuthBrowser, ESPMode::ThreadSafe>();

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(
        Config,
        State->Transport.ToSharedRef(),
        CreateOpenPocketBaseSecureStore(),
        CreateOpenPocketBaseClock(),
        State->Browser.ToSharedRef(),
        Error);
    if (!TestTrue(TEXT("The policy test client is created"), State->Client.IsValid()))
    {
        return false;
    }

    FOpenPocketBaseAssistedOAuth2Options Options;
    Options.Provider = TEXT("github");
    State->Client->Collection(TEXT("sdk_users")).AuthWithOAuth2(
        MoveTemp(Options),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
            ++State->CompletionCount;
            if (!Result.IsSuccess())
            {
                State->ResultError = Result.GetError();
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyAssistedOAuthPolicy(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAssistedOAuthBackgroundTest,
    "OpenPocketBase.Client.Authentication.AssistedOAuthFailsSafelyWhenBackgrounded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAssistedOAuthBackgroundTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAssistedOAuthBackgroundState, ESPMode::ThreadSafe> State =
        MakeShared<FAssistedOAuthBackgroundState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FAssistedOAuthTransport, ESPMode::ThreadSafe>();
    State->Browser = MakeShared<FAssistedOAuthBrowser, ESPMode::ThreadSafe>();

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    Config.bEnableAssistedOAuth = true;
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(
        Config,
        State->Transport.ToSharedRef(),
        CreateOpenPocketBaseSecureStore(),
        CreateOpenPocketBaseClock(),
        State->Browser.ToSharedRef(),
        Error);
    if (!TestTrue(TEXT("The background test client is created"), State->Client.IsValid()))
    {
        return false;
    }

    FOpenPocketBaseAssistedOAuth2Options Options;
    Options.Provider = TEXT("github");
    State->Handle = State->Client->Collection(TEXT("sdk_users")).AuthWithOAuth2(
        MoveTemp(Options),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
            ++State->CompletionCount;
            if (!Result.IsSuccess())
            {
                State->ResultError = Result.GetError();
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FDriveAssistedOAuthBackground(State, this));
    return true;
}

#endif
