#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"

namespace
{
struct FPinnedServerState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    int32 CompletionCount = 0;
    bool bAuthSucceeded = false;
    bool bGetOneSucceeded = false;
    bool bGetListSucceeded = false;
    FString AuthRecordId;
    FString RecordTitle;
    int32 ListItems = 0;
    TArray<FString> Errors;
};

class FVerifyPinnedServer final : public IAutomationLatentCommand
{
public:
    FVerifyPinnedServer(
        const TSharedRef<FPinnedServerState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (State->CompletionCount != 3)
        {
            return false;
        }

        for (const FString& Error : State->Errors)
        {
            Test->AddError(Error);
        }
        Test->TestTrue(TEXT("Password login succeeds against v0.39.11"), State->bAuthSucceeded);
        Test->TestTrue(TEXT("Get One succeeds against v0.39.11"), State->bGetOneSucceeded);
        Test->TestTrue(TEXT("Get List succeeds against v0.39.11"), State->bGetListSucceeded);
        Test->TestEqual(
            TEXT("The seeded auth record is returned"),
            State->AuthRecordId,
            FString(TEXT("user00000000001")));
        Test->TestEqual(
            TEXT("The seeded record title is returned"),
            State->RecordTitle,
            FString(TEXT("Ship the Unreal SDK")));
        Test->TestEqual(TEXT("The seeded list has one item"), State->ListItems, 1);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FPinnedServerState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

FString DescribeIntegrationError(const TCHAR* Operation, const FOpenPocketBaseError& Error)
{
    return FString::Printf(
        TEXT("%s failed: kind=%d status=%d code=%s message=%s request=%s"),
        Operation,
        static_cast<int32>(Error.Kind),
        Error.HttpStatus,
        *Error.ServerCode,
        *Error.ServerMessage,
        *Error.RequestId);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBasePinnedServerTest,
    "OpenPocketBase.Integration.V03911.MinimalVerticalSlice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBasePinnedServerTest::RunTest(const FString& Parameters)
{
    const FString BaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENPOCKETBASE_TEST_URL"));
    if (BaseUrl.IsEmpty())
    {
        AddInfo(TEXT("OPENPOCKETBASE_TEST_URL is not set; the pinned-server test was not requested."));
        return true;
    }

    const TSharedRef<FPinnedServerState, ESPMode::ThreadSafe> State =
        MakeShared<FPinnedServerState, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = BaseUrl;
    Config.ProfileName = TEXT("integration-v03911");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, Error);
    if (!TestNotNull(TEXT("The integration client is created"), State->Client.Get()))
    {
        AddError(Error.ServerMessage);
        return false;
    }

    State->Client->Collection(TEXT("sdk_users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("correct-horse-battery"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result)
        {
            State->bAuthSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                State->AuthRecordId = Result.GetValue().Record.Id;
            }
            else
            {
                State->Errors.Add(DescribeIntegrationError(TEXT("Password login"), Result.GetError()));
            }
            ++State->CompletionCount;
        });

    State->Client->Collection(TEXT("sdk_tasks")).GetOne(
        TEXT("task00000000001"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bGetOneSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                Result.GetValue().Data.JsonObject->TryGetStringField(TEXT("title"), State->RecordTitle);
            }
            else
            {
                State->Errors.Add(DescribeIntegrationError(TEXT("Get One"), Result.GetError()));
            }
            ++State->CompletionCount;
        });

    FOpenPocketBaseListOptions ListOptions;
    ListOptions.Page = 1;
    ListOptions.PerPage = 30;
    State->Client->Collection(TEXT("sdk_tasks")).GetList(
        ListOptions,
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecordPage>&& Result)
        {
            State->bGetListSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                State->ListItems = Result.GetValue().Items.Num();
            }
            else
            {
                State->Errors.Add(DescribeIntegrationError(TEXT("Get List"), Result.GetError()));
            }
            ++State->CompletionCount;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyPinnedServer(State, this));
    return true;
}

#endif
