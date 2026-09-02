#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"
#include "Transport/OpenPocketBaseTransport.h"

#include <type_traits>

using FExpectedEphemeralClientFactory = FOpenPocketBaseClientResult (*)(
    const FOpenPocketBaseClientConfig&,
    FString,
    FOpenPocketBaseAuthCollectionRef,
    const FOpenPocketBaseRecord&,
    FOpenPocketBaseClientDependencies);
static_assert(std::is_same_v<
    decltype(&FOpenPocketBaseClient::CreateEphemeralAuthenticated),
    FExpectedEphemeralClientFactory>);

namespace
{
TArray<uint8> NativeClientToUtf8(const FString& Value)
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
        const FString* CorrelationHeader = State->Transport->LastRequest.Headers.Find(TEXT("X-Request-Id"));
        Test->TestNotNull(TEXT("Every request has a correlation header"), CorrelationHeader);
        if (CorrelationHeader != nullptr)
        {
            Test->TestEqual(
                TEXT("The correlation header matches the transport request ID"),
                *CorrelationHeader,
                State->Transport->LastRequest.RequestId);
            FGuid ParsedRequestId;
            Test->TestTrue(
                TEXT("The correlation ID uses a sanitized GUID"),
                FGuid::ParseExact(*CorrelationHeader, EGuidFormats::DigitsWithHyphensLower, ParsedRequestId));
        }
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

struct FUnsafeSegmentTestState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FImmediateTransport, ESPMode::ThreadSafe> Transport;
    bool bCompleted = false;
    EOpenPocketBaseErrorKind ErrorKind = EOpenPocketBaseErrorKind::None;
};

class FVerifyUnsafeSegment final : public IAutomationLatentCommand
{
public:
    FVerifyUnsafeSegment(
        const TSharedRef<FUnsafeSegmentTestState, ESPMode::ThreadSafe>& InState,
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

        Test->TestEqual(
            TEXT("A traversal-like record ID is rejected"),
            State->ErrorKind,
            EOpenPocketBaseErrorKind::InvalidArgument);
        Test->TestTrue(TEXT("An unsafe request never reaches transport"), State->Transport->LastRequest.Url.IsEmpty());
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FUnsafeSegmentTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseNativeFactoryResultTest,
    "OpenPocketBase.Client.Factory.ReturnsTypedResult",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseNativeFactoryResultTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig InvalidConfig;
    const auto InvalidResult = FOpenPocketBaseClient::Create(InvalidConfig);
    TestFalse(TEXT("Invalid configuration returns a failed result"), InvalidResult.IsSuccess());
    TestEqual(
        TEXT("The failed result preserves the configuration error"),
        InvalidResult.GetError().Kind,
        EOpenPocketBaseErrorKind::InvalidArgument);

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseClientDependencies Dependencies;
    Dependencies.Transport = MakeShared<FImmediateTransport, ESPMode::ThreadSafe>();
    auto Result = FOpenPocketBaseClient::Create(Config, MoveTemp(Dependencies));
    TestTrue(TEXT("Valid configuration returns a client result"), Result.IsSuccess());
    if (Result.IsSuccess())
    {
        Result.GetValue()->Shutdown();
    }
    return true;
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
    State->Transport->NextResponse.Body = NativeClientToUtf8(
        TEXT("{\"id\":\"task123\",\"collectionId\":\"tasks_id\",\"collectionName\":\"tasks\","
             "\"created\":\"2026-08-22 10:00:00.000Z\",\"updated\":\"2026-08-22 10:01:00.000Z\","
             "\"title\":\"First task\"}"));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");

    FOpenPocketBaseError CreateError;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), CreateError);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    State->Client->DynamicCollection(TEXT("tasks")).GetOne(
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
    State->Transport->NextResponse.Body = NativeClientToUtf8(
        TEXT("{\"page\":2,\"perPage\":25,\"totalItems\":1,\"totalPages\":1,\"items\":[{"
             "\"id\":\"task123\",\"collectionId\":\"tasks_id\",\"collectionName\":\"tasks\","
             "\"created\":\"2026-08-22 10:00:00.000Z\",\"updated\":\"2026-08-22 10:01:00.000Z\"}]}"));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError CreateError;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), CreateError);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseListOptions Options;
    Options.Page = 2;
    Options.PerPage = 25;
    State->Client->DynamicCollection(TEXT("tasks")).GetList(
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseUnsafePathSegmentTest,
    "OpenPocketBase.Client.Config.RejectsUnsafePathSegments",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseUnsafePathSegmentTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FUnsafeSegmentTestState, ESPMode::ThreadSafe> State =
        MakeShared<FUnsafeSegmentTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FImmediateTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError CreateError;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), CreateError);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    TestFalse(TEXT("A traversal-like collection is invalid"), State->Client->DynamicCollection(TEXT("..")).IsValid());
    TestFalse(TEXT("A pre-encoded collection is invalid"), State->Client->DynamicCollection(TEXT("tasks%2Fadmin")).IsValid());

    State->Client->DynamicCollection(TEXT("tasks")).GetOne(
        TEXT("../secret"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->ErrorKind = Result.GetError().Kind;
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyUnsafeSegment(State, this));
    return true;
}

#endif
