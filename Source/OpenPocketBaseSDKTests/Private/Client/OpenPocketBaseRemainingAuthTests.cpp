#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "OpenPocketBaseAuthentication.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"

namespace
{
TArray<uint8> RemainingAuthUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

class FRemainingAuthTransport final : public IOpenPocketBaseTransport
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

    void AddResponse(const int32 Status, const FString& Json)
    {
        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = Status;
        Response.Body = RemainingAuthUtf8(Json);
        Responses.Add(MoveTemp(Response));
    }

    TArray<FOpenPocketBaseHttpRequest> Requests;
    TArray<FOpenPocketBaseHttpResponse> Responses;
};

FString RemainingAuthBody(const FOpenPocketBaseHttpRequest& Request)
{
    const FUTF8ToTCHAR Converted(
        reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()),
        Request.Body.Num());
    return FString(Converted.Length(), Converted.Get());
}

struct FRemainingAuthState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FRemainingAuthTransport, ESPMode::ThreadSafe> Transport;
    bool bCompleted = false;
    bool bBoundedIdChecked = false;
    bool bSucceeded = true;
    bool bMethodsTyped = false;
    FString MfaId;
    FString OtpId;
};

class FVerifyRemainingAuth final : public IAutomationLatentCommand
{
public:
    FVerifyRemainingAuth(
        const TSharedRef<FRemainingAuthState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted || !State->bBoundedIdChecked)
        {
            return false;
        }

        Test->TestTrue(TEXT("The remaining auth flow succeeds"), State->bSucceeded);
        Test->TestTrue(TEXT("An oversized MFA continuation is rejected locally"),
            State->bBoundedIdChecked);
        Test->TestTrue(TEXT("Auth-method discovery returns typed data"), State->bMethodsTyped);
        Test->TestEqual(TEXT("The MFA challenge is returned separately"),
            State->MfaId, FString(TEXT("mfa00000000001")));
        Test->TestEqual(TEXT("The OTP request ID is returned separately"),
            State->OtpId, FString(TEXT("otp00000000001")));
        Test->TestEqual(TEXT("The flow sends four requests"), State->Transport->Requests.Num(), 4);
        Test->TestTrue(TEXT("Auth methods uses the pinned route"),
            State->Transport->Requests[0].Url.Contains(
                TEXT("/api/collections/sdk_users/auth-methods?fields=")));
        Test->TestTrue(TEXT("Password auth uses the pinned route"),
            State->Transport->Requests[1].Url.EndsWith(TEXT("/api/collections/sdk_users/auth-with-password")));
        Test->TestTrue(TEXT("OTP request uses the pinned route"),
            State->Transport->Requests[2].Url.EndsWith(TEXT("/api/collections/sdk_users/request-otp")));
        Test->TestTrue(TEXT("OTP exchange uses the pinned route"),
            State->Transport->Requests[3].Url.EndsWith(TEXT("/api/collections/sdk_users/auth-with-otp")));
        const FString OtpBody = RemainingAuthBody(State->Transport->Requests[3]);
        Test->TestTrue(TEXT("The second factor carries the explicit MFA continuation"),
            OtpBody.Contains(TEXT("\"mfaId\"")) &&
                OtpBody.Contains(TEXT("\"mfa00000000001\"")));
        Test->TestFalse(TEXT("The transient MFA ID is not persisted in client state"),
            State->Client->GetBaseUrl().Contains(State->MfaId));
        Test->TestTrue(TEXT("Successful OTP authentication updates the shared session"),
            State->Client->IsAuthenticated());
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FRemainingAuthState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRemainingAuthTest,
    "OpenPocketBase.Client.Authentication.DiscoversMethodsAndContinuesMfaWithOtp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRemainingAuthTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FRemainingAuthState, ESPMode::ThreadSafe> State =
        MakeShared<FRemainingAuthState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FRemainingAuthTransport, ESPMode::ThreadSafe>();
    State->Transport->AddResponse(
        200,
        TEXT("{\"mfa\":{\"enabled\":true,\"duration\":1800},"
             "\"otp\":{\"enabled\":true,\"duration\":300},"
             "\"password\":{\"enabled\":true,\"identityFields\":[\"email\"]},"
             "\"oauth2\":{\"enabled\":true,\"providers\":[{\"name\":\"github\","
             "\"displayName\":\"GitHub\",\"state\":\"provider-state\","
             "\"authURL\":\"https://github.com/login/oauth/authorize?redirect_uri=\","
             "\"codeVerifier\":\"verifier\",\"codeChallenge\":\"challenge\","
             "\"codeChallengeMethod\":\"S256\"}]}}"));
    State->Transport->AddResponse(
        401,
        TEXT("{\"code\":401,\"message\":\"Failed to authenticate.\","
             "\"mfaId\":\"mfa00000000001\"}"));
    State->Transport->AddResponse(200, TEXT("{\"otpId\":\"otp00000000001\"}"));
    State->Transport->AddResponse(
        200,
        TEXT("{\"token\":\"otp.token.signature\",\"record\":{"
             "\"id\":\"user00000000001\",\"collectionId\":\"users_id\","
             "\"collectionName\":\"sdk_users\","
             "\"created\":\"2026-08-22 10:00:00.000Z\","
             "\"updated\":\"2026-08-22 10:00:00.000Z\"}}"));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    if (!TestTrue(TEXT("The client is created"), State->Client.IsValid()))
    {
        return false;
    }

    const FOpenPocketBaseCollectionService Auth = State->Client->Collection(TEXT("sdk_users"));
    FOpenPocketBaseMfaContinuation OversizedContinuation;
    OversizedContinuation.Id = FString::ChrN(257, TEXT('x'));
    Auth.AuthenticateWithOtp(
        TEXT("otp00000000001"),
        TEXT("123456"),
        MoveTemp(OversizedContinuation),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
            State->bSucceeded = State->bSucceeded && !Result.IsSuccess() &&
                Result.GetError().Kind == EOpenPocketBaseErrorKind::InvalidArgument;
            State->bBoundedIdChecked = true;
        });
    Auth.ListAuthMethods(
        [State, Auth](TOpenPocketBaseResult<FOpenPocketBaseAuthMethods>&& Methods) mutable
        {
            State->bSucceeded = State->bSucceeded && Methods.IsSuccess();
            State->bMethodsTyped = Methods.IsSuccess() && Methods.GetValue().Mfa.bEnabled &&
                Methods.GetValue().Otp.bEnabled && Methods.GetValue().Password.bEnabled &&
                Methods.GetValue().OAuth2.Providers.Num() == 1;
            Auth.AuthenticateWithPassword(
                TEXT("player@example.com"),
                TEXT("correct-horse-battery"),
                [State, Auth](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Password) mutable
                {
                    State->bSucceeded = State->bSucceeded && Password.IsSuccess() &&
                        Password.GetValue().Status == EOpenPocketBaseAuthAttemptStatus::MfaRequired;
                    if (!Password.IsSuccess())
                    {
                        State->bCompleted = true;
                        return;
                    }
                    State->MfaId = Password.GetValue().Mfa.Id;
                    Auth.RequestOtp(
                        TEXT("player@example.com"),
                        [State, Auth](TOpenPocketBaseResult<FOpenPocketBaseOtpRequest>&& Otp) mutable
                        {
                            State->bSucceeded = State->bSucceeded && Otp.IsSuccess();
                            if (!Otp.IsSuccess())
                            {
                                State->bCompleted = true;
                                return;
                            }
                            State->OtpId = Otp.GetValue().OtpId;
                            FOpenPocketBaseMfaContinuation Continuation;
                            Continuation.Id = State->MfaId;
                            Auth.AuthenticateWithOtp(
                                State->OtpId,
                                TEXT("123456"),
                                Continuation,
                                [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
                                {
                                    State->bSucceeded = State->bSucceeded && Result.IsSuccess() &&
                                        Result.GetValue().Status ==
                                            EOpenPocketBaseAuthAttemptStatus::Authenticated;
                                    State->bCompleted = true;
                                });
                        });
                });
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyRemainingAuth(State, this));
    return true;
}

#endif
