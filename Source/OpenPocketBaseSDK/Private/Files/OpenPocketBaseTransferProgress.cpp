#include "Files/OpenPocketBaseTransferProgress.h"

#include "Async/Async.h"
#include "Misc/ScopeLock.h"

namespace
{
constexpr int64 ProgressByteDelta = 64 * 1024;
constexpr double ProgressTimeDeltaSeconds = 0.1;

class FOpenPocketBaseProgressArchive final : public FArchive
{
public:
    FOpenPocketBaseProgressArchive(
        TSharedPtr<FArchive, ESPMode::ThreadSafe> InSource,
        TSharedRef<FOpenPocketBaseTransferProgressState, ESPMode::ThreadSafe> InProgress)
        : Source(MoveTemp(InSource))
        , Progress(MoveTemp(InProgress))
    {
        SetIsLoading(true);
        SetIsPersistent(true);
    }

    virtual void Serialize(void* Data, const int64 Num) override
    {
        int64 Position = 0;
        int64 Length = 0;
        {
            FScopeLock Lock(&Mutex);
            if (!Source.IsValid() || Num < 0)
            {
                SetError();
                return;
            }
            Source->Serialize(Data, Num);
            if (Source->IsError())
            {
                SetError();
                return;
            }
            Position = Source->Tell();
            Length = Source->TotalSize();
            MaxTransferredBytes = FMath::Max(MaxTransferredBytes, Position);
            Position = MaxTransferredBytes;
        }
        Progress->Report(
            Position,
            Length,
            EOpenPocketBaseTransferPhase::Uploading);
    }

    virtual int64 Tell() override
    {
        FScopeLock Lock(&Mutex);
        return Source.IsValid() ? Source->Tell() : 0;
    }

    virtual int64 TotalSize() override
    {
        FScopeLock Lock(&Mutex);
        return Source.IsValid() ? Source->TotalSize() : 0;
    }

    virtual void Seek(const int64 InPosition) override
    {
        FScopeLock Lock(&Mutex);
        if (!Source.IsValid())
        {
            SetError();
            return;
        }
        Source->Seek(InPosition);
        if (Source->IsError())
        {
            SetError();
        }
    }

private:
    FCriticalSection Mutex;
    TSharedPtr<FArchive, ESPMode::ThreadSafe> Source;
    TSharedRef<FOpenPocketBaseTransferProgressState, ESPMode::ThreadSafe> Progress;
    int64 MaxTransferredBytes = 0;
};
}

TSharedPtr<FOpenPocketBaseTransferProgressState, ESPMode::ThreadSafe>
FOpenPocketBaseTransferProgressState::Create(
    TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock,
    FOpenPocketBaseTransferProgressCallback Callback)
{
    if (!Callback)
    {
        return nullptr;
    }
    return MakeShareable(new FOpenPocketBaseTransferProgressState(
        MoveTemp(Clock),
        MoveTemp(Callback)));
}

FOpenPocketBaseTransferProgressState::FOpenPocketBaseTransferProgressState(
    TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> InClock,
    FOpenPocketBaseTransferProgressCallback InCallback)
    : Clock(MoveTemp(InClock))
    , Callback(MoveTemp(InCallback))
{
}

void FOpenPocketBaseTransferProgressState::Report(
    const int64 TransferredBytes,
    const TOptional<int64> TotalBytes,
    const EOpenPocketBaseTransferPhase Phase,
    const bool bForce)
{
    bool bScheduleDrain = false;
    {
        FScopeLock Lock(&Mutex);
        if (bStopped || TransferredBytes < 0)
        {
            return;
        }

        Pending.TransferredBytes = TransferredBytes;
        Pending.bHasTotalBytes = TotalBytes.IsSet();
        Pending.TotalBytes = TotalBytes.Get(0);
        Pending.Attempt = 1;
        Pending.Phase = Phase;
        bHasPending = true;

        const double Now = Clock->MonotonicSeconds();
        const bool bReachedByteDelta = TransferredBytes >= LastPublishedBytes &&
            TransferredBytes - LastPublishedBytes >= ProgressByteDelta;
        const bool bReachedTimeDelta = bPublished &&
            Now - LastPublishedSeconds >= ProgressTimeDeltaSeconds;
        if (!bDrainScheduled && (bForce || !bPublished || bReachedByteDelta || bReachedTimeDelta))
        {
            bDrainScheduled = true;
            bScheduleDrain = true;
        }
    }

    if (bScheduleDrain)
    {
        const TSharedRef<FOpenPocketBaseTransferProgressState, ESPMode::ThreadSafe> Self = AsShared();
        AsyncTask(
            ENamedThreads::GameThread,
            [Self]()
            {
                Self->Drain();
            });
    }
}

void FOpenPocketBaseTransferProgressState::Finish(
    const int64 TransferredBytes,
    const int64 TotalBytes)
{
    Report(
        TransferredBytes,
        TotalBytes,
        EOpenPocketBaseTransferPhase::Finalizing,
        true);
}

void FOpenPocketBaseTransferProgressState::Stop()
{
    FScopeLock Lock(&Mutex);
    bStopped = true;
    bHasPending = false;
    Callback.Reset();
}

void FOpenPocketBaseTransferProgressState::Drain()
{
    FOpenPocketBaseTransferProgress LocalProgress;
    FOpenPocketBaseTransferProgressCallback LocalCallback;
    {
        FScopeLock Lock(&Mutex);
        bDrainScheduled = false;
        if (bStopped || !bHasPending || !Callback)
        {
            return;
        }
        LocalProgress = Pending;
        LocalCallback = Callback;
        bHasPending = false;
        bPublished = true;
        LastPublishedBytes = LocalProgress.TransferredBytes;
        LastPublishedSeconds = Clock->MonotonicSeconds();
    }
    LocalCallback(LocalProgress);
}

TSharedPtr<FArchive, ESPMode::ThreadSafe> CreateOpenPocketBaseProgressArchive(
    TSharedPtr<FArchive, ESPMode::ThreadSafe> Source,
    TSharedRef<FOpenPocketBaseTransferProgressState, ESPMode::ThreadSafe> Progress)
{
    if (!Source.IsValid())
    {
        return nullptr;
    }
    return MakeShared<FOpenPocketBaseProgressArchive, ESPMode::ThreadSafe>(
        MoveTemp(Source),
        MoveTemp(Progress));
}
