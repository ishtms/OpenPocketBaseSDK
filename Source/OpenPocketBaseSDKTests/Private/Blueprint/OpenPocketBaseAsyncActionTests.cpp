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
#include "OpenPocketBaseBlueprintClient.h"
#include "OpenPocketBaseRecordLibrary.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRegisterPasswordMismatchTest,
    "OpenPocketBase.Blueprint.Async.RegisterPasswordMismatchNamesField",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRegisterPasswordMismatchTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError CreateError;
    UOpenPocketBaseClient* Client =
        UOpenPocketBaseClient::Create(GetTransientPackage(), Config, CreateError);
    if (!TestNotNull(TEXT("The Blueprint client is created"), Client))
    {
        return false;
    }

    UOpenPocketBaseSchema* Schema = NewObject<UOpenPocketBaseSchema>();
    Schema->SchemaId = FGuid(377, 233, 144, 89);
    FOpenPocketBaseSchemaCollection Users;
    Users.Id = TEXT("users_id");
    Users.Name = TEXT("users");
    Users.Type = EOpenPocketBaseCollectionType::Auth;
    Schema->Collections = {Users};

    FOpenPocketBaseCollectionRef UsersRef;
    TestTrue(
        TEXT("The auth collection reference is created"),
        Schema->MakeCollectionRef(Users.Id, UsersRef));
    const FOpenPocketBaseCollection UsersCollection = Client->Collection(UsersRef);
    const FOpenPocketBaseRecordBody UserFields =
        UOpenPocketBaseRecordLibrary::NewRecordBody(UsersCollection);

    UOpenPocketBaseAsyncActionTestReceiver* Receiver =
        NewObject<UOpenPocketBaseAsyncActionTestReceiver>();
    UOpenPocketBaseRegisterUserAsyncAction* Action =
        UOpenPocketBaseRegisterUserAsyncAction::RegisterUser(
            UsersCollection,
            UserFields,
            TEXT("correct-horse-battery"),
            TEXT("different-password"),
            {});
    Action->Failed.AddDynamic(
        Receiver,
        &UOpenPocketBaseAsyncActionTestReceiver::HandleRecordFailure);
    Action->Activate();

    TestTrue(TEXT("A password mismatch reaches the Failed pin"), Receiver->bFailed);
    TestEqual(
        TEXT("The failure reports the password mismatch"),
        Receiver->Error.Message,
        FString(TEXT("Password and Confirm Password do not match.")));
    TestTrue(
        TEXT("The error identifies Confirm Password"),
        Receiver->Error.FieldErrors.Contains(TEXT("passwordConfirm")));
    Client->Shutdown();
    return true;
}

#endif
