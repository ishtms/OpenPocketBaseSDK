#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"
#include "OpenPocketBaseScriptedTransport.h"

namespace
{
TArray<uint8> FullListToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

FOpenPocketBaseTransportScript MakePageScript(
    const int32 Page,
    const TArray<FString>& RecordIds,
    const bool bHold = false)
{
    FString Items;
    for (const FString& RecordId : RecordIds)
    {
        if (!Items.IsEmpty())
        {
            Items += TEXT(",");
        }
        Items += FString::Printf(
            TEXT("{\"id\":\"%s\",\"collectionId\":\"tasks_id\",\"collectionName\":\"tasks\"}"),
            *RecordId);
    }

    FOpenPocketBaseTransportScript Script;
    Script.Response.bTransportSucceeded = true;
    Script.Response.HttpStatus = 200;
    Script.Response.Body = FullListToUtf8(FString::Printf(
        TEXT("{\"page\":%d,\"perPage\":2,\"totalItems\":-1,\"totalPages\":-1,\"items\":[%s]}"),
        Page,
        *Items));
    Script.bHoldCompletion = bHold;
    return Script;
}

struct FFullListTestState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport;
    FOpenPocketBaseRequestHandle Handle;
    bool bCompleted = false;
    bool bSucceeded = false;
    bool bCancelled = false;
    bool bHasTotalItems = true;
    bool bHasTotalPages = true;
    EOpenPocketBaseErrorKind ErrorKind = EOpenPocketBaseErrorKind::None;
    FString ErrorMessage;
    FOpenPocketBaseFullListResult FullList;
};

class FVerifyBoundedFullList final : public IAutomationLatentCommand
{
public:
    FVerifyBoundedFullList(
        const TSharedRef<FFullListTestState, ESPMode::ThreadSafe>& InState,
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

        Test->TestTrue(TEXT("Bounded full-list traversal succeeds"), State->bSucceeded);
        Test->TestEqual(TEXT("The item bound is enforced"), State->FullList.Items.Num(), 3);
        Test->TestEqual(TEXT("Only two pages are fetched"), State->FullList.PagesFetched, 2);
        Test->TestTrue(TEXT("The result reports the item limit"), State->FullList.bReachedItemLimit);
        Test->TestFalse(TEXT("The result does not claim an unknown collection end"), State->FullList.bReachedEnd);
        Test->TestEqual(TEXT("No extra page is requested"), State->Transport->GetRequestCount(), 2);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FFullListTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FCancelBetweenPages final : public IAutomationLatentCommand
{
public:
    explicit FCancelBetweenPages(const TSharedRef<FFullListTestState, ESPMode::ThreadSafe>& InState)
        : State(InState)
    {
    }

    virtual bool Update() override
    {
        if (State->Transport->GetRequestCount() < 2)
        {
            return false;
        }
        State->Handle.Cancel();
        return true;
    }

private:
    TSharedRef<FFullListTestState, ESPMode::ThreadSafe> State;
};

class FVerifyPageBound final : public IAutomationLatentCommand
{
public:
    FVerifyPageBound(
        const TSharedRef<FFullListTestState, ESPMode::ThreadSafe>& InState,
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
        Test->TestTrue(TEXT("Page-bounded traversal succeeds"), State->bSucceeded);
        Test->TestEqual(TEXT("One page is returned"), State->FullList.PagesFetched, 1);
        Test->TestTrue(TEXT("The result reports the page limit"), State->FullList.bReachedPageLimit);
        Test->TestFalse(TEXT("The result does not claim collection exhaustion"), State->FullList.bReachedEnd);
        Test->TestEqual(TEXT("Only one page is requested"), State->Transport->GetRequestCount(), 1);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FFullListTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyPaginationValidation final : public IAutomationLatentCommand
{
public:
    FVerifyPaginationValidation(
        const TSharedRef<FFullListTestState, ESPMode::ThreadSafe>& InState,
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
        Test->TestEqual(TEXT("An unbounded full list is rejected"), State->ErrorKind, EOpenPocketBaseErrorKind::InvalidArgument);
        Test->TestEqual(
            TEXT("The missing bound is explained directly"),
            State->ErrorMessage,
            FString(TEXT("Set Max Items or Max Pages before starting full-list traversal.")));
        Test->TestEqual(TEXT("Invalid traversal never reaches transport"), State->Transport->GetRequestCount(), 0);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FFullListTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifySkipTotal final : public IAutomationLatentCommand
{
public:
    FVerifySkipTotal(
        const TSharedRef<FFullListTestState, ESPMode::ThreadSafe>& InState,
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
        Test->TestTrue(TEXT("The skip-total list succeeds"), State->bSucceeded);
        Test->TestFalse(TEXT("Minus-one total items become absent"), State->bHasTotalItems);
        Test->TestFalse(TEXT("Minus-one total pages become absent"), State->bHasTotalPages);
        FOpenPocketBaseHttpRequest Request;
        Test->TestTrue(TEXT("The list request is captured"), State->Transport->TryGetRequest(0, Request));
        Test->TestTrue(TEXT("skipTotal is sent explicitly"), Request.Url.Contains(TEXT("skipTotal=true")));
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FFullListTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyFullListCancellation final : public IAutomationLatentCommand
{
public:
    FVerifyFullListCancellation(
        const TSharedRef<FFullListTestState, ESPMode::ThreadSafe>& InState,
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

        Test->TestTrue(TEXT("Cancellation reaches the full-list callback"), State->bCancelled);
        Test->TestEqual(TEXT("Cancellation stops before another page"), State->Transport->GetRequestCount(), 2);
        Test->TestTrue(TEXT("Cancellation reaches the active page"), State->Transport->GetCancelCount() >= 1);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FFullListTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

TSharedRef<FFullListTestState, ESPMode::ThreadSafe> MakeFullListState(FAutomationTestBase* Test)
{
    const TSharedRef<FFullListTestState, ESPMode::ThreadSafe> State =
        MakeShared<FFullListTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    Test->TestNotNull(TEXT("The client is created"), State->Client.Get());
    return State;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBoundedFullListTest,
    "OpenPocketBase.Client.Pagination.FullListStopsAtItemBound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBoundedFullListTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FFullListTestState, ESPMode::ThreadSafe> State = MakeFullListState(this);
    if (!State->Client.IsValid())
    {
        return false;
    }
    State->Transport->Enqueue(MakePageScript(1, {TEXT("task00000000001"), TEXT("task00000000002")}));
    State->Transport->Enqueue(MakePageScript(2, {TEXT("task00000000003"), TEXT("task00000000004")}));

    FOpenPocketBaseFullListOptions Options;
    Options.ListOptions.PerPage = 2;
    Options.ListOptions.bSkipTotal = true;
    Options.MaxItems = 3;
    State->Handle = State->Client->DynamicCollection(TEXT("tasks")).GetFullList(
        Options,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFullListResult>&& Result)
        {
            State->bSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                State->FullList = Result.GetValue();
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyBoundedFullList(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseCancelFullListTest,
    "OpenPocketBase.Client.Pagination.FullListCancelsBetweenPages",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseCancelFullListTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FFullListTestState, ESPMode::ThreadSafe> State = MakeFullListState(this);
    if (!State->Client.IsValid())
    {
        return false;
    }
    State->Transport->Enqueue(MakePageScript(1, {TEXT("task00000000001"), TEXT("task00000000002")}));
    State->Transport->Enqueue(MakePageScript(2, {TEXT("task00000000003"), TEXT("task00000000004")}, true));

    FOpenPocketBaseFullListOptions Options;
    Options.ListOptions.PerPage = 2;
    Options.ListOptions.bSkipTotal = true;
    Options.MaxPages = 5;
    State->Handle = State->Client->DynamicCollection(TEXT("tasks")).GetFullList(
        Options,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFullListResult>&& Result)
        {
            State->bCancelled = !Result.IsSuccess() &&
                Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled;
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FCancelBetweenPages(State));
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyFullListCancellation(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBasePageBoundedFullListTest,
    "OpenPocketBase.Client.Pagination.FullListStopsAtPageBound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBasePageBoundedFullListTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FFullListTestState, ESPMode::ThreadSafe> State = MakeFullListState(this);
    if (!State->Client.IsValid())
    {
        return false;
    }
    State->Transport->Enqueue(MakePageScript(1, {TEXT("task00000000001"), TEXT("task00000000002")}));

    FOpenPocketBaseFullListOptions Options;
    Options.ListOptions.PerPage = 2;
    Options.ListOptions.bSkipTotal = true;
    Options.MaxPages = 1;
    State->Handle = State->Client->DynamicCollection(TEXT("tasks")).GetFullList(
        Options,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFullListResult>&& Result)
        {
            State->bSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                State->FullList = Result.GetValue();
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyPageBound(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRejectUnboundedFullListTest,
    "OpenPocketBase.Client.Pagination.FullListRequiresBound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRejectUnboundedFullListTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FFullListTestState, ESPMode::ThreadSafe> State = MakeFullListState(this);
    if (!State->Client.IsValid())
    {
        return false;
    }
    State->Client->DynamicCollection(TEXT("tasks")).GetFullList(
        {},
        [State](TOpenPocketBaseResult<FOpenPocketBaseFullListResult>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->ErrorKind = Result.GetError().Kind;
                State->ErrorMessage = Result.GetError().ServerMessage;
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyPaginationValidation(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSkipTotalTest,
    "OpenPocketBase.Client.Pagination.SkipTotalIsOptional",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSkipTotalTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FFullListTestState, ESPMode::ThreadSafe> State = MakeFullListState(this);
    if (!State->Client.IsValid())
    {
        return false;
    }
    State->Transport->Enqueue(MakePageScript(1, {TEXT("task00000000001")}));

    FOpenPocketBaseListOptions Options;
    Options.PerPage = 2;
    Options.bSkipTotal = true;
    State->Client->DynamicCollection(TEXT("tasks")).GetList(
        Options,
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecordPage>&& Result)
        {
            State->bSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                State->bHasTotalItems = Result.GetValue().bHasTotalItems;
                State->bHasTotalPages = Result.GetValue().bHasTotalPages;
            }
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifySkipTotal(State, this));
    return true;
}

#endif
