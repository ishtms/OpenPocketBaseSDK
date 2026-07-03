#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

class OPENPOCKETBASESDK_API FOpenPocketBaseClockHandle
{
public:
    FOpenPocketBaseClockHandle() = default;

    explicit FOpenPocketBaseClockHandle(TUniqueFunction<void()> InCancel)
        : CancelAction(MoveTemp(InCancel))
    {
    }

    FOpenPocketBaseClockHandle(FOpenPocketBaseClockHandle&&) = default;
    FOpenPocketBaseClockHandle& operator=(FOpenPocketBaseClockHandle&&) = default;
    FOpenPocketBaseClockHandle(const FOpenPocketBaseClockHandle&) = delete;
    FOpenPocketBaseClockHandle& operator=(const FOpenPocketBaseClockHandle&) = delete;

    void Cancel()
    {
        if (CancelAction)
        {
            TUniqueFunction<void()> Action = MoveTemp(CancelAction);
            Action();
        }
    }

private:
    TUniqueFunction<void()> CancelAction;
};

class OPENPOCKETBASESDK_API IOpenPocketBaseClock
{
public:
    virtual ~IOpenPocketBaseClock() = default;

    virtual FDateTime UtcNow() const = 0;
    virtual double MonotonicSeconds() const = 0;
    virtual FOpenPocketBaseClockHandle Schedule(
        double DelaySeconds,
        TUniqueFunction<void()> Callback) = 0;
};

OPENPOCKETBASESDK_API TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe>
CreateOpenPocketBaseClock();
