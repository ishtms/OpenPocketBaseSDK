// Copyright 2026 Ishtmeet Singh.

#include "Clock/OpenPocketBaseClock.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"

namespace
{
class FSystemOpenPocketBaseClock final : public IOpenPocketBaseClock
{
public:
    virtual FDateTime UtcNow() const override
    {
        return FDateTime::UtcNow();
    }

    virtual double MonotonicSeconds() const override
    {
        return FPlatformTime::Seconds();
    }

    virtual FOpenPocketBaseClockHandle Schedule(
        const double DelaySeconds,
        TUniqueFunction<void()> Callback) override
    {
        const FTSTicker::FDelegateHandle TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            TEXT("OpenPocketBase.Clock"),
            static_cast<float>(FMath::Max(0.0, DelaySeconds)),
            [Callback = MoveTemp(Callback)](float) mutable
            {
                if (Callback)
                {
                    TUniqueFunction<void()> LocalCallback = MoveTemp(Callback);
                    LocalCallback();
                }
                return false;
            });
        return FOpenPocketBaseClockHandle(
            [TickerHandle]()
            {
                FTSTicker::RemoveTicker(TickerHandle);
            });
    }
};
}

TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> CreateOpenPocketBaseClock()
{
    return MakeShared<FSystemOpenPocketBaseClock, ESPMode::ThreadSafe>();
}
