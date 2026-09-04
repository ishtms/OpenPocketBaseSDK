// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "Clock/OpenPocketBaseClock.h"
#include "OpenPocketBaseFile.h"
#include "Serialization/Archive.h"

class FOpenPocketBaseTransferProgressState final
    : public TSharedFromThis<FOpenPocketBaseTransferProgressState, ESPMode::ThreadSafe>
{
public:
    static TSharedPtr<FOpenPocketBaseTransferProgressState, ESPMode::ThreadSafe> Create(
        TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock,
        FOpenPocketBaseTransferProgressCallback Callback);

    void Report(
        int64 TransferredBytes,
        TOptional<int64> TotalBytes,
        EOpenPocketBaseTransferPhase Phase,
        bool bForce = false);
    void Finish(int64 TransferredBytes, int64 TotalBytes);
    void Stop();

private:
    FOpenPocketBaseTransferProgressState(
        TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> InClock,
        FOpenPocketBaseTransferProgressCallback InCallback);

    void Drain();

    mutable FCriticalSection Mutex;
    TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock;
    FOpenPocketBaseTransferProgressCallback Callback;
    FOpenPocketBaseTransferProgress Pending;
    int64 LastPublishedBytes = 0;
    double LastPublishedSeconds = 0;
    bool bHasPending = false;
    bool bPublished = false;
    bool bDrainScheduled = false;
    bool bStopped = false;
};

OPENPOCKETBASESDK_API TSharedPtr<FArchive, ESPMode::ThreadSafe>
CreateOpenPocketBaseProgressArchive(
    TSharedPtr<FArchive, ESPMode::ThreadSafe> Source,
    TSharedRef<FOpenPocketBaseTransferProgressState, ESPMode::ThreadSafe> Progress);
