#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"
#include "OpenPocketBaseFile.h"
#include "Transport/OpenPocketBaseTransport.h"

namespace
{
class FMultipartRequestTransport final : public IOpenPocketBaseTransport
{
public:
    TArray<FOpenPocketBaseHttpRequest> Requests;
    TArray<int64> UploadedBytes;

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        Requests.Add(Request);
        int64 TransferredBytes = 0;
        if (Request.BodyStream.IsValid())
        {
            uint8 Byte = 0;
            while (!Request.BodyStream->AtEnd())
            {
                Request.BodyStream->Serialize(&Byte, 1);
                if (Request.BodyStream->IsError())
                {
                    break;
                }
                ++TransferredBytes;
            }
        }
        UploadedBytes.Add(TransferredBytes);

        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = 200;
        Response.RequestId = Request.RequestId;
        Response.EffectiveUrl = Request.Url;
        const FString Json = FString::Printf(
            TEXT("{\"id\":\"task123\",\"collectionId\":\"tasks_id\",")
            TEXT("\"collectionName\":\"tasks\",\"title\":\"Uploaded\"}"));
        FTCHARToUTF8 Converted(*Json);
        Response.Body.Append(
            reinterpret_cast<const uint8*>(Converted.Get()),
            Converted.Length());
        OnComplete(MoveTemp(Response));
        return {};
    }
};

struct FMultipartRequestState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FMultipartRequestTransport, ESPMode::ThreadSafe> Transport;
    int32 CompletionCount = 0;
    bool bCreateSucceeded = false;
    bool bUpdateSucceeded = false;
    bool bProgressOnGameThread = true;
    TArray<FOpenPocketBaseTransferProgress> CreateProgress;
    TArray<FOpenPocketBaseTransferProgress> UpdateProgress;
};

class FHeldUploadTransport final : public IOpenPocketBaseTransport
{
public:
    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        ++RequestCount;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = 200;
        Response.RequestId = Request.RequestId;
        Response.EffectiveUrl = Request.Url;
        const FString Json = TEXT("{\"id\":\"task123\",\"collectionId\":\"tasks_id\",\"collectionName\":\"tasks\"}");
        FTCHARToUTF8 Converted(*Json);
        Response.Body.Append(
            reinterpret_cast<const uint8*>(Converted.Get()),
            Converted.Length());
        Completion = MoveTemp(OnComplete);
        return FOpenPocketBaseTransportHandle(
            [this]()
            {
                ++CancelCount;
            });
    }

    bool CompleteHeld()
    {
        if (!Completion)
        {
            return false;
        }
        FOpenPocketBaseHttpCompleteCallback Callback = MoveTemp(Completion);
        Callback(MoveTemp(Response));
        return true;
    }

    int32 RequestCount = 0;
    int32 CancelCount = 0;

private:
    FOpenPocketBaseHttpResponse Response;
    FOpenPocketBaseHttpCompleteCallback Completion;
};

struct FUploadTeardownState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FHeldUploadTransport, ESPMode::ThreadSafe> Transport;
    FString SourcePath;
    int32 CompletionCount = 0;
    bool bCancelled = false;
    bool bProgressAfterTerminal = false;
};

class FVerifyUploadTeardown final : public IAutomationLatentCommand
{
public:
    FVerifyUploadTeardown(
        TSharedRef<FUploadTeardownState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (State->CompletionCount < 1)
        {
            return false;
        }
        Test->TestTrue(TEXT("Client teardown cancels an upload"), State->bCancelled);
        Test->TestEqual(TEXT("Upload teardown has exactly one terminal callback"), State->CompletionCount, 1);
        Test->TestFalse(TEXT("Upload teardown has no progress after terminal"), State->bProgressAfterTerminal);
        Test->TestEqual(TEXT("Upload teardown sends one transport request"), State->Transport->RequestCount, 1);
        Test->TestEqual(TEXT("Upload teardown cancels one transport request"), State->Transport->CancelCount, 1);
        Test->TestTrue(TEXT("The upload source archive is released"), IFileManager::Get().Delete(*State->SourcePath, false, true));
        return true;
    }

private:
    TSharedRef<FUploadTeardownState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyMultipartRequests final : public IAutomationLatentCommand
{
public:
    FVerifyMultipartRequests(
        TSharedRef<FMultipartRequestState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (State->CompletionCount < 2)
        {
            return false;
        }

        Test->TestTrue(TEXT("Multipart create succeeds"), State->bCreateSucceeded);
        Test->TestTrue(TEXT("Multipart update succeeds"), State->bUpdateSucceeded);
        Test->TestTrue(TEXT("Upload progress uses the game thread"), State->bProgressOnGameThread);
        Test->TestTrue(TEXT("Create upload progress is published"), !State->CreateProgress.IsEmpty());
        Test->TestTrue(TEXT("Update upload progress is published"), !State->UpdateProgress.IsEmpty());
        Test->TestTrue(TEXT("Tiny create reads are coalesced"), State->CreateProgress.Num() <= 3);
        Test->TestTrue(TEXT("Tiny update reads are coalesced"), State->UpdateProgress.Num() <= 3);
        Test->TestEqual(TEXT("Two requests reach the transport"), State->Transport->Requests.Num(), 2);
        if (State->Transport->Requests.Num() == 2)
        {
            const FOpenPocketBaseHttpRequest& CreateRequest = State->Transport->Requests[0];
            const FOpenPocketBaseHttpRequest& UpdateRequest = State->Transport->Requests[1];
            Test->TestEqual(TEXT("Multipart create uses POST"), CreateRequest.Method, FString(TEXT("POST")));
            Test->TestEqual(TEXT("Multipart update uses PATCH"), UpdateRequest.Method, FString(TEXT("PATCH")));
            Test->TestTrue(TEXT("Multipart create uses a stream"), CreateRequest.BodyStream.IsValid());
            Test->TestTrue(TEXT("Multipart update uses a stream"), UpdateRequest.BodyStream.IsValid());
            Test->TestTrue(TEXT("Multipart requests do not duplicate bytes"), CreateRequest.Body.IsEmpty());
            Test->TestTrue(
                TEXT("Multipart content type is set"),
                CreateRequest.Headers.FindRef(TEXT("Content-Type")).StartsWith(TEXT("multipart/form-data; boundary=")));
            Test->TestEqual(
                TEXT("Content-Length matches the stream"),
                CreateRequest.Headers.FindRef(TEXT("Content-Length")),
                LexToString(CreateRequest.BodyLength));
            Test->TestEqual(
                TEXT("The body length matches the archive"),
                CreateRequest.BodyLength,
                CreateRequest.BodyStream.IsValid() ? CreateRequest.BodyStream->TotalSize() : 0);
            Test->TestEqual(TEXT("Create body is fully consumed"), State->Transport->UploadedBytes[0], CreateRequest.BodyLength);
            Test->TestEqual(TEXT("Update body is fully consumed"), State->Transport->UploadedBytes[1], UpdateRequest.BodyLength);
            if (!State->CreateProgress.IsEmpty())
            {
                const FOpenPocketBaseTransferProgress& FinalProgress = State->CreateProgress.Last();
                Test->TestEqual(TEXT("Create progress has all bytes"), FinalProgress.TransferredBytes, CreateRequest.BodyLength);
                Test->TestTrue(TEXT("Create progress has a known total"), FinalProgress.bHasTotalBytes);
                Test->TestEqual(TEXT("Create total is exact"), FinalProgress.TotalBytes, CreateRequest.BodyLength);
                Test->TestEqual(TEXT("Create progress reports attempt one"), FinalProgress.Attempt, 1);
                Test->TestEqual(
                    TEXT("Completed uploads enter finalizing"),
                    FinalProgress.Phase,
                    EOpenPocketBaseTransferPhase::Finalizing);
            }
        }

        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FMultipartRequestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseMultipartRequestTest,
    "OpenPocketBase.Client.Files.CreateAndUpdateUseMultipartStreams",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseMultipartRequestTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FMultipartRequestState, ESPMode::ThreadSafe> State =
        MakeShared<FMultipartRequestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FMultipartRequestTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError CreateError;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), CreateError);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRecordBody Body;
    Body.SetDynamicStringField(TEXT("title"), TEXT("Uploaded"));
    FOpenPocketBaseFileInput File;
    File.FieldName = TEXT("attachment");
    File.FileName = TEXT("sample.bin");
    File.bUseFilePath = false;
    File.Bytes = {0x01, 0x02, 0x03};

    State->Client->DynamicCollection(TEXT("tasks")).CreateWithFiles(
        Body,
        {File},
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bCreateSucceeded = Result.IsSuccess();
            ++State->CompletionCount;
        },
        {},
        {},
        [State](const FOpenPocketBaseTransferProgress& Progress)
        {
            State->bProgressOnGameThread = State->bProgressOnGameThread && IsInGameThread();
            State->CreateProgress.Add(Progress);
        });
    State->Client->DynamicCollection(TEXT("tasks")).UpdateWithFiles(
        TEXT("task123"),
        Body,
        {File},
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bUpdateSucceeded = Result.IsSuccess();
            ++State->CompletionCount;
        },
        {},
        {},
        [State](const FOpenPocketBaseTransferProgress& Progress)
        {
            State->bProgressOnGameThread = State->bProgressOnGameThread && IsInGameThread();
            State->UpdateProgress.Add(Progress);
        });
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyMultipartRequests(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseUploadTeardownTest,
    "OpenPocketBase.Client.Files.UploadTeardownIsExactlyOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseUploadTeardownTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FUploadTeardownState, ESPMode::ThreadSafe> State =
        MakeShared<FUploadTeardownState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FHeldUploadTransport, ESPMode::ThreadSafe>();

    State->SourcePath = FPaths::ConvertRelativePathToFull(
        FPaths::CreateTempFilename(
            *FPaths::ProjectIntermediateDir(),
            TEXT("OpenPocketBaseUploadTeardown-"),
            TEXT(".txt")));
    if (!TestTrue(TEXT("The upload source is written"), FFileHelper::SaveStringToFile(TEXT("upload teardown"), *State->SourcePath)))
    {
        return false;
    }

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        IFileManager::Get().Delete(*State->SourcePath, false, true);
        return false;
    }

    FOpenPocketBaseRecordBody Body;
    Body.SetDynamicStringField(TEXT("title"), TEXT("Teardown"));
    FOpenPocketBaseFileInput File;
    File.FieldName = TEXT("attachment");
    File.FileName = TEXT("teardown.txt");
    File.ContentType = TEXT("text/plain");
    File.FilePath = State->SourcePath;
    State->Client->DynamicCollection(TEXT("tasks")).CreateWithFiles(
        MoveTemp(Body),
        {MoveTemp(File)},
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bCancelled = !Result.IsSuccess() &&
                Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled;
            ++State->CompletionCount;
        },
        {},
        {},
        [State](const FOpenPocketBaseTransferProgress& Progress)
        {
            State->bProgressAfterTerminal = State->bProgressAfterTerminal ||
                State->CompletionCount > 0;
        });
    State->Client->Shutdown();
    State->Transport->CompleteHeld();

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyUploadTeardown(State, this));
    return true;
}

#endif
