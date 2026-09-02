#pragma once

#include "Transport/OpenPocketBaseTransport.h"
#include "Templates/SharedPointer.h"

struct OPENPOCKETBASESDKEDITOR_API FOpenPocketBaseTransportScript
{
    TArray<TArray<uint8>> Chunks;
    FOpenPocketBaseHttpResponse Response;
    bool bHoldCompletion = false;
    bool bCompleteAfterCancel = false;
};

class OPENPOCKETBASESDKEDITOR_API FOpenPocketBaseScriptedTransport final : public IOpenPocketBaseTransport
{
public:
    FOpenPocketBaseScriptedTransport();
    virtual ~FOpenPocketBaseScriptedTransport() override;

    void Enqueue(FOpenPocketBaseTransportScript&& Script);
    void SetIncrementalResponseStreamingAvailable(bool bAvailable);
    bool CompleteNextHeld();
    int32 GetRequestCount() const;
    int32 GetCancelCount() const;
    bool TryGetRequest(int32 Index, FOpenPocketBaseHttpRequest& OutRequest) const;

    virtual bool IsIncrementalResponseStreamingAvailable(FString& OutReason) const override;

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override;

private:
    class FState;
    TSharedRef<FState, ESPMode::ThreadSafe> State;
};
