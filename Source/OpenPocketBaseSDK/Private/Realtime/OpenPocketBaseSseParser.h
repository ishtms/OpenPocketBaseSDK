#pragma once

#include "OpenPocketBaseError.h"

namespace OpenPocketBase::Realtime
{
struct OPENPOCKETBASESDK_API FSseLimits
{
    int32 MaxLineCharacters = 64 * 1024;
    int32 MaxEventDataCharacters = 1024 * 1024;
    int32 MaxEventsPerFeed = 1024;
};

struct OPENPOCKETBASESDK_API FSseEvent
{
    FString Event;
    FString Data;
    FString Id;
    TOptional<int32> RetryMilliseconds;
};

class OPENPOCKETBASESDK_API FSseParser final
{
public:
    explicit FSseParser(FSseLimits InLimits = {});

    bool Feed(
        TArrayView<const uint8> Bytes,
        TArray<FSseEvent>& OutEvents,
        FOpenPocketBaseError& OutError);
    bool Finish(FOpenPocketBaseError& OutError);

private:
    enum class EUtf8DecodeResult : uint8
    {
        Complete,
        NeedMore,
        Invalid
    };

    static EUtf8DecodeResult TryDecodeUtf8(
        TArrayView<const uint8> Bytes,
        uint32& OutCodepoint);
    bool AppendCodepoint(
        uint32 Codepoint,
        TArray<FSseEvent>& OutEvents,
        int32& InOutEventsProduced,
        FOpenPocketBaseError& OutError);
    bool ProcessLine(
        TArray<FSseEvent>& OutEvents,
        int32& InOutEventsProduced,
        FOpenPocketBaseError& OutError);
    bool Fail(FString Message, FOpenPocketBaseError& OutError);

    FSseLimits Limits;
    TArray<uint8> PendingUtf8;
    FString PendingLine;
    FString EventName;
    TArray<FString> DataLines;
    FString LastEventId;
    TOptional<int32> RetryMilliseconds;
    int32 EventDataCharacters = 0;
    bool bFirstCodepoint = true;
    bool bPreviousWasCarriageReturn = false;
    bool bLimitsValid = true;
    bool bFailed = false;
    bool bFinished = false;
};
}
