#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Files/OpenPocketBaseDownload.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"
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
    FString RedirectDestinationPath;
    int32 CompletionCount = 0;
    bool bMemorySucceeded = false;
    bool bDiskSucceeded = false;
    bool bBoundRejected = false;
    bool bShortWriteRejected = false;
    bool bRedirectRejected = false;
    bool bTimeoutRejected = false;
    bool bProgressOnGameThread = true;
    TArray<FOpenPocketBaseTransferProgress> MemoryProgress;
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
        if (State->CompletionCount < 6)
        {
            return false;
        }

        Test->TestTrue(TEXT("A bounded memory download succeeds"), State->bMemorySucceeded);
        Test->TestEqual(TEXT("Memory chunks are joined"), State->MemoryResult.Bytes, DownloadBytes(TEXT("abcdef")));
        Test->TestEqual(TEXT("The actual length is returned"), State->MemoryResult.ContentLength, 6LL);
        Test->TestEqual(TEXT("The content type is returned"), State->MemoryResult.ContentType, FString(TEXT("text/plain")));
        Test->TestEqual(TEXT("A safe filename hint is returned"), State->MemoryResult.FileName, FString(TEXT("server_name.txt")));
        Test->TestEqual(TEXT("The ETag is returned"), State->MemoryResult.ETag, FString(TEXT("fixture-etag")));
        Test->TestTrue(TEXT("Download progress uses the game thread"), State->bProgressOnGameThread);
        Test->TestTrue(TEXT("Download progress is published"), !State->MemoryProgress.IsEmpty());
        Test->TestTrue(TEXT("Tiny download chunks are coalesced"), State->MemoryProgress.Num() <= 2);
        if (!State->MemoryProgress.IsEmpty())
        {
            const FOpenPocketBaseTransferProgress& FinalProgress = State->MemoryProgress.Last();
            Test->TestEqual(TEXT("Final download progress has all bytes"), FinalProgress.TransferredBytes, 6LL);
            Test->TestTrue(TEXT("Final download progress has a known total"), FinalProgress.bHasTotalBytes);
            Test->TestEqual(TEXT("Final download total is exact"), FinalProgress.TotalBytes, 6LL);
            Test->TestEqual(TEXT("Download progress reports attempt one"), FinalProgress.Attempt, 1);
            Test->TestEqual(
                TEXT("Completed downloads enter finalizing"),
                FinalProgress.Phase,
                EOpenPocketBaseTransferPhase::Finalizing);
        }

        Test->TestTrue(TEXT("A streamed disk download succeeds"), State->bDiskSucceeded);
        Test->TestTrue(TEXT("Disk metadata identifies the destination"), State->DiskResult.bSavedToFile);
        Test->TestEqual(TEXT("The destination is retained"), State->DiskResult.DestinationPath, State->DestinationPath);
        TArray<uint8> SavedBytes;
        Test->TestTrue(TEXT("The final destination exists"), FFileHelper::LoadFileToArray(SavedBytes, *State->DestinationPath));
        Test->TestEqual(TEXT("Disk chunks are joined"), SavedBytes, DownloadBytes(TEXT("disk-data")));
        Test->TestFalse(TEXT("The temporary file is removed after rename"), IFileManager::Get().FileExists(*(State->DestinationPath + TEXT(".tmp"))));

        Test->TestTrue(TEXT("A response exceeding its bound fails"), State->bBoundRejected);
        Test->TestTrue(TEXT("A short streamed response fails"), State->bShortWriteRejected);
        Test->TestTrue(TEXT("A cross-origin redirect fails"), State->bRedirectRejected);
        Test->TestFalse(TEXT("A rejected redirect removes its temporary file"), IFileManager::Get().FileExists(*(State->RedirectDestinationPath + TEXT(".tmp"))));
        Test->TestFalse(TEXT("A rejected redirect never publishes its final file"), IFileManager::Get().FileExists(*State->RedirectDestinationPath));
        Test->TestTrue(TEXT("A timeout remains classified and is not retried"), State->bTimeoutRejected);
        Test->TestEqual(TEXT("Six requests reach the transport"), State->Transport->GetRequestCount(), 6);
        FOpenPocketBaseHttpRequest Request;
        if (State->Transport->TryGetRequest(0, Request))
        {
            Test->TestTrue(TEXT("Downloads request incremental chunks"), Request.bStreamResponse);
            Test->TestTrue(TEXT("The URL uses encoded path segments"), Request.Url.Contains(TEXT("/api/files/tasks%20id/record-1/report%20final.txt")));
        }

        IFileManager::Get().Delete(*State->DestinationPath, false, true);
        IFileManager::Get().Delete(*State->RedirectDestinationPath, false, true);
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
    bool bProgressAfterTerminal = false;
    int32 ProgressCount = 0;
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
        Test->TestTrue(TEXT("Partial progress can be observed before cancellation"), State->ProgressCount > 0);
        Test->TestFalse(TEXT("Progress never follows the terminal callback"), State->bProgressAfterTerminal);
        Test->TestFalse(TEXT("Cancellation removes the temporary file"), IFileManager::Get().FileExists(*(State->DestinationPath + TEXT(".tmp"))));
        Test->TestFalse(TEXT("Cancellation never publishes the final file"), IFileManager::Get().FileExists(*State->DestinationPath));
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FCancelledDownloadState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FFailingDownloadWriter final : public FArchive
{
public:
    FFailingDownloadWriter()
    {
        SetIsSaving(true);
    }

    virtual void Serialize(void* Data, int64 Num) override
    {
        SetError();
    }
};

struct FInvalidDownloadPathState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe> Transport;
    FString OwnedTempPath;
    int32 CompletionCount = 0;
    bool bMissingParentRejected = false;
    bool bOwnedTempRejected = false;
};

class FVerifyInvalidDownloadPaths final : public IAutomationLatentCommand
{
public:
    FVerifyInvalidDownloadPaths(
        TSharedRef<FInvalidDownloadPathState, ESPMode::ThreadSafe> InState,
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
        Test->TestTrue(TEXT("A missing destination parent is rejected"), State->bMissingParentRejected);
        Test->TestTrue(TEXT("An existing temporary owner is rejected"), State->bOwnedTempRejected);
        Test->TestTrue(TEXT("The existing temporary owner is preserved"), IFileManager::Get().FileExists(*State->OwnedTempPath));
        Test->TestEqual(TEXT("Invalid destinations do not reach the transport"), State->Transport->GetRequestCount(), 0);
        IFileManager::Get().Delete(*State->OwnedTempPath, false, true);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FInvalidDownloadPathState, ESPMode::ThreadSafe> State;
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
    State->RedirectDestinationPath = FPaths::ConvertRelativePathToFull(
        FPaths::CreateTempFilename(
            *FPaths::ProjectIntermediateDir(),
            TEXT("OpenPocketBaseRedirect-"),
            TEXT(".bin")));

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

    FOpenPocketBaseTransportScript ShortScript;
    ShortScript.Chunks = {DownloadBytes(TEXT("short"))};
    ShortScript.Response.bTransportSucceeded = true;
    ShortScript.Response.HttpStatus = 200;
    ShortScript.Response.Headers.Add(TEXT("Content-Length"), TEXT("10"));

    FOpenPocketBaseTransportScript RedirectScript;
    RedirectScript.Chunks = {DownloadBytes(TEXT("partial"))};
    RedirectScript.Response.bTransportSucceeded = true;
    RedirectScript.Response.HttpStatus = 200;
    RedirectScript.Response.EffectiveUrl = TEXT("https://evil.example.com/stolen");
    State->Transport->Enqueue(MoveTemp(RedirectScript));

    FOpenPocketBaseTransportScript TimeoutScript;
    TimeoutScript.Response.bTimedOut = true;
    TimeoutScript.Response.ErrorMessage = TEXT("Timed out");
    State->Transport->Enqueue(MoveTemp(TimeoutScript));
    State->Transport->Enqueue(MoveTemp(BoundScript));
    State->Transport->Enqueue(MoveTemp(ShortScript));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseFileDownloadOptions MemoryOptions;
    MemoryOptions.MaxBytes = 6;
    State->Client->Files().DynamicDownload(
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
        },
        {},
        [State](const FOpenPocketBaseTransferProgress& Progress)
        {
            State->bProgressOnGameThread = State->bProgressOnGameThread && IsInGameThread();
            State->MemoryProgress.Add(Progress);
        });

    FOpenPocketBaseFileDownloadOptions DiskOptions;
    DiskOptions.Target = EOpenPocketBaseFileDownloadTarget::File;
    DiskOptions.DestinationPath = State->DestinationPath;
    DiskOptions.MaxBytes = 1024;
    State->Client->Files().DynamicDownload(
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

    FOpenPocketBaseFileDownloadOptions RedirectOptions;
    RedirectOptions.Target = EOpenPocketBaseFileDownloadTarget::File;
    RedirectOptions.DestinationPath = State->RedirectDestinationPath;
    RedirectOptions.MaxBytes = 32;
    State->Client->Files().DynamicDownload(
        TEXT("tasks"),
        TEXT("record-1"),
        TEXT("redirect.bin"),
        RedirectOptions,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&& Result)
        {
            State->bRedirectRejected = !Result.IsSuccess() &&
                Result.GetError().Kind == EOpenPocketBaseErrorKind::Transport;
            ++State->CompletionCount;
        });

    FOpenPocketBaseFileDownloadOptions TimeoutOptions;
    TimeoutOptions.MaxBytes = 32;
    TimeoutOptions.RequestOptions.bRetryEligibleReads = true;
    TimeoutOptions.RequestOptions.MaxReadRetries = 5;
    State->Client->Files().DynamicDownload(
        TEXT("tasks"),
        TEXT("record-1"),
        TEXT("timeout.bin"),
        TimeoutOptions,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&& Result)
        {
            State->bTimeoutRejected = !Result.IsSuccess() &&
                Result.GetError().Kind == EOpenPocketBaseErrorKind::Timeout;
            ++State->CompletionCount;
        });

    FOpenPocketBaseFileDownloadOptions BoundOptions;
    BoundOptions.MaxBytes = 4;
    State->Client->Files().DynamicDownload(
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
    State->Client->Files().DynamicDownload(
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
    State->Client = CreateOpenPocketBaseTestClient(Config, Transport, Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseFileDownloadOptions Options;
    Options.Target = EOpenPocketBaseFileDownloadTarget::File;
    Options.DestinationPath = State->DestinationPath;
    Options.MaxBytes = 1024;
    FOpenPocketBaseRequestHandle Handle = State->Client->Files().DynamicDownload(
        TEXT("tasks"),
        TEXT("record-1"),
        TEXT("cancel.bin"),
        Options,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&& Result)
        {
            State->bCancelled = !Result.IsSuccess() &&
                Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled;
            State->bCompleted = true;
        },
        {},
        [State](const FOpenPocketBaseTransferProgress& Progress)
        {
            State->bProgressAfterTerminal = State->bProgressAfterTerminal || State->bCompleted;
            ++State->ProgressCount;
        });
    Handle.Cancel();

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyCancelledDownload(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseFileDownloadWriteFailureTest,
    "OpenPocketBase.Client.Files.RejectsDownloadWriteFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseFileDownloadWriteFailureTest::RunTest(const FString& Parameters)
{
    const FString TempPath = FPaths::ConvertRelativePathToFull(
        FPaths::CreateTempFilename(
            *FPaths::ProjectIntermediateDir(),
            TEXT("OpenPocketBaseFailingWriter-"),
            TEXT(".tmp")));
    IFileManager::Get().Delete(*TempPath, false, true);
    TSharedPtr<FOpenPocketBaseDownloadSink, ESPMode::ThreadSafe> Sink =
        FOpenPocketBaseDownloadSink::CreateForTesting(
            1024,
            TempPath,
            MakeUnique<FFailingDownloadWriter>());
    if (!TestTrue(TEXT("A failing download sink is created"), Sink.IsValid()))
    {
        return false;
    }

    const TArray<uint8> Bytes = DownloadBytes(TEXT("disk-full"));
    Sink->Receive(MakeArrayView(Bytes));
    FOpenPocketBaseHttpResponse Response;
    Response.bTransportSucceeded = true;
    Response.HttpStatus = 200;
    Response.RequestId = TEXT("write-failure");
    FOpenPocketBaseFileDownloadResult Result;
    FOpenPocketBaseError Error;
    TestFalse(TEXT("A destination write failure cannot finalize"), Sink->Finalize(Response, Result, Error));
    TestEqual(TEXT("A destination write failure is a transport error"), Error.Kind, EOpenPocketBaseErrorKind::Transport);
    TestFalse(TEXT("A destination write failure leaves no temporary file"), IFileManager::Get().FileExists(*TempPath));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseInvalidDownloadPathTest,
    "OpenPocketBase.Client.Files.RejectsInvalidDownloadDestinations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseInvalidDownloadPathTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FInvalidDownloadPathState, ESPMode::ThreadSafe> State =
        MakeShared<FInvalidDownloadPathState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FOpenPocketBaseScriptedTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseFileDownloadOptions MissingParentOptions;
    MissingParentOptions.Target = EOpenPocketBaseFileDownloadTarget::File;
    MissingParentOptions.DestinationPath = FPaths::Combine(
        FPaths::ProjectIntermediateDir(),
        TEXT("OpenPocketBaseMissingParent"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits),
        TEXT("download.bin"));
    State->Client->Files().DynamicDownload(
        TEXT("tasks"),
        TEXT("record-1"),
        TEXT("missing.bin"),
        MissingParentOptions,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&& Result)
        {
            State->bMissingParentRejected = !Result.IsSuccess() &&
                Result.GetError().Kind == EOpenPocketBaseErrorKind::InvalidArgument;
            ++State->CompletionCount;
        });

    const FString FinalPath = FPaths::ConvertRelativePathToFull(
        FPaths::CreateTempFilename(
            *FPaths::ProjectIntermediateDir(),
            TEXT("OpenPocketBaseOwnedTemp-"),
            TEXT(".bin")));
    IFileManager::Get().Delete(*FinalPath, false, true);
    State->OwnedTempPath = FinalPath + TEXT(".tmp");
    FFileHelper::SaveStringToFile(TEXT("owned"), *State->OwnedTempPath);
    FOpenPocketBaseFileDownloadOptions OwnedTempOptions;
    OwnedTempOptions.Target = EOpenPocketBaseFileDownloadTarget::File;
    OwnedTempOptions.DestinationPath = FinalPath;
    State->Client->Files().DynamicDownload(
        TEXT("tasks"),
        TEXT("record-1"),
        TEXT("owned.bin"),
        OwnedTempOptions,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&& Result)
        {
            State->bOwnedTempRejected = !Result.IsSuccess() &&
                Result.GetError().Kind == EOpenPocketBaseErrorKind::InvalidArgument;
            ++State->CompletionCount;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyInvalidDownloadPaths(State, this));
    return true;
}

#endif
