#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseFile.h"
#include "OpenPocketBaseScriptedTransport.h"

namespace
{
TArray<uint8> DownloadBytes(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

struct FDownloadTestState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport;
    FString DestinationPath;
    int32 CompletionCount = 0;
    bool bMemorySucceeded = false;
    bool bDiskSucceeded = false;
    bool bBoundRejected = false;
    bool bShortWriteRejected = false;
    FOpenPocketBaseFileDownloadResult MemoryResult;
    FOpenPocketBaseFileDownloadResult DiskResult;
};

class FVerifyDownloads final : public IAutomationLatentCommand
{
public:
    FVerifyDownloads(
        TSharedRef<FDownloadTestState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (State->CompletionCount < 4)
        {
            return false;
        }

        Test->TestTrue(TEXT("A bounded memory download succeeds"), State->bMemorySucceeded);
        Test->TestEqual(TEXT("Memory chunks are joined"), State->MemoryResult.Bytes, DownloadBytes(TEXT("abcdef")));
        Test->TestEqual(TEXT("The actual length is returned"), State->MemoryResult.ContentLength, 6LL);
        Test->TestEqual(TEXT("The content type is returned"), State->MemoryResult.ContentType, FString(TEXT("text/plain")));
        Test->TestEqual(TEXT("A safe filename hint is returned"), State->MemoryResult.FileName, FString(TEXT("server_name.txt")));
        Test->TestEqual(TEXT("The ETag is returned"), State->MemoryResult.ETag, FString(TEXT("fixture-etag")));

        Test->TestTrue(TEXT("A streamed disk download succeeds"), State->bDiskSucceeded);
        Test->TestTrue(TEXT("Disk metadata identifies the destination"), State->DiskResult.bSavedToFile);
        Test->TestEqual(TEXT("The destination is retained"), State->DiskResult.DestinationPath, State->DestinationPath);
        TArray<uint8> SavedBytes;
        Test->TestTrue(TEXT("The final destination exists"), FFileHelper::LoadFileToArray(SavedBytes, *State->DestinationPath));
        Test->TestEqual(TEXT("Disk chunks are joined"), SavedBytes, DownloadBytes(TEXT("disk-data")));
        Test->TestFalse(TEXT("The temporary file is removed after rename"), IFileManager::Get().FileExists(*(State->DestinationPath + TEXT(".tmp"))));

        Test->TestTrue(TEXT("A response exceeding its bound fails"), State->bBoundRejected);
        Test->TestTrue(TEXT("A short streamed response fails"), State->bShortWriteRejected);
        Test->TestEqual(TEXT("Four requests reach the transport"), State->Transport->GetRequestCount(), 4);
        FOpenPocketBaseHttpRequest Request;
        if (State->Transport->TryGetRequest(0, Request))
        {
            Test->TestTrue(TEXT("Downloads request incremental chunks"), Request.bStreamResponse);
            Test->TestTrue(TEXT("The URL uses encoded path segments"), Request.Url.Contains(TEXT("/api/files/tasks%20id/record-1/report%20final.txt")));
        }

        IFileManager::Get().Delete(*State->DestinationPath, false, true);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FDownloadTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

struct FCancelledDownloadState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    FString DestinationPath;
    bool bCompleted = false;
    bool bCancelled = false;
};

class FVerifyCancelledDownload final : public IAutomationLatentCommand
{
public:
    FVerifyCancelledDownload(
        TSharedRef<FCancelledDownloadState, ESPMode::ThreadSafe> InState,
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
        Test->TestTrue(TEXT("Cancellation has one cancelled result"), State->bCancelled);
        Test->TestFalse(TEXT("Cancellation removes the temporary file"), IFileManager::Get().FileExists(*(State->DestinationPath + TEXT(".tmp"))));
        Test->TestFalse(TEXT("Cancellation never publishes the final file"), IFileManager::Get().FileExists(*State->DestinationPath));
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FCancelledDownloadState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseFileDownloadTest,
    "OpenPocketBase.Client.Files.StreamsBoundedDownloads",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseFileDownloadTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FDownloadTestState, ESPMode::ThreadSafe> State =
        MakeShared<FDownloadTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    State->DestinationPath = FPaths::CreateTempFilename(
        *FPaths::ProjectIntermediateDir(),
        TEXT("OpenPocketBaseDownload-"),
        TEXT(".bin"));
    State->DestinationPath = FPaths::ConvertRelativePathToFull(State->DestinationPath);

    FOpenPocketBaseTransportScript MemoryScript;
    MemoryScript.Chunks = {DownloadBytes(TEXT("abc")), DownloadBytes(TEXT("def"))};
    MemoryScript.Response.bTransportSucceeded = true;
    MemoryScript.Response.HttpStatus = 200;
    MemoryScript.Response.Headers.Add(TEXT("Content-Type"), TEXT("text/plain"));
    MemoryScript.Response.Headers.Add(TEXT("Content-Length"), TEXT("6"));
    MemoryScript.Response.Headers.Add(TEXT("Content-Disposition"), TEXT("attachment; filename=\"server_name.txt\""));
    MemoryScript.Response.Headers.Add(TEXT("ETag"), TEXT("fixture-etag"));
    State->Transport->Enqueue(MoveTemp(MemoryScript));

    FOpenPocketBaseTransportScript DiskScript;
    DiskScript.Chunks = {DownloadBytes(TEXT("disk-")), DownloadBytes(TEXT("data"))};
    DiskScript.Response.bTransportSucceeded = true;
    DiskScript.Response.HttpStatus = 200;
    State->Transport->Enqueue(MoveTemp(DiskScript));

    FOpenPocketBaseTransportScript BoundScript;
    BoundScript.Chunks = {DownloadBytes(TEXT("too-large"))};
    BoundScript.Response.bTransportSucceeded = true;
    BoundScript.Response.HttpStatus = 200;
    State->Transport->Enqueue(MoveTemp(BoundScript));

    FOpenPocketBaseTransportScript ShortScript;
    ShortScript.Chunks = {DownloadBytes(TEXT("short"))};
    ShortScript.Response.bTransportSucceeded = true;
    ShortScript.Response.HttpStatus = 200;
    ShortScript.Response.Headers.Add(TEXT("Content-Length"), TEXT("10"));
    State->Transport->Enqueue(MoveTemp(ShortScript));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseFileDownloadOptions MemoryOptions;
    MemoryOptions.MaxBytes = 6;
    State->Client->Files().Download(
        TEXT("tasks id"),
        TEXT("record-1"),
        TEXT("report final.txt"),
        MemoryOptions,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&& Result)
        {
            State->bMemorySucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                State->MemoryResult = MoveTemp(Result.GetValue());
            }
            ++State->CompletionCount;
        });

    FOpenPocketBaseFileDownloadOptions DiskOptions;
    DiskOptions.Target = EOpenPocketBaseFileDownloadTarget::File;
    DiskOptions.DestinationPath = State->DestinationPath;
    DiskOptions.MaxBytes = 1024;
    State->Client->Files().Download(
        TEXT("tasks"),
        TEXT("record-1"),
        TEXT("disk.bin"),
        DiskOptions,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&& Result)
        {
            State->bDiskSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                State->DiskResult = MoveTemp(Result.GetValue());
            }
            ++State->CompletionCount;
        });

    FOpenPocketBaseFileDownloadOptions BoundOptions;
    BoundOptions.MaxBytes = 4;
    State->Client->Files().Download(
        TEXT("tasks"),
        TEXT("record-1"),
        TEXT("large.bin"),
        BoundOptions,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&& Result)
        {
            State->bBoundRejected = !Result.IsSuccess() &&
                Result.GetError().Kind == EOpenPocketBaseErrorKind::Transport;
            ++State->CompletionCount;
        });

    FOpenPocketBaseFileDownloadOptions ShortOptions;
    ShortOptions.MaxBytes = 32;
    State->Client->Files().Download(
        TEXT("tasks"),
        TEXT("record-1"),
        TEXT("short.bin"),
        ShortOptions,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&& Result)
        {
            State->bShortWriteRejected = !Result.IsSuccess() &&
                Result.GetError().Kind == EOpenPocketBaseErrorKind::Transport;
            ++State->CompletionCount;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyDownloads(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseFileDownloadCancelTest,
    "OpenPocketBase.Client.Files.CancelRemovesTemporaryDownload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseFileDownloadCancelTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FCancelledDownloadState, ESPMode::ThreadSafe> State =
        MakeShared<FCancelledDownloadState, ESPMode::ThreadSafe>();
    const TSharedRef<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    State->DestinationPath = FPaths::CreateTempFilename(
        *FPaths::ProjectIntermediateDir(),
        TEXT("OpenPocketBaseCancelled-"),
        TEXT(".bin"));
    State->DestinationPath = FPaths::ConvertRelativePathToFull(State->DestinationPath);

    FOpenPocketBaseTransportScript Script;
    Script.Chunks = {DownloadBytes(TEXT("partial"))};
    Script.Response.bTransportSucceeded = true;
    Script.Response.HttpStatus = 200;
    Script.bHoldCompletion = true;
    Script.bCompleteAfterCancel = true;
    Transport->Enqueue(MoveTemp(Script));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, Transport, Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseFileDownloadOptions Options;
    Options.Target = EOpenPocketBaseFileDownloadTarget::File;
    Options.DestinationPath = State->DestinationPath;
    Options.MaxBytes = 1024;
    FOpenPocketBaseRequestHandle Handle = State->Client->Files().Download(
        TEXT("tasks"),
        TEXT("record-1"),
        TEXT("cancel.bin"),
        Options,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&& Result)
        {
            State->bCancelled = !Result.IsSuccess() &&
                Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled;
            State->bCompleted = true;
        });
    Handle.Cancel();

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyCancelledDownload(State, this));
    return true;
}

#endif
