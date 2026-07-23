#include "Blueprint/OpenPocketBaseAsyncActionTestReceiver.h"

void UOpenPocketBaseAsyncActionTestReceiver::HandleRecordFailure(
    FOpenPocketBaseRecord Record,
    FOpenPocketBaseError InError)
{
    bFailed = true;
    Error = MoveTemp(InError);
}

#if WITH_DEV_AUTOMATION_TESTS

#include "AsyncActions/OpenPocketBaseRecordAsyncActions.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseInvalidAsyncInputTest,
    "OpenPocketBase.Blueprint.Async.InvalidInputsBroadcastFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseInvalidAsyncInputTest::RunTest(const FString& Parameters)
{
    UOpenPocketBaseAsyncActionTestReceiver* Receiver =
        NewObject<UOpenPocketBaseAsyncActionTestReceiver>();
    UOpenPocketBaseGetRecordAsyncAction* Action =
        UOpenPocketBaseGetRecordAsyncAction::GetRecord(
            {},
            TEXT("record00000001"),
            {});
    Action->Failed.AddDynamic(
        Receiver,
        &UOpenPocketBaseAsyncActionTestReceiver::HandleRecordFailure);
    Action->Activate();

    TestTrue(TEXT("An invalid async input reaches the Failed pin"), Receiver->bFailed);
    TestEqual(
        TEXT("The failure explains the invalid argument"),
        Receiver->Error.Kind,
        EOpenPocketBaseErrorKind::InvalidArgument);
    return true;
}

#endif
