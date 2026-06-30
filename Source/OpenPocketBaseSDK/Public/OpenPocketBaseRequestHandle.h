#pragma once

#include "Templates/SharedPointer.h"

class FOpenPocketBaseRequestState;

class OPENPOCKETBASESDK_API FOpenPocketBaseRequestHandle
{
public:
    FOpenPocketBaseRequestHandle() = default;

    void Cancel() const;
    bool IsActive() const;
    uint64 GetRequestId() const;

private:
    explicit FOpenPocketBaseRequestHandle(
        TSharedPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> InState);

    TSharedPtr<FOpenPocketBaseRequestState, ESPMode::ThreadSafe> State;

    friend class FOpenPocketBaseClient;
    friend struct FOpenPocketBaseClientImplAccess;
};
