#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Transport/OpenPocketBaseResponseBuffer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseResponseBufferTest,
    "OpenPocketBase.Client.Transport.StopsBufferingAtResponseLimit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseResponseBufferTest::RunTest(const FString& Parameters)
{
    const TArray<uint8> Chunk = {1, 2, 3};
    FOpenPocketBaseResponseBuffer Exact(6);
    TestTrue(TEXT("The first chunk fits"), Exact.Append(MakeArrayView(Chunk)));
    TestTrue(TEXT("The limit itself is accepted"), Exact.Append(MakeArrayView(Chunk)));
    TestEqual(TEXT("Both accepted chunks are retained"), Exact.TakeBytes().Num(), 6);

    FOpenPocketBaseResponseBuffer Limited(5);
    TestTrue(TEXT("The first partial response fits"), Limited.Append(MakeArrayView(Chunk)));
    TestFalse(TEXT("The overflowing chunk is rejected before append"), Limited.Append(MakeArrayView(Chunk)));
    TestTrue(TEXT("Overflow remains classified"), Limited.HasExceededLimit());
    TestEqual(TEXT("Received bytes include the rejected chunk"), Limited.GetReceivedBytes(), 6LL);
    TestTrue(TEXT("Overflow releases the buffered body"), Limited.TakeBytes().IsEmpty());
    TestFalse(TEXT("Later chunks remain rejected"), Limited.Append(MakeArrayView(Chunk)));

    FOpenPocketBaseResponseBuffer FirstChunk(2);
    TestFalse(TEXT("An oversized first chunk is never buffered"), FirstChunk.Append(MakeArrayView(Chunk)));
    TestTrue(TEXT("An oversized first chunk leaves no body"), FirstChunk.TakeBytes().IsEmpty());
    return true;
}

#endif
