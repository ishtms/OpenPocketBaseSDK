#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseRecord.h"
#include "Serialization/OpenPocketBaseJson.h"

#include <limits>

namespace
{
TArray<uint8> ErrorMessageTestUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRecordPageErrorMessageTest,
    "OpenPocketBase.Serialization.Errors.NameInvalidRecordPageItem",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRecordPageErrorMessageTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseHttpResponse Response;
    Response.bTransportSucceeded = true;
    Response.HttpStatus = 200;
    Response.Body = ErrorMessageTestUtf8(
        TEXT("{\"page\":1,\"perPage\":5,\"totalItems\":1,\"totalPages\":1,\"items\":[{\"title\":\"Missing ID\"}]}"));

    TOpenPocketBaseResult<FOpenPocketBaseRecordPage> Result =
        OpenPocketBase::Json::ParseRecordPageResponse(Response);
    TestFalse(TEXT("The malformed page fails"), Result.IsSuccess());
    TestEqual(
        TEXT("The error kind is Serialization"),
        Result.GetError().Kind,
        EOpenPocketBaseErrorKind::Serialization);
    TestTrue(
        TEXT("The message names the failing item"),
        Result.GetError().Message.Contains(TEXT("item 0")));
    TestTrue(
        TEXT("The message names the missing ID"),
        Result.GetError().Message.Contains(TEXT("missing its non-empty string 'id'")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSkippedTotalsParsingTest,
    "OpenPocketBase.Serialization.Errors.AcceptsSkippedRecordTotals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSkippedTotalsParsingTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseHttpResponse Response;
    Response.bTransportSucceeded = true;
    Response.HttpStatus = 200;
    Response.Body = ErrorMessageTestUtf8(
        TEXT("{\"page\":1,\"perPage\":1,\"totalItems\":-1,\"totalPages\":-1,\"items\":[{\"id\":\"task123\"}]}"));

    TOpenPocketBaseResult<FOpenPocketBaseRecordPage> Result =
        OpenPocketBase::Json::ParseRecordPageResponse(Response);
    TestTrue(TEXT("Skipped totals remain valid"), Result.IsSuccess());
    if (Result.IsSuccess())
    {
        TestFalse(TEXT("Skipped Total Items stays absent"), Result.GetValue().bHasTotalItems);
        TestFalse(TEXT("Skipped Total Pages stays absent"), Result.GetValue().bHasTotalPages);
        TestEqual(TEXT("The record remains available"), Result.GetValue().Items.Num(), 1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseDynamicBodyErrorMessageTest,
    "OpenPocketBase.Records.Errors.ExplainDynamicBodyFailures",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseDynamicBodyErrorMessageTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseRecordBody EmptyFieldBody;
    EmptyFieldBody.SetDynamicStringField(TEXT(""), TEXT("value"));
    TestFalse(TEXT("An empty dynamic field fails"), EmptyFieldBody.IsValid());
    TestTrue(
        TEXT("The field error explains the allowed name"),
        EmptyFieldBody.ErrorMessage.Contains(TEXT("letters, numbers, and underscores")));

    FOpenPocketBaseRecordBody NonFiniteBody;
    NonFiniteBody.SetDynamicNumberField(
        TEXT("score"),
        std::numeric_limits<double>::quiet_NaN());
    TestFalse(TEXT("A non-finite number fails"), NonFiniteBody.IsValid());
    TestTrue(
        TEXT("The number error explains the invalid value class"),
        NonFiniteBody.ErrorMessage.Contains(TEXT("NaN and infinity")));
    return true;
}

#endif
