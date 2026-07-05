#include "Realtime/OpenPocketBaseSseParser.h"

namespace
{
void AppendCodepointToString(const uint32 Codepoint, FString& OutString)
{
    if constexpr (sizeof(TCHAR) == 4)
    {
        OutString.AppendChar(static_cast<TCHAR>(Codepoint));
    }
    else if (Codepoint <= 0xffff)
    {
        OutString.AppendChar(static_cast<TCHAR>(Codepoint));
    }
    else
    {
        const uint32 Supplementary = Codepoint - 0x10000;
        OutString.AppendChar(static_cast<TCHAR>(0xd800 + (Supplementary >> 10)));
        OutString.AppendChar(static_cast<TCHAR>(0xdc00 + (Supplementary & 0x3ff)));
    }
}

bool ContainsNull(const FString& Value)
{
    for (const TCHAR Character : Value)
    {
        if (Character == 0)
        {
            return true;
        }
    }
    return false;
}

bool TryParseRetry(const FString& Value, int32& OutMilliseconds)
{
    if (Value.IsEmpty())
    {
        return false;
    }
    int64 Parsed = 0;
    for (const TCHAR Character : Value)
    {
        if (Character < TEXT('0') || Character > TEXT('9'))
        {
            return false;
        }
        const int32 Digit = Character - TEXT('0');
        if (Parsed > (MAX_int32 - Digit) / 10)
        {
            return false;
        }
        Parsed = Parsed * 10 + Digit;
    }
    OutMilliseconds = static_cast<int32>(Parsed);
    return true;
}
}

namespace OpenPocketBase::Realtime
{
FSseParser::FSseParser(FSseLimits InLimits)
    : Limits(InLimits)
{
    bLimitsValid = Limits.MaxLineCharacters > 0 &&
        Limits.MaxLineCharacters <= 1024 * 1024 &&
        Limits.MaxEventDataCharacters > 0 &&
        Limits.MaxEventDataCharacters <= 16 * 1024 * 1024 &&
        Limits.MaxEventsPerFeed > 0 && Limits.MaxEventsPerFeed <= 16384;
    PendingUtf8.Reserve(4);
}

bool FSseParser::Feed(
    const TArrayView<const uint8> Bytes,
    TArray<FSseEvent>& OutEvents,
    FOpenPocketBaseError& OutError)
{
    if (bFinished || bFailed || !bLimitsValid)
    {
        return Fail(TEXT("The SSE parser is not available for more input."), OutError);
    }

    int32 EventsProduced = 0;
    for (const uint8 Byte : Bytes)
    {
        PendingUtf8.Add(Byte);
        uint32 Codepoint = 0;
        const EUtf8DecodeResult DecodeResult = TryDecodeUtf8(PendingUtf8, Codepoint);
        if (DecodeResult == EUtf8DecodeResult::NeedMore)
        {
            continue;
        }
        if (DecodeResult == EUtf8DecodeResult::Invalid)
        {
            return Fail(TEXT("The SSE stream contains malformed UTF-8."), OutError);
        }

        PendingUtf8.Reset();
        if (!AppendCodepoint(Codepoint, OutEvents, EventsProduced, OutError))
        {
            return false;
        }
    }

    OutError = FOpenPocketBaseError();
    return true;
}

bool FSseParser::Finish(FOpenPocketBaseError& OutError)
{
    if (bFinished || bFailed || !bLimitsValid)
    {
        return Fail(TEXT("The SSE parser cannot finish in its current state."), OutError);
    }
    if (!PendingUtf8.IsEmpty())
    {
        return Fail(TEXT("The SSE stream ended inside a UTF-8 character."), OutError);
    }

    if (!PendingLine.IsEmpty())
    {
        TArray<FSseEvent> IgnoredEvents;
        int32 EventsProduced = 0;
        if (!ProcessLine(IgnoredEvents, EventsProduced, OutError))
        {
            return false;
        }
    }
    bFinished = true;
    OutError = FOpenPocketBaseError();
    return true;
}

FSseParser::EUtf8DecodeResult FSseParser::TryDecodeUtf8(
    const TArrayView<const uint8> Bytes,
    uint32& OutCodepoint)
{
    if (Bytes.IsEmpty())
    {
        return EUtf8DecodeResult::NeedMore;
    }

    const uint8 First = Bytes[0];
    int32 Required = 0;
    if (First <= 0x7f)
    {
        Required = 1;
    }
    else if (First >= 0xc2 && First <= 0xdf)
    {
        Required = 2;
    }
    else if (First >= 0xe0 && First <= 0xef)
    {
        Required = 3;
    }
    else if (First >= 0xf0 && First <= 0xf4)
    {
        Required = 4;
    }
    else
    {
        return EUtf8DecodeResult::Invalid;
    }

    if (Bytes.Num() < Required)
    {
        return EUtf8DecodeResult::NeedMore;
    }
    if (Bytes.Num() > Required)
    {
        return EUtf8DecodeResult::Invalid;
    }
    for (int32 Index = 1; Index < Required; ++Index)
    {
        if (Bytes[Index] < 0x80 || Bytes[Index] > 0xbf)
        {
            return EUtf8DecodeResult::Invalid;
        }
    }
    if ((First == 0xe0 && Bytes[1] < 0xa0) ||
        (First == 0xed && Bytes[1] > 0x9f) ||
        (First == 0xf0 && Bytes[1] < 0x90) ||
        (First == 0xf4 && Bytes[1] > 0x8f))
    {
        return EUtf8DecodeResult::Invalid;
    }

    if (Required == 1)
    {
        OutCodepoint = First;
    }
    else if (Required == 2)
    {
        OutCodepoint = ((First & 0x1f) << 6) | (Bytes[1] & 0x3f);
    }
    else if (Required == 3)
    {
        OutCodepoint = ((First & 0x0f) << 12) |
            ((Bytes[1] & 0x3f) << 6) |
            (Bytes[2] & 0x3f);
    }
    else
    {
        OutCodepoint = ((First & 0x07) << 18) |
            ((Bytes[1] & 0x3f) << 12) |
            ((Bytes[2] & 0x3f) << 6) |
            (Bytes[3] & 0x3f);
    }
    return EUtf8DecodeResult::Complete;
}

bool FSseParser::AppendCodepoint(
    const uint32 Codepoint,
    TArray<FSseEvent>& OutEvents,
    int32& InOutEventsProduced,
    FOpenPocketBaseError& OutError)
{
    if (bFirstCodepoint)
    {
        bFirstCodepoint = false;
        if (Codepoint == 0xfeff)
        {
            return true;
        }
    }

    if (Codepoint == TEXT('\r'))
    {
        bPreviousWasCarriageReturn = true;
        return ProcessLine(OutEvents, InOutEventsProduced, OutError);
    }
    if (Codepoint == TEXT('\n'))
    {
        if (bPreviousWasCarriageReturn)
        {
            bPreviousWasCarriageReturn = false;
            return true;
        }
        return ProcessLine(OutEvents, InOutEventsProduced, OutError);
    }

    bPreviousWasCarriageReturn = false;
    AppendCodepointToString(Codepoint, PendingLine);
    if (PendingLine.Len() > Limits.MaxLineCharacters)
    {
        return Fail(TEXT("An SSE line exceeded its configured character bound."), OutError);
    }
    return true;
}

bool FSseParser::ProcessLine(
    TArray<FSseEvent>& OutEvents,
    int32& InOutEventsProduced,
    FOpenPocketBaseError& OutError)
{
    FString Line = MoveTemp(PendingLine);
    PendingLine.Reset();
    if (Line.IsEmpty())
    {
        if (!DataLines.IsEmpty())
        {
            if (InOutEventsProduced >= Limits.MaxEventsPerFeed)
            {
                return Fail(TEXT("An SSE chunk produced too many events."), OutError);
            }
            FSseEvent Event;
            Event.Event = EventName.IsEmpty() ? TEXT("message") : MoveTemp(EventName);
            Event.Data = FString::Join(DataLines, TEXT("\n"));
            Event.Id = LastEventId;
            Event.RetryMilliseconds = RetryMilliseconds;
            OutEvents.Add(MoveTemp(Event));
            ++InOutEventsProduced;
        }
        EventName.Reset();
        DataLines.Reset();
        EventDataCharacters = 0;
        return true;
    }
    if (Line.StartsWith(TEXT(":")))
    {
        return true;
    }

    FString Field;
    FString Value;
    int32 ColonIndex = INDEX_NONE;
    if (Line.FindChar(TEXT(':'), ColonIndex))
    {
        Field = Line.Left(ColonIndex);
        Value = Line.Mid(ColonIndex + 1);
        if (Value.StartsWith(TEXT(" ")))
        {
            Value.RightChopInline(1, EAllowShrinking::No);
        }
    }
    else
    {
        Field = MoveTemp(Line);
    }

    if (Field == TEXT("event"))
    {
        EventName = MoveTemp(Value);
    }
    else if (Field == TEXT("data"))
    {
        const int32 SeparatorCharacters = DataLines.IsEmpty() ? 0 : 1;
        if (Value.Len() > Limits.MaxEventDataCharacters ||
            EventDataCharacters > Limits.MaxEventDataCharacters - Value.Len() - SeparatorCharacters)
        {
            return Fail(TEXT("An SSE event exceeded its configured data bound."), OutError);
        }
        EventDataCharacters += Value.Len() + SeparatorCharacters;
        DataLines.Add(MoveTemp(Value));
    }
    else if (Field == TEXT("id") && !ContainsNull(Value))
    {
        LastEventId = MoveTemp(Value);
    }
    else if (Field == TEXT("retry"))
    {
        int32 ParsedRetry = 0;
        if (TryParseRetry(Value, ParsedRetry))
        {
            RetryMilliseconds = ParsedRetry;
        }
    }
    return true;
}

bool FSseParser::Fail(const TCHAR* Message, FOpenPocketBaseError& OutError)
{
    bFailed = true;
    OutError = FOpenPocketBaseError();
    OutError.Kind = EOpenPocketBaseErrorKind::Serialization;
    OutError.ServerMessage = Message;
    return false;
}
}
