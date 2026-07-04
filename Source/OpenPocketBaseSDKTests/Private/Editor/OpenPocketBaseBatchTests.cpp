#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "OpenPocketBaseBatch.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseScriptedTransport.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
TArray<uint8> BatchToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

FString BatchFromUtf8(const TArray<uint8>& Value)
{
    const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Value.GetData()), Value.Num());
    return FString(Converted.Length(), Converted.Get());
}

struct FBatchTestState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport;
    bool bCompleted = false;
    int32 CallbackCount = 0;
    int32 ValidationFailureCount = 0;
    bool bSucceeded = false;
    FOpenPocketBaseBatchResult BatchResult;
    FOpenPocketBaseError Error;
};

class FVerifyBatchContract final : public IAutomationLatentCommand
{
public:
    FVerifyBatchContract(
        const TSharedRef<FBatchTestState, ESPMode::ThreadSafe>& InState,
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
        Test->TestTrue(TEXT("The batch transaction succeeds"), State->bSucceeded);
        Test->TestEqual(TEXT("The batch callback runs exactly once"), State->CallbackCount, 1);
        Test->TestEqual(TEXT("Every operation has a typed result"), State->BatchResult.Results.Num(), 4);
        if (State->BatchResult.Results.Num() == 4)
        {
            Test->TestEqual(TEXT("Create result is typed"), State->BatchResult.Results[0].Operation, EOpenPocketBaseBatchOperation::Create);
            Test->TestTrue(TEXT("Create returns a record"), State->BatchResult.Results[0].bHasRecord);
            Test->TestEqual(TEXT("Update result is typed"), State->BatchResult.Results[1].Operation, EOpenPocketBaseBatchOperation::Update);
            Test->TestEqual(TEXT("Upsert result is typed"), State->BatchResult.Results[2].Operation, EOpenPocketBaseBatchOperation::Upsert);
            Test->TestEqual(TEXT("Delete result is typed"), State->BatchResult.Results[3].Operation, EOpenPocketBaseBatchOperation::Delete);
            Test->TestFalse(TEXT("Delete has no record body"), State->BatchResult.Results[3].bHasRecord);
        }

        FOpenPocketBaseHttpRequest Request;
        Test->TestTrue(TEXT("The batch request is captured"), State->Transport->TryGetRequest(0, Request));
        Test->TestEqual(TEXT("Batch uses POST"), Request.Method, FString(TEXT("POST")));
        Test->TestEqual(TEXT("Batch uses the protocol route"), Request.Url, FString(TEXT("https://pb.example.com/api/batch")));

        TSharedPtr<FJsonObject> Object;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BatchFromUtf8(Request.Body));
        Test->TestTrue(TEXT("Batch sends JSON"), FJsonSerializer::Deserialize(Reader, Object));
        const TArray<TSharedPtr<FJsonValue>>* Requests = nullptr;
        Test->TestTrue(
            TEXT("Batch sends four explicit requests"),
            Object.IsValid() && Object->TryGetArrayField(TEXT("requests"), Requests) &&
                Requests != nullptr && Requests->Num() == 4);
        if (Requests != nullptr && Requests->Num() == 4)
        {
            Test->TestEqual(TEXT("Create wire method"), (*Requests)[0]->AsObject()->GetStringField(TEXT("method")), FString(TEXT("POST")));
            Test->TestEqual(TEXT("Update wire method"), (*Requests)[1]->AsObject()->GetStringField(TEXT("method")), FString(TEXT("PATCH")));
            Test->TestEqual(TEXT("Upsert wire method"), (*Requests)[2]->AsObject()->GetStringField(TEXT("method")), FString(TEXT("PUT")));
            Test->TestEqual(TEXT("Delete wire method"), (*Requests)[3]->AsObject()->GetStringField(TEXT("method")), FString(TEXT("DELETE")));
        }
        Test->TestEqual(TEXT("One transaction uses one HTTP request"), State->Transport->GetRequestCount(), 1);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FBatchTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyBatchFailure final : public IAutomationLatentCommand
{
public:
    FVerifyBatchFailure(
        const TSharedRef<FBatchTestState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest,
        const EOpenPocketBaseErrorKind InExpectedKind,
        const int32 InExpectedRequestCount = 1,
        FString InExpectedFieldKey = {})
        : State(InState)
        , Test(InTest)
        , ExpectedKind(InExpectedKind)
        , ExpectedRequestCount(InExpectedRequestCount)
        , ExpectedFieldKey(MoveTemp(InExpectedFieldKey))
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted)
        {
            return false;
        }
        Test->TestEqual(TEXT("The expected batch failure is reported"), State->Error.Kind, ExpectedKind);
        Test->TestEqual(TEXT("The failed batch uses the expected request count"), State->Transport->GetRequestCount(), ExpectedRequestCount);
        Test->TestEqual(TEXT("The failed batch callback runs exactly once"), State->CallbackCount, 1);
        if (!ExpectedFieldKey.IsEmpty())
        {
            Test->TestTrue(
                TEXT("Nested operation validation is retained in the shared error"),
                State->Error.FieldErrors.Contains(ExpectedFieldKey));
        }
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FBatchTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
    EOpenPocketBaseErrorKind ExpectedKind;
    int32 ExpectedRequestCount;
    FString ExpectedFieldKey;
};

class FVerifyBatchBounds final : public IAutomationLatentCommand
{
public:
    FVerifyBatchBounds(
        const TSharedRef<FBatchTestState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (State->ValidationFailureCount != 3)
        {
            return false;
        }
        Test->TestEqual(TEXT("All invalid bounds fail locally"), State->CallbackCount, 3);
        Test->TestEqual(TEXT("Invalid batches never reach the transport"), State->Transport->GetRequestCount(), 0);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FBatchTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

TSharedRef<FBatchTestState, ESPMode::ThreadSafe> MakeBatchState(FAutomationTestBase* Test)
{
    const TSharedRef<FBatchTestState, ESPMode::ThreadSafe> State =
        MakeShared<FBatchTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    Test->TestNotNull(TEXT("The client is created"), State->Client.Get());
    return State;
}

FOpenPocketBaseBatchRequest MakeCompleteBatch()
{
    FOpenPocketBaseRecordBody CreateBody;
    CreateBody.SetStringField(TEXT("title"), TEXT("Create"));
    FOpenPocketBaseRecordBody UpdateBody;
    UpdateBody.SetStringField(TEXT("title"), TEXT("Update"));
    FOpenPocketBaseRecordBody UpsertBody;
    UpsertBody.SetStringField(TEXT("id"), TEXT("task00000000003"));
    UpsertBody.SetStringField(TEXT("title"), TEXT("Upsert"));

    FOpenPocketBaseBatchRequest Batch;
    Batch.AddCreate(TEXT("tasks"), MoveTemp(CreateBody));
    Batch.AddUpdate(TEXT("tasks"), TEXT("task00000000001"), MoveTemp(UpdateBody));
    Batch.AddUpsert(TEXT("tasks"), MoveTemp(UpsertBody));
    Batch.AddDelete(TEXT("tasks"), TEXT("task00000000002"));
    return Batch;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBatchContractTest,
    "OpenPocketBase.Client.Batch.SerializesAndParsesOperations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBatchContractTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FBatchTestState, ESPMode::ThreadSafe> State = MakeBatchState(this);
    if (!State->Client.IsValid())
    {
        return false;
    }

    FOpenPocketBaseTransportScript Script;
    Script.Response.bTransportSucceeded = true;
    Script.Response.HttpStatus = 200;
    Script.Response.Body = BatchToUtf8(
        TEXT("[{\"status\":200,\"body\":{\"id\":\"task00000000004\",\"collectionId\":\"tasks_id\",\"collectionName\":\"tasks\"}},")
        TEXT("{\"status\":200,\"body\":{\"id\":\"task00000000001\",\"collectionId\":\"tasks_id\",\"collectionName\":\"tasks\"}},")
        TEXT("{\"status\":200,\"body\":{\"id\":\"task00000000003\",\"collectionId\":\"tasks_id\",\"collectionName\":\"tasks\"}},")
        TEXT("{\"status\":204,\"body\":null}]") );
    State->Transport->Enqueue(MoveTemp(Script));

    State->Client->SendBatch(
        MakeCompleteBatch(),
        [State](TOpenPocketBaseResult<FOpenPocketBaseBatchResult>&& Result)
        {
            State->bSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                State->BatchResult = Result.GetValue();
            }
            ++State->CallbackCount;
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyBatchContract(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseDisabledBatchTest,
    "OpenPocketBase.Client.Batch.ReportsDisabledServer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseDisabledBatchTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FBatchTestState, ESPMode::ThreadSafe> State = MakeBatchState(this);
    if (!State->Client.IsValid())
    {
        return false;
    }

    FOpenPocketBaseTransportScript Script;
    Script.Response.bTransportSucceeded = true;
    Script.Response.HttpStatus = 403;
    Script.Response.Body = BatchToUtf8(
        TEXT("{\"status\":403,\"message\":\"Batch requests are not allowed.\",\"data\":{}}"));
    State->Transport->Enqueue(MoveTemp(Script));

    FOpenPocketBaseBatchOptions Options;
    Options.RequestOptions.MaxReadRetries = 5;
    State->Client->SendBatch(
        MakeCompleteBatch(),
        [State](TOpenPocketBaseResult<FOpenPocketBaseBatchResult>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->Error = Result.GetError();
            }
            ++State->CallbackCount;
            State->bCompleted = true;
        },
        Options);

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyBatchFailure(State, this, EOpenPocketBaseErrorKind::Unsupported));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBatchBoundsTest,
    "OpenPocketBase.Client.Batch.EnforcesLocalBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBatchBoundsTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FBatchTestState, ESPMode::ThreadSafe> State = MakeBatchState(this);
    if (!State->Client.IsValid())
    {
        return false;
    }

    const auto OnFailure = [State](TOpenPocketBaseResult<FOpenPocketBaseBatchResult>&& Result)
    {
        if (!Result.IsSuccess() && Result.GetError().Kind == EOpenPocketBaseErrorKind::InvalidArgument)
        {
            ++State->ValidationFailureCount;
        }
        ++State->CallbackCount;
    };

    FOpenPocketBaseBatchOptions CountOptions;
    CountOptions.MaxOperations = 3;
    State->Client->SendBatch(MakeCompleteBatch(), OnFailure, CountOptions);

    FOpenPocketBaseRecordBody LargeBody;
    LargeBody.SetStringField(TEXT("title"), FString::ChrN(2048, TEXT('x')));
    FOpenPocketBaseBatchRequest LargeBatch;
    LargeBatch.AddCreate(TEXT("tasks"), MoveTemp(LargeBody));
    FOpenPocketBaseBatchOptions BodyOptions;
    BodyOptions.MaxBodyBytes = 1024;
    State->Client->SendBatch(MoveTemp(LargeBatch), OnFailure, BodyOptions);

    FOpenPocketBaseBatchOptions TimeoutOptions;
    TimeoutOptions.RequestOptions.TotalTimeoutSeconds = 121;
    State->Client->SendBatch(MakeCompleteBatch(), OnFailure, TimeoutOptions);

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyBatchBounds(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBatchTransactionFailureTest,
    "OpenPocketBase.Client.Batch.PreservesTransactionFailureDetails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBatchTransactionFailureTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FBatchTestState, ESPMode::ThreadSafe> State = MakeBatchState(this);
    if (!State->Client.IsValid())
    {
        return false;
    }

    FOpenPocketBaseTransportScript Script;
    Script.Response.bTransportSucceeded = true;
    Script.Response.HttpStatus = 400;
    Script.Response.Body = BatchToUtf8(
        TEXT("{\"status\":400,\"message\":\"Failed to process the batch request.\",\"data\":{")
        TEXT("\"requests\":{\"1\":{\"code\":\"validation_failure\",\"response\":{")
        TEXT("\"data\":{\"title\":{\"code\":\"validation_required\",\"message\":\"Required value.\"}}}}}}}"));
    State->Transport->Enqueue(MoveTemp(Script));

    State->Client->SendBatch(
        MakeCompleteBatch(),
        [State](TOpenPocketBaseResult<FOpenPocketBaseBatchResult>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->Error = Result.GetError();
            }
            ++State->CallbackCount;
            State->bCompleted = true;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyBatchFailure(
        State,
        this,
        EOpenPocketBaseErrorKind::PocketBase,
        1,
        TEXT("requests.1.title")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBatchCancellationTest,
    "OpenPocketBase.Client.Batch.CancelsExactlyOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBatchCancellationTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FBatchTestState, ESPMode::ThreadSafe> State = MakeBatchState(this);
    if (!State->Client.IsValid())
    {
        return false;
    }

    FOpenPocketBaseTransportScript Script;
    Script.bHoldCompletion = true;
    Script.bCompleteAfterCancel = true;
    Script.Response.bTransportSucceeded = true;
    Script.Response.HttpStatus = 200;
    Script.Response.Body = BatchToUtf8(TEXT("[]"));
    State->Transport->Enqueue(MoveTemp(Script));

    const FOpenPocketBaseRequestHandle Handle = State->Client->SendBatch(
        MakeCompleteBatch(),
        [State](TOpenPocketBaseResult<FOpenPocketBaseBatchResult>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->Error = Result.GetError();
            }
            ++State->CallbackCount;
            State->bCompleted = true;
        });
    Handle.Cancel();
    TestTrue(TEXT("The late held completion remains observable"), State->Transport->CompleteNextHeld());

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyBatchFailure(
        State,
        this,
        EOpenPocketBaseErrorKind::Cancelled));
    return true;
}

#endif
