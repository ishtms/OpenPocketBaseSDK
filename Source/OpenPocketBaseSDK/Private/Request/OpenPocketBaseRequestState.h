// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "HAL/CriticalSection.h"
#include "OpenPocketBaseRequestHandle.h"
#include "Templates/Function.h"
#include "Transport/OpenPocketBaseTransport.h"

#include <atomic>

enum class EOpenPocketBaseRequestState : uint8
{
    Pending,
    WaitingForAuthRefresh,
    Sending,
    WaitingForRetry,
    Succeeded,
    Failed,
    Cancelled
};

class FOpenPocketBaseRequestState final
    : public TSharedFromThis<FOpenPocketBaseRequestState, ESPMode::ThreadSafe>
{
public:
    FOpenPocketBaseRequestState(
        uint64 InRequestId,
        TUniqueFunction<void()> InOnCancelling,
        TUniqueFunction<void()> InOnCancelled,
        TUniqueFunction<void()> InOnTerminal);

    uint64 GetRequestId() const;
    EOpenPocketBaseRequestState GetState() const;
    bool IsActive() const;

    bool TryMarkSending();
    bool TryMarkWaitingForAuthRefresh();
    bool TryMarkWaitingForRetry();
    void AttachTransportHandle(FOpenPocketBaseTransportHandle&& InHandle);
    void AttachAuthRefreshHandle(FOpenPocketBaseTransportHandle&& InHandle);
    void AttachRetryHandle(FOpenPocketBaseTransportHandle&& InHandle);
    bool TryComplete(EOpenPocketBaseRequestState TerminalState, TUniqueFunction<void()> Completion);
    void Cancel();

private:
    static bool IsTerminal(EOpenPocketBaseRequestState State);
    bool TrySetTerminal(EOpenPocketBaseRequestState TerminalState);
    void AttachStageHandle(
        FOpenPocketBaseTransportHandle&& InHandle,
        EOpenPocketBaseRequestState ExpectedState,
        bool bCancelOnMismatch);
    static void Dispatch(TUniqueFunction<void()> Completion);

    uint64 RequestId;
    std::atomic<EOpenPocketBaseRequestState> State = EOpenPocketBaseRequestState::Pending;
    mutable FCriticalSection Mutex;
    FOpenPocketBaseTransportHandle TransportHandle;
    TUniqueFunction<void()> OnCancelling;
    TUniqueFunction<void()> OnCancelled;
    TUniqueFunction<void()> OnTerminal;
};
