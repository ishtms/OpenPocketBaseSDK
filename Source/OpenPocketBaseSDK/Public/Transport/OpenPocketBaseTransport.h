#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"
#include "Containers/ArrayView.h"
#include "Templates/Function.h"

struct OPENPOCKETBASESDK_API FOpenPocketBaseHttpRequest
{
    FString Method;
    FString Url;
    TMap<FString, FString> Headers;
    TArray<uint8> Body;
    FString RequestId;
    double TotalTimeoutSeconds = 30.0;
    double ActivityTimeoutSeconds = 15.0;
    bool bStreamResponse = false;
};

struct OPENPOCKETBASESDK_API FOpenPocketBaseHttpResponse
{
    bool bTransportSucceeded = false;
    bool bTimedOut = false;
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

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) = 0;
};
