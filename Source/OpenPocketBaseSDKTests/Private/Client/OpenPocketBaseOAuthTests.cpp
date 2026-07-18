#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Crypto/OpenPocketBaseSha256.h"
#include "Dom/JsonObject.h"
#include "Misc/Base64.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"
#include "SecureStorage/OpenPocketBaseSecureStore.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
TArray<uint8> OAuthUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

class FOAuthClock final : public IOpenPocketBaseClock
{
public:
    virtual FDateTime UtcNow() const override
    {
        return FDateTime::FromUnixTimestamp(2000000000) + FTimespan::FromSeconds(Now);
    }

    virtual double MonotonicSeconds() const override
    {
        return Now;
    }

    virtual FOpenPocketBaseClockHandle Schedule(
        double DelaySeconds,
        TUniqueFunction<void()> Callback) override
    {
        return {};
    }

    void Advance(const double Seconds)
    {
        Now += Seconds;
    }

private:
    double Now = 1000;
};

class FOAuthTransport final : public IOpenPocketBaseTransport
{
public:
    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        Requests.Add(Request);
        FOpenPocketBaseHttpResponse Response = MoveTemp(Responses[0]);
        Responses.RemoveAt(0, EAllowShrinking::No);
        Response.RequestId = Request.RequestId;
        Response.EffectiveUrl = Request.Url;
        OnComplete(MoveTemp(Response));
        return {};
    }

    void AddJsonResponse(const int32 Status, const FString& Json)
    {
        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = Status;
        Response.Body = OAuthUtf8(Json);
        Responses.Add(MoveTemp(Response));
    }

    TArray<FOpenPocketBaseHttpRequest> Requests;
    TArray<FOpenPocketBaseHttpResponse> Responses;
};

FString OAuthBody(const FOpenPocketBaseHttpRequest& Request)
{
    const FUTF8ToTCHAR Converted(
        reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()),
        Request.Body.Num());
    return FString(Converted.Length(), Converted.Get());
}

FString ComputePkceChallenge(const FString& Verifier)
{
    const FTCHARToUTF8 Utf8(*Verifier);
    uint8 Signature[32] = {};
    OpenPocketBase::Crypto::Sha256(
        MakeArrayView(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length()),
        Signature);
    FString Challenge = FBase64::Encode(Signature, 32, EBase64Mode::UrlSafe);
    Challenge.RemoveFromEnd(TEXT("="));
    return Challenge;
}

FString AuthMethodsResponse()
{
    return TEXT("{\"mfa\":{\"enabled\":false,\"duration\":0},"
                "\"otp\":{\"enabled\":true,\"duration\":300},"
                "\"password\":{\"enabled\":true,\"identityFields\":[\"email\"]},"
                "\"oauth2\":{\"enabled\":true,\"providers\":[{\"name\":\"github\","
                "\"displayName\":\"GitHub\",\"state\":\"server-state\","
                "\"authURL\":\"https://github.com/login/oauth/authorize?client_id=client-one"
                "&redirect_uri=&state=server-state&code_challenge=server-challenge"
                "&code_challenge_method=S256\",\"codeVerifier\":\"server-verifier\","
                "\"codeChallenge\":\"server-challenge\","
                "\"codeChallengeMethod\":\"S256\"}]}}" );
}

struct FOAuthTestState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FOAuthTransport, ESPMode::ThreadSafe> Transport;
    TSharedPtr<FOAuthClock, ESPMode::ThreadSafe> Clock;
    FOpenPocketBaseOAuth2Authorization Authorization;
    bool bCompleted = false;
    bool bWrongOriginRejected = false;
    bool bWrongStateRejected = false;
    bool bExchangeSucceeded = false;
    bool bExchangeRequestShapeValid = false;
    bool bDuplicateRejected = false;
    bool bExpiredRejected = false;
    bool bTeardownRejected = false;
    bool bVerifierMatchesChallenge = false;
    FString Failure;
};

void BeginTeardownCheck(const TSharedRef<FOAuthTestState, ESPMode::ThreadSafe>& State);

void BeginExpiryCheck(const TSharedRef<FOAuthTestState, ESPMode::ThreadSafe>& State)
{
    FOpenPocketBaseOAuth2StartOptions Options;
    Options.Provider = TEXT("github");
    Options.RedirectUrl = TEXT("https://game.example.test/oauth/callback");
    State->Client->DynamicCollection(TEXT("sdk_users")).BeginOAuth2(
        MoveTemp(Options),
        [State](TOpenPocketBaseResult<FOpenPocketBaseOAuth2Authorization>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->Failure = TEXT("The expiry transaction did not start.");
                State->bCompleted = true;
                return;
            }
            State->Clock->Advance(301);
            FOpenPocketBaseOAuth2Callback Callback;
            Callback.TransactionId = Result.GetValue().TransactionId;
            Callback.CallbackUrl = Result.GetValue().RedirectUrl +
                TEXT("?state=") + Result.GetValue().State + TEXT("&code=expired-secret-code");
            State->Client->DynamicCollection(TEXT("sdk_users")).CompleteOAuth2(
                MoveTemp(Callback),
                [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Expired)
                {
                    State->bExpiredRejected = !Expired.IsSuccess() &&
                        Expired.GetError().Kind == EOpenPocketBaseErrorKind::Authentication &&
                        !Expired.GetError().ServerMessage.Contains(TEXT("expired-secret-code"));
                    BeginTeardownCheck(State);
                });
        });
}

void BeginTeardownCheck(const TSharedRef<FOAuthTestState, ESPMode::ThreadSafe>& State)
{
    FOpenPocketBaseOAuth2StartOptions Options;
    Options.Provider = TEXT("github");
    Options.RedirectUrl = TEXT("https://game.example.test/oauth/callback");
    State->Client->DynamicCollection(TEXT("sdk_users")).BeginOAuth2(
        MoveTemp(Options),
        [State](TOpenPocketBaseResult<FOpenPocketBaseOAuth2Authorization>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->Failure = TEXT("The teardown transaction did not start.");
                State->bCompleted = true;
                return;
            }
            FOpenPocketBaseOAuth2Callback Callback;
            Callback.TransactionId = Result.GetValue().TransactionId;
            Callback.CallbackUrl = Result.GetValue().RedirectUrl +
                TEXT("?state=") + Result.GetValue().State + TEXT("&code=teardown-secret-code");
            State->Client->Shutdown();
            State->Client->DynamicCollection(TEXT("sdk_users")).CompleteOAuth2(
                MoveTemp(Callback),
                [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Teardown)
                {
                    State->bTeardownRejected = !Teardown.IsSuccess() &&
                        Teardown.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled &&
                        !Teardown.GetError().ServerMessage.Contains(TEXT("teardown-secret-code"));
                    State->bCompleted = true;
                });
        });
}

class FVerifyOAuthFlow final : public IAutomationLatentCommand
{
public:
    FVerifyOAuthFlow(
        TSharedRef<FOAuthTestState, ESPMode::ThreadSafe> InState,
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
        if (!State->Failure.IsEmpty())
        {
            Test->AddError(State->Failure);
        }
        Test->TestTrue(TEXT("Wrong callback origin is rejected"), State->bWrongOriginRejected);
        Test->TestTrue(TEXT("Mismatched OAuth state is rejected"), State->bWrongStateRejected);
        Test->TestTrue(TEXT("A valid callback exchanges the code"), State->bExchangeSucceeded);
        Test->TestTrue(TEXT("The OAuth exchange uses the pinned route and body"),
            State->bExchangeRequestShapeValid);
        Test->TestTrue(TEXT("The PKCE verifier matches the public challenge"),
            State->bVerifierMatchesChallenge);
        Test->TestTrue(TEXT("A duplicate callback is rejected"), State->bDuplicateRejected);
        Test->TestTrue(TEXT("An expired transaction is rejected"), State->bExpiredRejected);
        Test->TestTrue(TEXT("A post-teardown callback is rejected"), State->bTeardownRejected);
        Test->TestEqual(TEXT("Only valid stages reach the transport"),
            State->Transport->Requests.Num(), 4);
        return true;
    }

private:
    TSharedRef<FOAuthTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

struct FOAuthCapacityTestState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FOAuthTransport, ESPMode::ThreadSafe> Transport;
    int32 CompletionCount = 0;
    int32 SuccessCount = 0;
    int32 FailureCount = 0;
    FOpenPocketBaseError LastFailure;
};

class FVerifyOAuthCapacity final : public IAutomationLatentCommand
{
public:
    FVerifyOAuthCapacity(
        TSharedRef<FOAuthCapacityTestState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (State->CompletionCount < 17)
        {
            return false;
        }
        Test->TestEqual(TEXT("Sixteen OAuth transactions are accepted"),
            State->SuccessCount, 16);
        Test->TestEqual(TEXT("The seventeenth OAuth transaction is rejected"),
            State->FailureCount, 1);
        Test->TestEqual(TEXT("The capacity error is an authentication error"),
            State->LastFailure.Kind, EOpenPocketBaseErrorKind::Authentication);
        Test->TestEqual(TEXT("Discovery remains explicit for every attempted transaction"),
            State->Transport->Requests.Num(), 17);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FOAuthCapacityTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseOAuthFlowTest,
    "OpenPocketBase.Client.Authentication.ManualOAuthPkceIsBoundedAndExactlyOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseOAuthFlowTest::RunTest(const FString& Parameters)
{
    TestEqual(
        TEXT("SHA-256 and URL-safe Base64 match the RFC 7636 PKCE vector"),
        ComputePkceChallenge(
            TEXT("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk")),
        FString(TEXT("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM")));
    TestEqual(
        TEXT("SHA-256 padding matches the two-block NIST vector"),
        ComputePkceChallenge(
            TEXT("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")),
        FString(TEXT("JI1qYdIGOLjlwCaTDD5gOaM85Flk_yFn9uzt1BnbBsE")));

    const TSharedRef<FOAuthTestState, ESPMode::ThreadSafe> State =
        MakeShared<FOAuthTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOAuthTransport, ESPMode::ThreadSafe>();
    State->Clock = MakeShared<FOAuthClock, ESPMode::ThreadSafe>();
    State->Transport->AddJsonResponse(200, AuthMethodsResponse());
    State->Transport->AddJsonResponse(
        200,
        TEXT("{\"token\":\"oauth.token.signature\",\"record\":{"
             "\"id\":\"user00000000001\",\"collectionId\":\"users_id\","
             "\"collectionName\":\"sdk_users\","
             "\"created\":\"2026-08-22 10:00:00.000Z\","
             "\"updated\":\"2026-08-22 10:00:00.000Z\"}}"));
    State->Transport->AddJsonResponse(200, AuthMethodsResponse());
    State->Transport->AddJsonResponse(200, AuthMethodsResponse());

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(
        Config,
        State->Transport.ToSharedRef(),
        CreateOpenPocketBaseSecureStore(),
        State->Clock.ToSharedRef(),
        Error);
    if (!TestTrue(TEXT("The OAuth test client is created"), State->Client.IsValid()))
    {
        return false;
    }

    FOpenPocketBaseOAuth2StartOptions Options;
    Options.Provider = TEXT("github");
    Options.RedirectUrl = TEXT("https://game.example.test/oauth/callback");
    Options.Scopes = {TEXT("read:user"), TEXT("user:email")};
    const FOpenPocketBaseDynamicCollectionService Auth = State->Client->DynamicCollection(TEXT("sdk_users"));
    Auth.BeginOAuth2(
        MoveTemp(Options),
        [State, Auth](TOpenPocketBaseResult<FOpenPocketBaseOAuth2Authorization>&& Result) mutable
        {
            if (!Result.IsSuccess())
            {
                State->Failure = TEXT("The OAuth transaction did not start.");
                State->bCompleted = true;
                return;
            }
            State->Authorization = Result.GetValue();
            if (State->Authorization.TransactionId.IsEmpty() ||
                State->Authorization.State.Len() < 43 ||
                State->Authorization.CodeChallenge.Len() < 43 ||
                State->Authorization.CodeChallengeMethod != TEXT("S256") ||
                !State->Authorization.AuthorizationUrl.Contains(State->Authorization.State) ||
                !State->Authorization.AuthorizationUrl.Contains(State->Authorization.CodeChallenge))
            {
                State->Failure = TEXT("The generated OAuth metadata is incomplete.");
                State->bCompleted = true;
                return;
            }

            FOpenPocketBaseOAuth2Callback WrongOrigin;
            WrongOrigin.TransactionId = State->Authorization.TransactionId;
            WrongOrigin.CallbackUrl = TEXT("https://attacker.example.test/oauth/callback?state=") +
                State->Authorization.State + TEXT("&code=origin-secret-code");
            Auth.CompleteOAuth2(
                MoveTemp(WrongOrigin),
                [State, Auth](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Origin) mutable
                {
                    State->bWrongOriginRejected = !Origin.IsSuccess() &&
                        Origin.GetError().Kind == EOpenPocketBaseErrorKind::Authentication &&
                        !Origin.GetError().ServerMessage.Contains(TEXT("origin-secret-code"));

                    FOpenPocketBaseOAuth2Callback WrongState;
                    WrongState.TransactionId = State->Authorization.TransactionId;
                    WrongState.CallbackUrl = State->Authorization.RedirectUrl +
                        TEXT("?state=wrong-state&code=state-secret-code");
                    Auth.CompleteOAuth2(
                        MoveTemp(WrongState),
                        [State, Auth](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Mismatch) mutable
                        {
                            State->bWrongStateRejected = !Mismatch.IsSuccess() &&
                                Mismatch.GetError().Kind == EOpenPocketBaseErrorKind::Authentication &&
                                !Mismatch.GetError().ServerMessage.Contains(TEXT("state-secret-code"));

                            FOpenPocketBaseOAuth2Callback Valid;
                            Valid.TransactionId = State->Authorization.TransactionId;
                            Valid.CallbackUrl = State->Authorization.RedirectUrl + TEXT("?state=") +
                                State->Authorization.State + TEXT("&code=valid-secret-code");
                            Valid.CreateData.SetDynamicStringField(TEXT("name"), TEXT("OAuth Player"));
                            Auth.CompleteOAuth2(
                                Valid,
                                [State, Auth, Valid](
                                    TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Exchange) mutable
                                {
                                    State->bExchangeSucceeded = Exchange.IsSuccess() &&
                                        Exchange.GetValue().Status ==
                                            EOpenPocketBaseAuthAttemptStatus::Authenticated &&
                                        State->Client->IsAuthenticated();
                                    const FString Body = OAuthBody(State->Transport->Requests[1]);
                                    TSharedPtr<FJsonObject> Object;
                                    const TSharedRef<TJsonReader<>> Reader =
                                        TJsonReaderFactory<>::Create(Body);
                                    FString Verifier;
                                    if (FJsonSerializer::Deserialize(Reader, Object) &&
                                        Object.IsValid() &&
                                        Object->TryGetStringField(TEXT("codeVerifier"), Verifier))
                                    {
                                        FString Provider;
                                        FString Code;
                                        FString RedirectUrl;
                                        const TSharedPtr<FJsonObject>* CreateData = nullptr;
                                        State->bExchangeRequestShapeValid =
                                            State->Transport->Requests[1].Method == TEXT("POST") &&
                                            State->Transport->Requests[1].Url ==
                                                TEXT("https://pb.example.test/api/collections/"
                                                     "sdk_users/auth-with-oauth2") &&
                                            !State->Transport->Requests[1].Headers.Contains(
                                                TEXT("Authorization")) &&
                                            Object->TryGetStringField(TEXT("provider"), Provider) &&
                                            Provider == TEXT("github") &&
                                            Object->TryGetStringField(TEXT("code"), Code) &&
                                            Code == TEXT("valid-secret-code") &&
                                            Object->TryGetStringField(
                                                TEXT("redirectURL"), RedirectUrl) &&
                                            RedirectUrl == State->Authorization.RedirectUrl &&
                                            Object->TryGetObjectField(
                                                TEXT("createData"), CreateData) &&
                                            CreateData != nullptr &&
                                            (*CreateData)->GetStringField(TEXT("name")) ==
                                                TEXT("OAuth Player") &&
                                            !Object->HasField(TEXT("state")) &&
                                            !Object->HasField(TEXT("transactionId"));
                                        State->bVerifierMatchesChallenge = Verifier.Len() >= 43 &&
                                            ComputePkceChallenge(Verifier) ==
                                                State->Authorization.CodeChallenge;
                                    }
                                    Auth.CompleteOAuth2(
                                        MoveTemp(Valid),
                                        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Duplicate)
                                        {
                                            State->bDuplicateRejected = !Duplicate.IsSuccess() &&
                                                Duplicate.GetError().Kind ==
                                                    EOpenPocketBaseErrorKind::Authentication &&
                                                !Duplicate.GetError().ServerMessage.Contains(
                                                    TEXT("valid-secret-code"));
                                            BeginExpiryCheck(State);
                                        });
                                });
                        });
                });
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyOAuthFlow(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseOAuthCapacityTest,
    "OpenPocketBase.Client.Authentication.ManualOAuthTransactionsAreCapacityBounded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseOAuthCapacityTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FOAuthCapacityTestState, ESPMode::ThreadSafe> State =
        MakeShared<FOAuthCapacityTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOAuthTransport, ESPMode::ThreadSafe>();
    for (int32 Index = 0; Index < 17; ++Index)
    {
        State->Transport->AddJsonResponse(200, AuthMethodsResponse());
    }

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(
        Config,
        State->Transport.ToSharedRef(),
        CreateOpenPocketBaseSecureStore(),
        MakeShared<FOAuthClock, ESPMode::ThreadSafe>(),
        Error);
    if (!TestTrue(TEXT("The capacity test client is created"), State->Client.IsValid()))
    {
        return false;
    }

    const FOpenPocketBaseDynamicCollectionService Auth =
        State->Client->DynamicCollection(TEXT("sdk_users"));
    for (int32 Index = 0; Index < 17; ++Index)
    {
        FOpenPocketBaseOAuth2StartOptions Options;
        Options.Provider = TEXT("github");
        Options.RedirectUrl = TEXT("https://game.example.test/oauth/callback");
        Auth.BeginOAuth2(
            MoveTemp(Options),
            [State](TOpenPocketBaseResult<FOpenPocketBaseOAuth2Authorization>&& Result)
            {
                ++State->CompletionCount;
                if (Result.IsSuccess())
                {
                    ++State->SuccessCount;
                }
                else
                {
                    ++State->FailureCount;
                    State->LastFailure = Result.GetError();
                }
            });
    }

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyOAuthCapacity(State, this));
    return true;
}

#endif
