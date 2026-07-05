#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Realtime/OpenPocketBaseSseParser.h"

namespace
{
TArray<uint8> SseUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

TArray<uint8> MakeSseFixture()
{
    TArray<uint8> Bytes = {0xef, 0xbb, 0xbf};
    Bytes.Append(SseUtf8(
        TEXT(": heartbeat\r\n")
        TEXT("retry: 1500\r")
        TEXT("id: event-7\n")
        TEXT("event: custom\r\n")
        TEXT("data: first\r\n")
        TEXT("data: snowman \u2603 and emoji \U0001f600\r\n")
        TEXT("\r\n")
        TEXT("data: second\n\n")));
    return Bytes;
}

bool VerifyFixtureEvents(
    FAutomationTestBase& Test,
    const TArray<OpenPocketBase::Realtime::FSseEvent>& Events,
    const FString& Prefix)
{
    if (!Test.TestEqual(*(Prefix + TEXT(" event count")), Events.Num(), 2))
    {
        return false;
    }
    Test.TestEqual(*(Prefix + TEXT(" first event name")), Events[0].Event, FString(TEXT("custom")));
    Test.TestEqual(
        *(Prefix + TEXT(" multiline data")),
        Events[0].Data,
        FString(TEXT("first\nsnowman \u2603 and emoji \U0001f600")));
    Test.TestEqual(*(Prefix + TEXT(" event ID")), Events[0].Id, FString(TEXT("event-7")));
    Test.TestTrue(*(Prefix + TEXT(" retry is present")), Events[0].RetryMilliseconds.IsSet());
    Test.TestEqual(*(Prefix + TEXT(" retry value")), Events[0].RetryMilliseconds.Get(0), 1500);
    Test.TestEqual(*(Prefix + TEXT(" default event name")), Events[1].Event, FString(TEXT("message")));
    Test.TestEqual(*(Prefix + TEXT(" second data")), Events[1].Data, FString(TEXT("second")));
    Test.TestEqual(*(Prefix + TEXT(" persistent event ID")), Events[1].Id, FString(TEXT("event-7")));
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSseBoundaryTest,
    "OpenPocketBase.Realtime.Parser.FuzzesEveryChunkBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSseBoundaryTest::RunTest(const FString& Parameters)
{
    const TArray<uint8> Fixture = MakeSseFixture();
    for (int32 Split = 0; Split <= Fixture.Num(); ++Split)
    {
        OpenPocketBase::Realtime::FSseParser Parser;
        TArray<OpenPocketBase::Realtime::FSseEvent> Events;
        FOpenPocketBaseError Error;
        const bool bFirst = Parser.Feed(
            MakeArrayView(Fixture.GetData(), Split),
            Events,
            Error);
        const bool bSecond = Parser.Feed(
            MakeArrayView(Fixture.GetData() + Split, Fixture.Num() - Split),
            Events,
            Error);
        const bool bFinished = Parser.Finish(Error);
        const FString Prefix = FString::Printf(TEXT("Split %d"), Split);
        TestTrue(*(Prefix + TEXT(" parses")), bFirst && bSecond && bFinished);
        VerifyFixtureEvents(*this, Events, Prefix);
    }

    OpenPocketBase::Realtime::FSseParser ByteParser;
    TArray<OpenPocketBase::Realtime::FSseEvent> ByteEvents;
    FOpenPocketBaseError Error;
    for (const uint8 Byte : Fixture)
    {
        if (!ByteParser.Feed(MakeArrayView(&Byte, 1), ByteEvents, Error))
        {
            AddError(TEXT("The byte-by-byte fixture failed to parse."));
            return false;
        }
    }
    TestTrue(TEXT("The byte-by-byte fixture finishes"), ByteParser.Finish(Error));
    VerifyFixtureEvents(*this, ByteEvents, TEXT("Byte-by-byte"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSseMalformedTest,
    "OpenPocketBase.Realtime.Parser.RejectsMalformedAndUnboundedInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSseMalformedTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseError Error;
    TArray<OpenPocketBase::Realtime::FSseEvent> Events;

    OpenPocketBase::Realtime::FSseParser InvalidUtf8;
    const TArray<uint8> Overlong = {0xc0, 0xaf};
    TestFalse(TEXT("Overlong UTF-8 is rejected"), InvalidUtf8.Feed(Overlong, Events, Error));
    TestEqual(TEXT("Malformed UTF-8 is a serialization error"), Error.Kind, EOpenPocketBaseErrorKind::Serialization);

    OpenPocketBase::Realtime::FSseParser TruncatedUtf8;
    const TArray<uint8> Truncated = {0xe2, 0x82};
    TestTrue(TEXT("A partial UTF-8 suffix waits for more bytes"), TruncatedUtf8.Feed(Truncated, Events, Error));
    TestFalse(TEXT("A partial UTF-8 suffix fails at EOF"), TruncatedUtf8.Finish(Error));

    OpenPocketBase::Realtime::FSseLimits LineLimits;
    LineLimits.MaxLineCharacters = 8;
    OpenPocketBase::Realtime::FSseParser LongLine(LineLimits);
    const TArray<uint8> LongLineBytes = SseUtf8(TEXT("data: 123456789\n\n"));
    TestFalse(TEXT("An oversized line is rejected"), LongLine.Feed(LongLineBytes, Events, Error));

    OpenPocketBase::Realtime::FSseLimits EventLimits;
    EventLimits.MaxEventsPerFeed = 2;
    OpenPocketBase::Realtime::FSseParser TooManyEvents(EventLimits);
    const TArray<uint8> ManyEvents = SseUtf8(TEXT("data: one\n\ndata: two\n\ndata: three\n\n"));
    Events.Reset();
    TestFalse(TEXT("Too many events in one feed are rejected"), TooManyEvents.Feed(ManyEvents, Events, Error));
    return true;
}

#endif
