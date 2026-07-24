#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClientLibrary.h"
#include "OpenPocketBaseStringLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseErrorBlueprintSurfaceTest,
    "OpenPocketBase.Blueprint.Errors.ExposesUsefulStructuredDetails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseErrorBlueprintSurfaceTest::RunTest(const FString& Parameters)
{
    UScriptStruct* ErrorStruct = FOpenPocketBaseError::StaticStruct();
    TestEqual(
        TEXT("Errors use the SDK break function"),
        ErrorStruct->GetMetaData(TEXT("HasNativeBreak")),
        FString(TEXT("/Script/OpenPocketBaseSDK.OpenPocketBaseClientLibrary.BreakError")));
    TestNotNull(TEXT("Error Code is reflected"), ErrorStruct->FindPropertyByName(TEXT("Code")));
    TestNotNull(TEXT("Error Message is reflected"), ErrorStruct->FindPropertyByName(TEXT("Message")));
    TestNull(TEXT("The old Server Code name is removed"), ErrorStruct->FindPropertyByName(TEXT("ServerCode")));
    TestNull(TEXT("The old Server Message name is removed"), ErrorStruct->FindPropertyByName(TEXT("ServerMessage")));

    const UFunction* BreakFunction = UOpenPocketBaseClientLibrary::StaticClass()->FindFunctionByName(
        TEXT("BreakError"));
    if (!TestNotNull(TEXT("The error break function exists"), BreakFunction))
    {
        return false;
    }
    TestTrue(
        TEXT("The error break function is a native Break Struct node"),
        BreakFunction->HasMetaData(TEXT("NativeBreakFunc")));
    TestFalse(
        TEXT("Every error output stays visible"),
        BreakFunction->HasMetaData(TEXT("AdvancedDisplay")));

    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::PocketBase;
    Error.HttpStatus = 400;
    Error.Code = TEXT("validation_failed");
    Error.Message = TEXT("One or more fields are invalid.");
    Error.bMayRetry = false;
    Error.RequestId = TEXT("request-123");
    FOpenPocketBaseFieldError& TitleError = Error.FieldErrors.Add(TEXT("title"));
    TitleError.Code = TEXT("validation_required");
    TitleError.Message = TEXT("Title is required.");

    EOpenPocketBaseErrorKind Kind = EOpenPocketBaseErrorKind::None;
    int32 HttpStatus = 0;
    FString Code;
    FString Message;
    TMap<FString, FOpenPocketBaseFieldError> FieldErrors;
    bool bMayRetry = true;
    FString RequestId;
    UOpenPocketBaseClientLibrary::BreakError(
        Error,
        Kind,
        HttpStatus,
        Code,
        Message,
        FieldErrors,
        bMayRetry,
        RequestId);

    TestEqual(TEXT("Break Error exposes Kind"), Kind, Error.Kind);
    TestEqual(TEXT("Break Error exposes HTTP Status"), HttpStatus, 400);
    TestEqual(TEXT("Break Error exposes Code"), Code, Error.Code);
    TestEqual(TEXT("Break Error exposes Message"), Message, Error.Message);
    TestEqual(TEXT("Break Error exposes Field Errors"), FieldErrors.Num(), 1);
    TestFalse(TEXT("Break Error exposes May Retry"), bMayRetry);
    TestEqual(TEXT("Break Error exposes Request ID"), RequestId, Error.RequestId);

    FOpenPocketBaseFieldError FoundFieldError;
    TestTrue(
        TEXT("Field errors can be found by PocketBase field name"),
        UOpenPocketBaseClientLibrary::TryGetFieldError(
            Error,
            TEXT("title"),
            FoundFieldError));
    TestEqual(
        TEXT("The field error message is preserved"),
        FoundFieldError.Message,
        FString(TEXT("Title is required.")));
    TestFalse(
        TEXT("Unknown field errors report not found"),
        UOpenPocketBaseClientLibrary::TryGetFieldError(
            Error,
            TEXT("unknown"),
            FoundFieldError));

    const FString Printed = UOpenPocketBaseStringLibrary::Conv_OpenPocketBaseErrorToString(Error);
    TestTrue(TEXT("Printed errors use Code"), Printed.Contains(TEXT("\"code\"")));
    TestTrue(TEXT("Printed errors use Message"), Printed.Contains(TEXT("\"message\"")));
    TestFalse(TEXT("Printed errors omit Server Code"), Printed.Contains(TEXT("serverCode")));
    TestFalse(TEXT("Printed errors omit Server Message"), Printed.Contains(TEXT("serverMessage")));
    return true;
}

#endif
