#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseBlueprintClient.h"
#include "Transport/OpenPocketBaseTransport.h"
#include "UObject/Package.h"

namespace
{
TArray<uint8> BlueprintToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

class FBlueprintFacadeTransport final : public IOpenPocketBaseTransport
{
public:
    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = 200;
        Response.Body = BlueprintToUtf8(
            TEXT("{\"id\":\"shared123\",\"collectionId\":\"tasks_id\",\"collectionName\":\"tasks\","
                 "\"created\":\"2026-08-22 10:00:00.000Z\",\"updated\":\"2026-08-22 10:00:00.000Z\"}"));
        OnComplete(MoveTemp(Response));
        return {};
    }
};

struct FBlueprintFacadeState
{
    UOpenPocketBaseClient* Client = nullptr;
    bool bCompleted = false;
    bool bSucceeded = false;
    FString RecordId;
};

class FVerifyBlueprintFacade final : public IAutomationLatentCommand
{
public:
    FVerifyBlueprintFacade(
        const TSharedRef<FBlueprintFacadeState, ESPMode::ThreadSafe>& InState,
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

        Test->TestTrue(TEXT("The wrapper uses the native request"), State->bSucceeded);
        Test->TestEqual(TEXT("The wrapper receives the native value"), State->RecordId, FString(TEXT("shared123")));
        State->Client->Shutdown();
        State->Client->RemoveFromRoot();
        return true;
    }

private:
    TSharedRef<FBlueprintFacadeState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBlueprintFacadeTest,
    "OpenPocketBase.Blueprint.Client.WrapsNativeImplementation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBlueprintFacadeTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FBlueprintFacadeState, ESPMode::ThreadSafe> State =
        MakeShared<FBlueprintFacadeState, ESPMode::ThreadSafe>();
    const TSharedRef<FBlueprintFacadeTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FBlueprintFacadeTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = UOpenPocketBaseClient::Create(GetTransientPackage(), Config, Transport, Error);
    if (!TestNotNull(TEXT("The Blueprint wrapper is created"), State->Client))
    {
        return false;
    }
    State->Client->AddToRoot();

    State->Client->GetNativeClient()->Collection(TEXT("tasks")).GetOne(
        TEXT("shared123"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                State->RecordId = Result.GetValue().Id;
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyBlueprintFacade(State, this));
    return true;
}

#endif
