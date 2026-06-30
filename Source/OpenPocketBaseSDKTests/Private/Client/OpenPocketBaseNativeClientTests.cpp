#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "Transport/OpenPocketBaseTransport.h"

namespace
{
TArray<uint8> ToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

class FImmediateTransport final : public IOpenPocketBaseTransport
{
public:
    FOpenPocketBaseHttpRequest LastRequest;
    FOpenPocketBaseHttpResponse NextResponse;

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        LastRequest = MoveTemp(Request);
        OnComplete(MoveTemp(NextResponse));
        return FOpenPocketBaseTransportHandle();
    }
};

struct FNativeClientTestState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FImmediateTransport, ESPMode::ThreadSafe> Transport;
    bool bCompleted = false;
    bool bSucceeded = false;
    bool bRanOnGameThread = false;
    FString RecordId;
};

class FVerifyNativeClientRequest final : public IAutomationLatentCommand
{
public:
    FVerifyNativeClientRequest(
        const TSharedRef<FNativeClientTestState, ESPMode::ThreadSafe>& InState,
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

        Test->TestTrue(TEXT("The request succeeds"), State->bSucceeded);
        Test->TestTrue(TEXT("The callback runs on the game thread"), State->bRanOnGameThread);
        Test->TestEqual(TEXT("The record ID is parsed"), State->RecordId, FString(TEXT("task123")));
        Test->TestEqual(
            TEXT("Path segments are encoded"),
            State->Transport->LastRequest.Url,
            FString(TEXT("https://pb.example.com/api/collections/tasks/records/record%20id")));
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FNativeClientTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

struct FListClientTestState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FImmediateTransport, ESPMode::ThreadSafe> Transport;
    bool bCompleted = false;
    bool bSucceeded = false;
    int32 ItemCount = 0;
    int64 TotalItems = 0;
};

class FVerifyListRequest final : public IAutomationLatentCommand
{
public:
    FVerifyListRequest(
        const TSharedRef<FListClientTestState, ESPMode::ThreadSafe>& InState,
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

        Test->TestTrue(TEXT("The list request succeeds"), State->bSucceeded);
        Test->TestEqual(TEXT("The list contains its records"), State->ItemCount, 1);
        Test->TestEqual(TEXT("The total is parsed"), State->TotalItems, static_cast<int64>(1));
        Test->TestTrue(
            TEXT("Pagination is sent through the shared request"),
            State->Transport->LastRequest.Url.Contains(TEXT("page=2&perPage=25")));
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FListClientTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseNativeGetOneTest,
    "OpenPocketBase.Client.Records.GetOneUsesSharedLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseNativeGetOneTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FNativeClientTestState, ESPMode::ThreadSafe> State =
        MakeShared<FNativeClientTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FImmediateTransport, ESPMode::ThreadSafe>();
    State->Transport->NextResponse.bTransportSucceeded = true;
    State->Transport->NextResponse.HttpStatus = 200;
    State->Transport->NextResponse.Body = ToUtf8(
        TEXT("{\"id\":\"task123\",\"collectionId\":\"tasks_id\",\"collectionName\":\"tasks\","
             "\"created\":\"2026-08-22 10:00:00.000Z\",\"updated\":\"2026-08-22 10:01:00.000Z\","
             "\"title\":\"First task\"}"));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");

    FOpenPocketBaseError CreateError;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), CreateError);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    State->Client->Collection(TEXT("tasks")).GetOne(
        TEXT("record id"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bRanOnGameThread = IsInGameThread();
            State->bSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                State->RecordId = Result.GetValue().Id;
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyNativeClientRequest(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseNativeGetListTest,
    "OpenPocketBase.Client.Records.GetListParsesPagination",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseNativeGetListTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FListClientTestState, ESPMode::ThreadSafe> State =
        MakeShared<FListClientTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FImmediateTransport, ESPMode::ThreadSafe>();
    State->Transport->NextResponse.bTransportSucceeded = true;
    State->Transport->NextResponse.HttpStatus = 200;
    State->Transport->NextResponse.Body = ToUtf8(
        TEXT("{\"page\":2,\"perPage\":25,\"totalItems\":1,\"totalPages\":1,\"items\":[{"
             "\"id\":\"task123\",\"collectionId\":\"tasks_id\",\"collectionName\":\"tasks\","
             "\"created\":\"2026-08-22 10:00:00.000Z\",\"updated\":\"2026-08-22 10:01:00.000Z\"}]}"));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError CreateError;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), CreateError);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseListOptions Options;
    Options.Page = 2;
    Options.PerPage = 25;
    State->Client->Collection(TEXT("tasks")).GetList(
        Options,
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecordPage>&& Result)
        {
            State->bSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                State->ItemCount = Result.GetValue().Items.Num();
                State->TotalItems = Result.GetValue().TotalItems;
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyListRequest(State, this));
    return true;
}

#endif
