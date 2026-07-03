#include "Request/OpenPocketBaseRequestState.h"

#include "Async/Async.h"
#include "Misc/ScopeLock.h"

FOpenPocketBaseRequestState::FOpenPocketBaseRequestState(
    const uint64 InRequestId,
    TUniqueFunction<void()> InOnCancelled,
    TUniqueFunction<void()> InOnTerminal)
    : RequestId(InRequestId)
    , OnCancelled(MoveTemp(InOnCancelled))
    , OnTerminal(MoveTemp(InOnTerminal))
{
}

uint64 FOpenPocketBaseRequestState::GetRequestId() const
{
    return RequestId;
}

EOpenPocketBaseRequestState FOpenPocketBaseRequestState::GetState() const
{
    return State.load(std::memory_order_acquire);
}

bool FOpenPocketBaseRequestState::IsActive() const
{
    return !IsTerminal(GetState());
}

bool FOpenPocketBaseRequestState::TryMarkSending()
{
    EOpenPocketBaseRequestState Expected = EOpenPocketBaseRequestState::Pending;
    if (State.compare_exchange_strong(
        Expected,
        EOpenPocketBaseRequestState::Sending,
        std::memory_order_acq_rel))
    {
        return true;
    }

    Expected = EOpenPocketBaseRequestState::WaitingForRetry;
    if (State.compare_exchange_strong(
        Expected,
        EOpenPocketBaseRequestState::Sending,
        std::memory_order_acq_rel))
    {
        return true;
    }

    Expected = EOpenPocketBaseRequestState::WaitingForAuthRefresh;
    return State.compare_exchange_strong(
        Expected,
        EOpenPocketBaseRequestState::Sending,
        std::memory_order_acq_rel);
}

bool FOpenPocketBaseRequestState::TryMarkWaitingForAuthRefresh()
{
    EOpenPocketBaseRequestState Expected = EOpenPocketBaseRequestState::Pending;
    if (State.compare_exchange_strong(
        Expected,
        EOpenPocketBaseRequestState::WaitingForAuthRefresh,
        std::memory_order_acq_rel))
    {
        return true;
    }

    Expected = EOpenPocketBaseRequestState::Sending;
    return State.compare_exchange_strong(
        Expected,
        EOpenPocketBaseRequestState::WaitingForAuthRefresh,
        std::memory_order_acq_rel);
}

bool FOpenPocketBaseRequestState::TryMarkWaitingForRetry()
{
    EOpenPocketBaseRequestState Expected = EOpenPocketBaseRequestState::Sending;
    return State.compare_exchange_strong(
        Expected,
        EOpenPocketBaseRequestState::WaitingForRetry,
        std::memory_order_acq_rel);
}

void FOpenPocketBaseRequestState::AttachTransportHandle(FOpenPocketBaseTransportHandle&& InHandle)
{
    AttachStageHandle(
        MoveTemp(InHandle),
        EOpenPocketBaseRequestState::Sending,
        false);
}

void FOpenPocketBaseRequestState::AttachAuthRefreshHandle(
    FOpenPocketBaseTransportHandle&& InHandle)
{
    AttachStageHandle(
        MoveTemp(InHandle),
        EOpenPocketBaseRequestState::WaitingForAuthRefresh,
        true);
}

void FOpenPocketBaseRequestState::AttachRetryHandle(FOpenPocketBaseTransportHandle&& InHandle)
{
    AttachStageHandle(
        MoveTemp(InHandle),
        EOpenPocketBaseRequestState::WaitingForRetry,
        true);
}

void FOpenPocketBaseRequestState::AttachStageHandle(
    FOpenPocketBaseTransportHandle&& InHandle,
    const EOpenPocketBaseRequestState ExpectedState,
    const bool bCancelOnMismatch)
{
    FOpenPocketBaseTransportHandle HandleToCancel;
    {
        FScopeLock Lock(&Mutex);
        const EOpenPocketBaseRequestState CurrentState = GetState();
        if (CurrentState != ExpectedState)
        {
            if (bCancelOnMismatch || CurrentState == EOpenPocketBaseRequestState::Cancelled)
            {
                HandleToCancel = MoveTemp(InHandle);
            }
        }
        else
        {
            TransportHandle = MoveTemp(InHandle);
        }
    }

    HandleToCancel.Cancel();
}

bool FOpenPocketBaseRequestState::TryComplete(
    const EOpenPocketBaseRequestState TerminalState,
    TUniqueFunction<void()> Completion)
{
    check(IsTerminal(TerminalState));
    if (!TrySetTerminal(TerminalState))
    {
        return false;
    }

    TUniqueFunction<void()> TerminalCallback;
    {
        FScopeLock Lock(&Mutex);
        TransportHandle = FOpenPocketBaseTransportHandle();
        TerminalCallback = MoveTemp(OnTerminal);
        OnCancelled.Reset();
    }

    if (TerminalCallback)
    {
        TerminalCallback();
    }
    Dispatch(MoveTemp(Completion));
    return true;
}

void FOpenPocketBaseRequestState::Cancel()
{
    if (!TrySetTerminal(EOpenPocketBaseRequestState::Cancelled))
    {
        return;
    }

    FOpenPocketBaseTransportHandle HandleToCancel;
    TUniqueFunction<void()> CancelledCallback;
    TUniqueFunction<void()> TerminalCallback;
    {
        FScopeLock Lock(&Mutex);
        HandleToCancel = MoveTemp(TransportHandle);
        CancelledCallback = MoveTemp(OnCancelled);
        TerminalCallback = MoveTemp(OnTerminal);
    }

    HandleToCancel.Cancel();
    if (TerminalCallback)
    {
        TerminalCallback();
    }
    Dispatch(MoveTemp(CancelledCallback));
}

bool FOpenPocketBaseRequestState::IsTerminal(const EOpenPocketBaseRequestState InState)
{
    return InState == EOpenPocketBaseRequestState::Succeeded ||
        InState == EOpenPocketBaseRequestState::Failed ||
        InState == EOpenPocketBaseRequestState::Cancelled;
}

bool FOpenPocketBaseRequestState::TrySetTerminal(const EOpenPocketBaseRequestState TerminalState)
{
    EOpenPocketBaseRequestState Expected = State.load(std::memory_order_acquire);
    while (!IsTerminal(Expected))
    {
        if (State.compare_exchange_weak(
                Expected,
                TerminalState,
                std::memory_order_acq_rel,
                std::memory_order_acquire))
        {
            return true;
        }
    }

    return false;
}

void FOpenPocketBaseRequestState::Dispatch(TUniqueFunction<void()> Completion)
{
    if (Completion)
    {
        AsyncTask(ENamedThreads::GameThread, MoveTemp(Completion));
    }
}
