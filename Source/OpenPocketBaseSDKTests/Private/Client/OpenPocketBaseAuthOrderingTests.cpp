#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"
#include "OpenPocketBaseScriptedTransport.h"

namespace
{
FOpenPocketBaseTransportScript LoginScript(const TCHAR* RecordId, bool bHold)
{
    const FString Json = FString::Printf(
        TEXT("{\"token\":\"test-token\",\"record\":{\"id\":\"%s\",\"collectionId\":\"users_id\",\"collectionName\":\"users\"}}"),
        RecordId);
    const FTCHARToUTF8 Utf8(*Json);
    FOpenPocketBaseTransportScript Script;
    Script.Response.bTransportSucceeded = true;
    Script.Response.HttpStatus = 200;
    Script.Response.Body.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
    Script.bHoldCompletion = bHold;
    return Script;
}

struct FLoginOrderingState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport;
    FString ExpectedUser;
    bool bCompleted = false;
    bool bRejected = false;
};

class FVerifyLoginOrdering final : public IAutomationLatentCommand
{
public:
    FVerifyLoginOrdering(TSharedRef<FLoginOrderingState, ESPMode::ThreadSafe> InState, FAutomationTestBase* InTest)
        : State(MoveTemp(InState)), Test(InTest) {}

    virtual bool Update() override
    {
        if (!State->bCompleted)
        {
            return false;
        }
        Test->TestTrue(TEXT("A superseded login is rejected"), State->bRejected);
        FOpenPocketBaseSessionSnapshot Session;
        State->Client->GetCurrentSession(Session);
        Test->TestEqual(TEXT("The newer session decision is preserved"), Session.AuthRecord.Id, State->ExpectedUser);
        Test->TestEqual(TEXT("Authentication matches the newer session decision"), Session.bAuthenticated, !State->ExpectedUser.IsEmpty());
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FLoginOrderingState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseLoginOrderingTest,
    "OpenPocketBase.Client.Session.LateLoginCannotUndoLogoutOrNewerLogin",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseLoginOrderingTest::RunTest(const FString& Parameters)
{
    for (const bool bSwitchUser : {false, true})
    {
        const TSharedRef<FLoginOrderingState, ESPMode::ThreadSafe> State = MakeShared<FLoginOrderingState, ESPMode::ThreadSafe>();
        State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
        FOpenPocketBaseClientConfig Config;
        Config.BaseUrl = TEXT("https://pb.example.test");
        Config.SessionPersistence = EOpenPocketBaseSessionPersistence::MemoryOnly;
        FOpenPocketBaseError Error;
        State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
        if (!TestTrue(TEXT("The ordering client is created"), State->Client.IsValid()))
        {
            return false;
        }
        State->Transport->Enqueue(LoginScript(TEXT("old-user"), true));
        State->Client->DynamicCollection(TEXT("users")).AuthWithPassword(
            TEXT("old@example.test"), TEXT("password"),
            [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result)
            {
                State->bRejected = !Result.IsSuccess();
                State->bCompleted = true;
            });
        if (bSwitchUser)
        {
            State->ExpectedUser = TEXT("new-user");
            State->Transport->Enqueue(LoginScript(TEXT("new-user"), false));
            State->Client->DynamicCollection(TEXT("users")).AuthWithPassword(
                TEXT("new@example.test"), TEXT("password"), [](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&&) {});
        }
        else
        {
            State->Client->Logout();
        }
        TestTrue(TEXT("The old response is released last"), State->Transport->CompleteNextHeld());
        ADD_LATENT_AUTOMATION_COMMAND(FVerifyLoginOrdering(State, this));
    }
    return true;
}

#endif
