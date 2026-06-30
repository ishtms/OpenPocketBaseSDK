#include "OpenPocketBaseRequestHandle.h"

#include "Request/OpenPocketBaseRequestState.h"

FOpenPocketBaseRequestHandle::FOpenPocketBaseRequestHandle(
    TSharedPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> InState)
    : State(MoveTemp(InState))
{
}

void FOpenPocketBaseRequestHandle::Cancel() const
{
    if (State.IsValid())
    {
        State->Cancel();
    }
}

bool FOpenPocketBaseRequestHandle::IsActive() const
{
    return State.IsValid() && State->IsActive();
}

uint64 FOpenPocketBaseRequestHandle::GetRequestId() const
{
    return State.IsValid() ? State->GetRequestId() : 0;
}
