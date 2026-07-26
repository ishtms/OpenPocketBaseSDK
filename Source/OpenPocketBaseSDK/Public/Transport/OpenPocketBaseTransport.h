#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"
#include "Containers/ArrayView.h"
#include "Serialization/Archive.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

struct OPENPOCKETBASESDK_API FOpenPocketBaseHttpRequest
{
    FString Method;
    FString Url;
    TMap<FString, FString> Headers;
    TArray<uint8> Body;
    TSharedPtr<FArchive, ESPMode::ThreadSafe> BodyStream;
    int64 BodyLength = 0;
    FString RequestId;
    double TotalTimeoutSeconds = 30.0;
    double ActivityTimeoutSeconds = 15.0;
    bool bStreamResponse = false;
};

enum class EOpenPocketBaseHttpTimeoutSource : uint8
{
    None,
    Total,
    Activity
};

struct OPENPOCKETBASESDK_API FOpenPocketBaseHttpResponse
{
    bool bTransportSucceeded = false;
    bool bTimedOut = false;
    EOpenPocketBaseHttpTimeoutSource TimeoutSource = EOpenPocketBaseHttpTimeoutSource::None;
    double TimeoutSeconds = 0.0;
    int32 HttpStatus = 0;
    TMap<FString, FString> Headers;
    TArray<uint8> Body;
    FString ErrorMessage;
    FString RequestId;
    FString EffectiveUrl;
};

using FOpenPocketBaseHttpChunkCallback = TUniqueFunction<void(TArrayView<const uint8>)>;
using FOpenPocketBaseHttpCompleteCallback = TUniqueFunction<void(FOpenPocketBaseHttpResponse&&)>;

class OPENPOCKETBASESDK_API FOpenPocketBaseTransportHandle
{
public:
    FOpenPocketBaseTransportHandle() = default;

    explicit FOpenPocketBaseTransportHandle(TUniqueFunction<void()> InCancel)
        : CancelAction(MoveTemp(InCancel))
    {
    }

    FOpenPocketBaseTransportHandle(FOpenPocketBaseTransportHandle&&) = default;
    FOpenPocketBaseTransportHandle& operator=(FOpenPocketBaseTransportHandle&&) = default;
    FOpenPocketBaseTransportHandle(const FOpenPocketBaseTransportHandle&) = delete;
    FOpenPocketBaseTransportHandle& operator=(const FOpenPocketBaseTransportHandle&) = delete;

    void Cancel()
    {
        if (CancelAction)
        {
            TUniqueFunction<void()> Action = MoveTemp(CancelAction);
            Action();
        }
    }

    bool IsValid() const
    {
        return static_cast<bool>(CancelAction);
    }

private:
    TUniqueFunction<void()> CancelAction;
};

class OPENPOCKETBASESDK_API IOpenPocketBaseTransport
{
public:
    virtual ~IOpenPocketBaseTransport() = default;

    virtual bool IsIncrementalResponseStreamingAvailable(FString& OutReason) const
    {
        OutReason = TEXT("The configured transport does not declare incremental response streaming support.");
        return false;
    }

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) = 0;
};
