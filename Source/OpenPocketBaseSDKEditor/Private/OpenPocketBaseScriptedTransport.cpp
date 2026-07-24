#include "OpenPocketBaseScriptedTransport.h"

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

namespace
{
class FPendingScriptedResponse
{
public:
    explicit FPendingScriptedResponse(
        FOpenPocketBaseHttpResponse&& InResponse,
        FOpenPocketBaseHttpCompleteCallback&& InOnComplete,
        const bool bInCompleteAfterCancel)
        : Response(MoveTemp(InResponse))
        , OnComplete(MoveTemp(InOnComplete))
        , bCompleteAfterCancel(bInCompleteAfterCancel)
    {
    }

    void Cancel()
    {
        FScopeLock Lock(&Mutex);
        bCancelled = true;
        if (!bCompleteAfterCancel)
        {
            bCompleted = true;
            OnComplete.Reset();
        }
    }

    bool Complete()
    {
        FOpenPocketBaseHttpCompleteCallback Callback;
        FOpenPocketBaseHttpResponse LocalResponse;
        {
            FScopeLock Lock(&Mutex);
            if (bCompleted || (bCancelled && !bCompleteAfterCancel))
            {
                return false;
            }

            bCompleted = true;
            Callback = MoveTemp(OnComplete);
            LocalResponse = MoveTemp(Response);
        }

        if (Callback)
        {
            Callback(MoveTemp(LocalResponse));
        }
        return true;
    }

    bool CompletesAfterCancel() const
    {
        return bCompleteAfterCancel;
    }

private:
    mutable FCriticalSection Mutex;
    FOpenPocketBaseHttpResponse Response;
    FOpenPocketBaseHttpCompleteCallback OnComplete;
    bool bCompleteAfterCancel = false;
    bool bCancelled = false;
    bool bCompleted = false;
};
}

class FOpenPocketBaseScriptedTransport::FState
{
public:
    void Enqueue(FOpenPocketBaseTransportScript&& Script)
    {
        FScopeLock Lock(&Mutex);
        Scripts.Add(MoveTemp(Script));
    }

    bool Dequeue(FOpenPocketBaseTransportScript& OutScript)
    {
        FScopeLock Lock(&Mutex);
        if (Scripts.IsEmpty())
        {
            return false;
        }

        OutScript = MoveTemp(Scripts[0]);
        Scripts.RemoveAt(0, EAllowShrinking::No);
        return true;
    }

    void RecordRequest(const FOpenPocketBaseHttpRequest& Request)
    {
        FScopeLock Lock(&Mutex);
        Requests.Add(Request);
    }

    void Hold(const TSharedRef<FPendingScriptedResponse, ESPMode::ThreadSafe>& Pending)
    {
        FScopeLock Lock(&Mutex);
        HeldResponses.Add(Pending);
    }

    bool CompleteNextHeld()
    {
        TSharedPtr<FPendingScriptedResponse, ESPMode::ThreadSafe> Pending;
        {
            FScopeLock Lock(&Mutex);
            if (HeldResponses.IsEmpty())
            {
                return false;
            }

            Pending = HeldResponses[0];
            HeldResponses.RemoveAt(0, EAllowShrinking::No);
        }

        Pending->Complete();
        return true;
    }

    void Cancel(const TSharedRef<FPendingScriptedResponse, ESPMode::ThreadSafe>& Pending)
    {
        {
            FScopeLock Lock(&Mutex);
            ++CancelCount;
            if (!Pending->CompletesAfterCancel())
            {
                HeldResponses.Remove(Pending);
            }
        }
        Pending->Cancel();
    }

    int32 GetRequestCount() const
    {
        FScopeLock Lock(&Mutex);
        return Requests.Num();
    }

    int32 GetCancelCount() const
    {
        FScopeLock Lock(&Mutex);
        return CancelCount;
    }

    bool TryGetRequest(const int32 Index, FOpenPocketBaseHttpRequest& OutRequest) const
    {
        FScopeLock Lock(&Mutex);
        if (!Requests.IsValidIndex(Index))
        {
            return false;
        }

        OutRequest = Requests[Index];
        return true;
    }

private:
    mutable FCriticalSection Mutex;
    TArray<FOpenPocketBaseTransportScript> Scripts;
    TArray<FOpenPocketBaseHttpRequest> Requests;
    TArray<TSharedRef<FPendingScriptedResponse, ESPMode::ThreadSafe>> HeldResponses;
    int32 CancelCount = 0;
};

FOpenPocketBaseScriptedTransport::FOpenPocketBaseScriptedTransport()
    : State(MakeShared<FState, ESPMode::ThreadSafe>())
{
}

FOpenPocketBaseScriptedTransport::~FOpenPocketBaseScriptedTransport() = default;

void FOpenPocketBaseScriptedTransport::Enqueue(FOpenPocketBaseTransportScript&& Script)
{
    State->Enqueue(MoveTemp(Script));
}

bool FOpenPocketBaseScriptedTransport::CompleteNextHeld()
{
    return State->CompleteNextHeld();
}

int32 FOpenPocketBaseScriptedTransport::GetRequestCount() const
{
    return State->GetRequestCount();
}

int32 FOpenPocketBaseScriptedTransport::GetCancelCount() const
{
    return State->GetCancelCount();
}

bool FOpenPocketBaseScriptedTransport::TryGetRequest(
    const int32 Index,
    FOpenPocketBaseHttpRequest& OutRequest) const
{
    return State->TryGetRequest(Index, OutRequest);
}

FOpenPocketBaseTransportHandle FOpenPocketBaseScriptedTransport::Send(
    FOpenPocketBaseHttpRequest&& Request,
    FOpenPocketBaseHttpChunkCallback OnChunk,
    FOpenPocketBaseHttpCompleteCallback OnComplete)
{
    State->RecordRequest(Request);

    FOpenPocketBaseTransportScript Script;
    if (!State->Dequeue(Script))
    {
        Script.Response.ErrorMessage = TEXT("The scripted transport received a request with no queued response. Queue the expected response before starting the SDK operation in this test.");
    }

    if (Script.Response.RequestId.IsEmpty())
    {
        Script.Response.RequestId = Request.RequestId;
    }
    if (Script.Response.EffectiveUrl.IsEmpty())
    {
        Script.Response.EffectiveUrl = Request.Url;
    }

    if (OnChunk)
    {
        for (const TArray<uint8>& Chunk : Script.Chunks)
        {
            OnChunk(MakeArrayView(Chunk));
        }
    }

    const TSharedRef<FPendingScriptedResponse, ESPMode::ThreadSafe> Pending =
        MakeShared<FPendingScriptedResponse, ESPMode::ThreadSafe>(
            MoveTemp(Script.Response),
            MoveTemp(OnComplete),
            Script.bCompleteAfterCancel);
    if (Script.bHoldCompletion)
    {
        State->Hold(Pending);
    }
    else
    {
        Pending->Complete();
    }

    const TSharedRef<FState, ESPMode::ThreadSafe> SharedState = State;
    return FOpenPocketBaseTransportHandle(
        [SharedState, Pending]()
        {
            SharedState->Cancel(Pending);
        });
}
