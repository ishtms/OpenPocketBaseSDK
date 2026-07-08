#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Clock/OpenPocketBaseClock.h"
#include "Misc/Base64.h"
#include "OpenPocketBaseClient.h"
#include "SecureStorage/OpenPocketBaseSecureStore.h"

namespace
{
TArray<uint8> AccountUtf8(const FString& Value)
{
    const FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

FString AccountToken(const FString& RecordId, const FString& CollectionId)
{
    const FString Payload = FString::Printf(
        TEXT("{\"id\":\"%s\",\"collectionId\":\"%s\"}"),
        *RecordId,
        *CollectionId);
    const FTCHARToUTF8 Utf8(*Payload);
    FString Encoded = FBase64::Encode(
        reinterpret_cast<const uint8*>(Utf8.Get()),
        Utf8.Length(),
        EBase64Mode::UrlSafe);
    Encoded.RemoveFromEnd(TEXT("="));
    Encoded.RemoveFromEnd(TEXT("="));
    return TEXT("header.") + Encoded + TEXT(".signature");
}

class FAccountOperationsTransport final : public IOpenPocketBaseTransport
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

    void AddResponse(const int32 Status, const FString& Json = {})
    {
        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = Status;
        Response.Body = AccountUtf8(Json);
        Responses.Add(MoveTemp(Response));
    }

    TArray<FOpenPocketBaseHttpRequest> Requests;
    TArray<FOpenPocketBaseHttpResponse> Responses;
};

class FDelayedAccountOperationsTransport final : public IOpenPocketBaseTransport
{
public:
    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        Requests.Add(Request);
        Completion = MoveTemp(OnComplete);
        return FOpenPocketBaseTransportHandle([this]()
        {
            bCancelled = true;
        });
    }

    void CompleteLate()
    {
        if (!Completion)
        {
            return;
        }
        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = 200;
        Response.RequestId = Requests[0].RequestId;
        Response.EffectiveUrl = Requests[0].Url;
        Response.Body = AccountUtf8(
            TEXT("{\"page\":1,\"perPage\":1,\"totalItems\":-1,\"totalPages\":-1,"
                 "\"items\":[{\"id\":\"external00001\","
                 "\"collectionId\":\"_externalAuths\","
                 "\"collectionName\":\"_externalAuths\","
                 "\"collectionRef\":\"users_id\",\"recordRef\":\"user00000000001\","
                 "\"provider\":\"github\",\"providerId\":\"github-user-1\","
                 "\"created\":\"2026-08-22 10:00:00.000Z\","
                 "\"updated\":\"2026-08-22 10:00:00.000Z\"}]}"));
        FOpenPocketBaseHttpCompleteCallback LocalCompletion = MoveTemp(Completion);
        LocalCompletion(MoveTemp(Response));
    }

    TArray<FOpenPocketBaseHttpRequest> Requests;
    FOpenPocketBaseHttpCompleteCallback Completion;
    bool bCancelled = false;
};

class FAccountOperationsSecureStore final : public IOpenPocketBaseSecureStore
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
            OutError.ServerMessage = TEXT("The account test secure store rejected the save.");
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
    bool bFailSave = false;
};

FString AccountRequestBody(const FOpenPocketBaseHttpRequest& Request)
{
    const FUTF8ToTCHAR Converted(
        reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()),
        Request.Body.Num());
    return FString(Converted.Length(), Converted.Get());
}

struct FAccountOperationsState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FAccountOperationsTransport, ESPMode::ThreadSafe> Transport;
    bool bCompleted = false;
    bool bSucceeded = true;
    bool bVerificationUpdatedSession = false;
    bool bEmailChangeClearedSession = false;
    TArray<FOpenPocketBaseExternalAuth> ExternalAuths;
};

class FAccountOperationsFlow final
    : public TSharedFromThis<FAccountOperationsFlow, ESPMode::ThreadSafe>
{
public:
    explicit FAccountOperationsFlow(
        TSharedRef<FAccountOperationsState, ESPMode::ThreadSafe> InState)
        : State(MoveTemp(InState))
    {
    }

    void Start()
    {
        const TSharedRef<FAccountOperationsFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->Collection(TEXT("sdk_users")).AuthenticateWithPassword(
            TEXT("player@example.com"),
            TEXT("correct-horse-battery"),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
            {
                Self->State->bSucceeded = Self->State->bSucceeded && Result.IsSuccess();
                Self->RequestPasswordReset();
            });
    }

private:
    void RequestPasswordReset()
    {
        const TSharedRef<FAccountOperationsFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->Collection(TEXT("sdk_users")).RequestPasswordReset(
            TEXT("player@example.com"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->State->bSucceeded = Self->State->bSucceeded && Result.IsSuccess();
                Self->ConfirmPasswordReset();
            });
    }

    void ConfirmPasswordReset()
    {
        const TSharedRef<FAccountOperationsFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->Collection(TEXT("sdk_users")).ConfirmPasswordReset(
            TEXT("password-reset-token"),
            TEXT("replacement-password"),
            TEXT("replacement-password"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->State->bSucceeded = Self->State->bSucceeded && Result.IsSuccess();
                Self->RequestVerification();
            });
    }

    void RequestVerification()
    {
        const TSharedRef<FAccountOperationsFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->Collection(TEXT("sdk_users")).RequestVerification(
            TEXT("player@example.com"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->State->bSucceeded = Self->State->bSucceeded && Result.IsSuccess();
                Self->ConfirmVerification();
            });
    }

    void ConfirmVerification()
    {
        const TSharedRef<FAccountOperationsFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->Collection(TEXT("sdk_users")).ConfirmVerification(
            AccountToken(TEXT("user00000000001"), TEXT("users_id")),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->State->bSucceeded = Self->State->bSucceeded && Result.IsSuccess();
                FOpenPocketBaseRecord Current;
                Self->State->bVerificationUpdatedSession =
                    Self->State->Client->GetCurrentAuthRecord(Current) &&
                    Current.Data.JsonObject.IsValid() &&
                    Current.Data.JsonObject->GetBoolField(TEXT("verified"));
                Self->RequestEmailChange();
            });
    }

    void RequestEmailChange()
    {
        const TSharedRef<FAccountOperationsFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->Collection(TEXT("sdk_users")).RequestEmailChange(
            TEXT("new-player@example.com"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->State->bSucceeded = Self->State->bSucceeded && Result.IsSuccess();
                Self->ListExternalAuths();
            });
    }

    void ListExternalAuths()
    {
        const TSharedRef<FAccountOperationsFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->Collection(TEXT("sdk_users")).ListExternalAuths(
            TEXT("user00000000001"),
            [Self](TOpenPocketBaseResult<TArray<FOpenPocketBaseExternalAuth>>&& Result)
            {
                Self->State->bSucceeded = Self->State->bSucceeded && Result.IsSuccess();
                if (Result.IsSuccess())
                {
                    Self->State->ExternalAuths = MoveTemp(Result.GetValue());
                }
                Self->UnlinkExternalAuth();
            });
    }

    void UnlinkExternalAuth()
    {
        const TSharedRef<FAccountOperationsFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->Collection(TEXT("sdk_users")).UnlinkExternalAuth(
            TEXT("user00000000001"),
            TEXT("github"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->State->bSucceeded = Self->State->bSucceeded && Result.IsSuccess();
                Self->ConfirmEmailChange();
            });
    }

    void ConfirmEmailChange()
    {
        const TSharedRef<FAccountOperationsFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->Collection(TEXT("sdk_users")).ConfirmEmailChange(
            AccountToken(TEXT("user00000000001"), TEXT("users_id")),
            TEXT("correct-horse-battery"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->State->bSucceeded = Self->State->bSucceeded && Result.IsSuccess();
                Self->State->bEmailChangeClearedSession = !Self->State->Client->IsAuthenticated();
                Self->State->bCompleted = true;
            });
    }

    TSharedRef<FAccountOperationsState, ESPMode::ThreadSafe> State;
};

class FVerifyAccountOperations final : public IAutomationLatentCommand
{
public:
    FVerifyAccountOperations(
        TSharedRef<FAccountOperationsState, ESPMode::ThreadSafe> InState,
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

        Test->TestTrue(TEXT("All account operations succeed"), State->bSucceeded);
        Test->TestTrue(TEXT("Confirm verification updates the current auth record"),
            State->bVerificationUpdatedSession);
        Test->TestTrue(TEXT("Confirm email change clears the matching session"),
            State->bEmailChangeClearedSession);
        Test->TestEqual(TEXT("One linked external auth is returned"),
            State->ExternalAuths.Num(), 1);
        if (State->ExternalAuths.Num() == 1)
        {
            Test->TestEqual(TEXT("The linked provider is typed"),
                State->ExternalAuths[0].Provider, FString(TEXT("github")));
            Test->TestEqual(TEXT("The linked provider ID is typed"),
                State->ExternalAuths[0].ProviderId, FString(TEXT("github-user-1")));
        }

        Test->TestEqual(TEXT("The account flow sends ten requests"),
            State->Transport->Requests.Num(), 10);
        const TArray<FString> Routes = {
            TEXT("/auth-with-password"),
            TEXT("/request-password-reset"),
            TEXT("/confirm-password-reset"),
            TEXT("/request-verification"),
            TEXT("/confirm-verification"),
            TEXT("/request-email-change"),
            TEXT("/api/collections/_externalAuths/records?"),
            TEXT("/api/collections/_externalAuths/records?"),
            TEXT("/api/collections/_externalAuths/records/external00001"),
            TEXT("/confirm-email-change")};
        if (State->Transport->Requests.Num() == Routes.Num())
        {
            for (int32 Index = 0; Index < Routes.Num(); ++Index)
            {
                Test->TestTrue(
                    *FString::Printf(TEXT("Request %d uses the pinned route"), Index),
                    State->Transport->Requests[Index].Url.Contains(Routes[Index]));
            }
            Test->TestTrue(TEXT("Email change is authorized with the current token"),
                State->Transport->Requests[5].Headers.Contains(TEXT("Authorization")));
            Test->TestTrue(TEXT("Linked-auth listing is authorized"),
                State->Transport->Requests[6].Headers.Contains(TEXT("Authorization")));
            Test->TestTrue(TEXT("Password reset confirmation carries all required fields"),
                AccountRequestBody(State->Transport->Requests[2]).Contains(TEXT("passwordConfirm")));
            Test->TestTrue(TEXT("Email change confirmation carries the current password"),
                AccountRequestBody(State->Transport->Requests[9]).Contains(
                    TEXT("correct-horse-battery")));
        }
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAccountOperationsState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

struct FAccountUnlinkCancellationState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FDelayedAccountOperationsTransport, ESPMode::ThreadSafe> Transport;
    bool bCompleted = false;
    int32 CompletionCount = 0;
    FOpenPocketBaseError ResultError;
    bool bLateLookupCompleted = false;
};

class FVerifyAccountUnlinkCancellation final : public IAutomationLatentCommand
{
public:
    FVerifyAccountUnlinkCancellation(
        TSharedRef<FAccountUnlinkCancellationState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bLateLookupCompleted && State->Transport->bCancelled)
        {
            State->bLateLookupCompleted = true;
            State->Transport->CompleteLate();
            return false;
        }
        if (!State->bCompleted)
        {
            return false;
        }
        Test->TestEqual(TEXT("Unlink cancellation completes exactly once"),
            State->CompletionCount, 1);
        Test->TestEqual(TEXT("Unlink cancellation returns the shared cancelled error"),
            State->ResultError.Kind, EOpenPocketBaseErrorKind::Cancelled);
        Test->TestTrue(TEXT("Unlink cancellation reaches the active transport"),
            State->Transport->bCancelled);
        Test->TestEqual(TEXT("A late linked-auth lookup cannot start deletion"),
            State->Transport->Requests.Num(), 1);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAccountUnlinkCancellationState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

struct FAccountVerificationPersistenceState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FAccountOperationsTransport, ESPMode::ThreadSafe> Transport;
    TSharedPtr<FAccountOperationsSecureStore, ESPMode::ThreadSafe> SecureStore;
    bool bCompleted = false;
    bool bRejected = false;
    bool bRecordUnchanged = false;
    bool bGenerationUnchanged = false;
};

class FVerifyAccountVerificationPersistence final : public IAutomationLatentCommand
{
public:
    FVerifyAccountVerificationPersistence(
        TSharedRef<FAccountVerificationPersistenceState, ESPMode::ThreadSafe> InState,
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
        Test->TestTrue(TEXT("A secure-store failure rejects verification commit"),
            State->bRejected);
        Test->TestTrue(TEXT("A rejected verification commit leaves the record unchanged"),
            State->bRecordUnchanged);
        Test->TestTrue(TEXT("A rejected verification commit leaves generation unchanged"),
            State->bGenerationUnchanged);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAccountVerificationPersistenceState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAccountOperationsTest,
    "OpenPocketBase.Client.Authentication.AccountOperationsUsePinnedRoutesAndSyncSession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAccountOperationsTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAccountOperationsState, ESPMode::ThreadSafe> State =
        MakeShared<FAccountOperationsState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FAccountOperationsTransport, ESPMode::ThreadSafe>();
    State->Transport->AddResponse(
        200,
        TEXT("{\"token\":\"login.token.signature\",\"record\":{"
             "\"id\":\"user00000000001\",\"collectionId\":\"users_id\","
             "\"collectionName\":\"sdk_users\",\"verified\":false,"
             "\"created\":\"2026-08-22 10:00:00.000Z\","
             "\"updated\":\"2026-08-22 10:00:00.000Z\"}}"));
    State->Transport->AddResponse(204);
    State->Transport->AddResponse(204);
    State->Transport->AddResponse(204);
    State->Transport->AddResponse(204);
    State->Transport->AddResponse(204);
    State->Transport->AddResponse(
        200,
        TEXT("{\"page\":1,\"perPage\":100,\"totalItems\":-1,\"totalPages\":-1,"
             "\"items\":[{\"id\":\"external00001\","
             "\"collectionId\":\"_externalAuths\","
             "\"collectionName\":\"_externalAuths\","
             "\"collectionRef\":\"users_id\",\"recordRef\":\"user00000000001\","
             "\"provider\":\"github\",\"providerId\":\"github-user-1\","
             "\"created\":\"2026-08-22 10:00:00.000Z\","
             "\"updated\":\"2026-08-22 10:00:00.000Z\"}]}"));
    State->Transport->AddResponse(
        200,
        TEXT("{\"page\":1,\"perPage\":1,\"totalItems\":-1,\"totalPages\":-1,"
             "\"items\":[{\"id\":\"external00001\","
             "\"collectionId\":\"_externalAuths\","
             "\"collectionName\":\"_externalAuths\","
             "\"collectionRef\":\"users_id\",\"recordRef\":\"user00000000001\","
             "\"provider\":\"github\",\"providerId\":\"github-user-1\","
             "\"created\":\"2026-08-22 10:00:00.000Z\","
             "\"updated\":\"2026-08-22 10:00:00.000Z\"}]}"));
    State->Transport->AddResponse(204);
    State->Transport->AddResponse(204);

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    if (!TestTrue(TEXT("The account operations client is created"), State->Client.IsValid()))
    {
        return false;
    }

    const TSharedRef<FAccountOperationsFlow, ESPMode::ThreadSafe> Flow =
        MakeShared<FAccountOperationsFlow, ESPMode::ThreadSafe>(State);
    Flow->Start();
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyAccountOperations(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAccountUnlinkCancellationTest,
    "OpenPocketBase.Client.Authentication.AccountUnlinkCancellationRejectsLateLookup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAccountUnlinkCancellationTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAccountUnlinkCancellationState, ESPMode::ThreadSafe> State =
        MakeShared<FAccountUnlinkCancellationState, ESPMode::ThreadSafe>();
    State->Transport =
        MakeShared<FDelayedAccountOperationsTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    if (!TestTrue(TEXT("The unlink cancellation client is created"), State->Client.IsValid()))
    {
        return false;
    }

    FOpenPocketBaseRequestHandle Handle =
        State->Client->Collection(TEXT("sdk_users")).UnlinkExternalAuth(
            TEXT("user00000000001"),
            TEXT("github"),
            [State](TOpenPocketBaseResult<bool>&& Result)
            {
                ++State->CompletionCount;
                if (!Result.IsSuccess())
                {
                    State->ResultError = Result.GetError();
                }
                State->bCompleted = true;
            });
    Handle.Cancel();

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyAccountUnlinkCancellation(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAccountVerificationPersistenceTest,
    "OpenPocketBase.Client.Authentication.AccountVerificationPersistenceIsTransactional",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAccountVerificationPersistenceTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAccountVerificationPersistenceState, ESPMode::ThreadSafe> State =
        MakeShared<FAccountVerificationPersistenceState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FAccountOperationsTransport, ESPMode::ThreadSafe>();
    State->SecureStore = MakeShared<FAccountOperationsSecureStore, ESPMode::ThreadSafe>();
    State->Transport->AddResponse(
        200,
        TEXT("{\"token\":\"login.token.signature\",\"record\":{"
             "\"id\":\"user00000000001\",\"collectionId\":\"users_id\","
             "\"collectionName\":\"sdk_users\",\"verified\":false,"
             "\"created\":\"2026-08-22 10:00:00.000Z\","
             "\"updated\":\"2026-08-22 10:00:00.000Z\"}}"));
    State->Transport->AddResponse(204);

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    Config.SessionPersistence = EOpenPocketBaseSessionPersistence::RequireSecureStorage;
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(
        Config,
        State->Transport.ToSharedRef(),
        State->SecureStore.ToSharedRef(),
        CreateOpenPocketBaseClock(),
        Error);
    if (!TestTrue(TEXT("The verification persistence client is created"),
            State->Client.IsValid()))
    {
        return false;
    }

    State->Client->Collection(TEXT("sdk_users")).AuthenticateWithPassword(
        TEXT("player@example.com"),
        TEXT("correct-horse-battery"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& LoginResult)
        {
            if (!LoginResult.IsSuccess())
            {
                State->bCompleted = true;
                return;
            }
            FOpenPocketBaseSessionSnapshot Before;
            State->Client->GetCurrentSession(Before);
            State->SecureStore->bFailSave = true;
            State->Client->Collection(TEXT("sdk_users")).ConfirmVerification(
                AccountToken(TEXT("user00000000001"), TEXT("users_id")),
                [State, Before](TOpenPocketBaseResult<bool>&& Result)
                {
                    State->bRejected = !Result.IsSuccess() &&
                        Result.GetError().Kind == EOpenPocketBaseErrorKind::SecureStorage;
                    FOpenPocketBaseSessionSnapshot After;
                    State->Client->GetCurrentSession(After);
                    bool bVerified = true;
                    State->bRecordUnchanged =
                        After.AuthRecord.Data.JsonObject.IsValid() &&
                        After.AuthRecord.Data.JsonObject->TryGetBoolField(
                            TEXT("verified"), bVerified) && !bVerified;
                    State->bGenerationUnchanged =
                        After.AuthGeneration == Before.AuthGeneration;
                    State->bCompleted = true;
                });
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyAccountVerificationPersistence(State, this));
    return true;
}

#endif
