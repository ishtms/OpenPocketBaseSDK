#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseFile.h"
#include "OpenPocketBaseFilter.h"

namespace
{
struct FPinnedServerState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    int32 CompletionCount = 0;
    bool bAuthSucceeded = false;
    bool bGetOneSucceeded = false;
    bool bGetListSucceeded = false;
    bool bGetFullListSucceeded = false;
    bool bCreateSucceeded = false;
    bool bUpdateSucceeded = false;
    bool bFirstSucceeded = false;
    bool bDeleteSucceeded = false;
    bool bBatchSucceeded = false;
    bool bBatchRollbackSucceeded = false;
    bool bRefreshSucceeded = false;
    FString AuthRecordId;
    FString RecordTitle;
    FString CreatedRecordId;
    FString FirstRecordId;
    int32 ListItems = 0;
    TArray<FString> Errors;
};

class FVerifyPinnedServer final : public IAutomationLatentCommand
{
public:
    FVerifyPinnedServer(
        const TSharedRef<FPinnedServerState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (State->CompletionCount != 7)
        {
            return false;
        }

        for (const FString& Error : State->Errors)
        {
            Test->AddError(Error);
        }
        Test->TestTrue(TEXT("Password login succeeds against v0.39.11"), State->bAuthSucceeded);
        Test->TestTrue(TEXT("Get One succeeds against v0.39.11"), State->bGetOneSucceeded);
        Test->TestTrue(TEXT("Get List succeeds against v0.39.11"), State->bGetListSucceeded);
        Test->TestTrue(TEXT("Bounded full list succeeds against v0.39.11"), State->bGetFullListSucceeded);
        Test->TestTrue(TEXT("Create succeeds against v0.39.11"), State->bCreateSucceeded);
        Test->TestTrue(TEXT("Update succeeds against v0.39.11"), State->bUpdateSucceeded);
        Test->TestTrue(TEXT("First match succeeds against v0.39.11"), State->bFirstSucceeded);
        Test->TestTrue(TEXT("Delete succeeds against v0.39.11"), State->bDeleteSucceeded);
        Test->TestTrue(TEXT("Transactional batch succeeds against v0.39.11"), State->bBatchSucceeded);
        Test->TestTrue(TEXT("Failed transactional batch rolls back against v0.39.11"), State->bBatchRollbackSucceeded);
        Test->TestTrue(TEXT("Auth Refresh succeeds against v0.39.11"), State->bRefreshSucceeded);
        Test->TestEqual(
            TEXT("The seeded auth record is returned"),
            State->AuthRecordId,
            FString(TEXT("user00000000001")));
        Test->TestEqual(
            TEXT("The seeded record title is returned"),
            State->RecordTitle,
            FString(TEXT("Ship the Unreal SDK")));
        Test->TestEqual(TEXT("The seeded list has one item"), State->ListItems, 1);
        Test->TestEqual(
            TEXT("Create accepts a deterministic record ID"),
            State->CreatedRecordId,
            FString(TEXT("task00000000002")));
        Test->TestEqual(
            TEXT("First match returns the updated record"),
            State->FirstRecordId,
            FString(TEXT("task00000000002")));
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FPinnedServerState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

FString DescribeIntegrationError(const TCHAR* Operation, const FOpenPocketBaseError& Error)
{
    return FString::Printf(
        TEXT("%s failed: kind=%d status=%d code=%s message=%s request=%s"),
        Operation,
        static_cast<int32>(Error.Kind),
        Error.HttpStatus,
        *Error.ServerCode,
        *Error.ServerMessage,
        *Error.RequestId);
}

void CompleteCrudFailure(
    const TSharedRef<FPinnedServerState, ESPMode::ThreadSafe>& State,
    const TCHAR* Operation,
    const FOpenPocketBaseError& Error)
{
    State->Errors.Add(DescribeIntegrationError(Operation, Error));
    ++State->CompletionCount;
}

void DeleteIntegrationRecord(
    const TSharedRef<FPinnedServerState, ESPMode::ThreadSafe>& State)
{
    State->Client->Collection(TEXT("sdk_tasks")).Delete(
        TEXT("task00000000002"),
        [State](TOpenPocketBaseResult<bool>&& Result)
        {
            State->bDeleteSucceeded = Result.IsSuccess() && Result.GetValue();
            if (!Result.IsSuccess())
            {
                State->Errors.Add(DescribeIntegrationError(TEXT("Delete"), Result.GetError()));
            }
            ++State->CompletionCount;
        });
}

void GetFirstIntegrationRecord(
    const TSharedRef<FPinnedServerState, ESPMode::ThreadSafe>& State)
{
    FOpenPocketBaseFilterParams FilterParams;
    FilterParams.AddString(TEXT("id"), TEXT("task00000000002"));
    FString Filter;
    FOpenPocketBaseError FilterError;
    if (!FOpenPocketBaseFilter::TryBind(TEXT("id = {:id}"), FilterParams, Filter, FilterError))
    {
        CompleteCrudFailure(State, TEXT("Bind Filter"), FilterError);
        return;
    }

    FOpenPocketBaseRecordOptions Options;
    Options.Fields = {TEXT("id"), TEXT("title:excerpt(12,true)"), TEXT("score")};
    State->Client->Collection(TEXT("sdk_tasks")).GetFirstListItem(
        MoveTemp(Filter),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bFirstSucceeded = Result.IsSuccess();
            if (!Result.IsSuccess())
            {
                CompleteCrudFailure(State, TEXT("Get First"), Result.GetError());
                return;
            }
            State->FirstRecordId = Result.GetValue().Id;
            DeleteIntegrationRecord(State);
        },
        MoveTemp(Options));
}

void UpdateIntegrationRecord(
    const TSharedRef<FPinnedServerState, ESPMode::ThreadSafe>& State)
{
    FOpenPocketBaseRecordBody Body;
    Body.SetStringField(TEXT("title"), TEXT("Updated integration task"));
    Body.SetNumberField(TEXT("score"), 2.0, EOpenPocketBaseFieldModifier::Append);
    State->Client->Collection(TEXT("sdk_tasks")).Update(
        TEXT("task00000000002"),
        MoveTemp(Body),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bUpdateSucceeded = Result.IsSuccess();
            if (!Result.IsSuccess())
            {
                CompleteCrudFailure(State, TEXT("Update"), Result.GetError());
                return;
            }
            GetFirstIntegrationRecord(State);
        });
}

void CreateIntegrationRecord(
    const TSharedRef<FPinnedServerState, ESPMode::ThreadSafe>& State)
{
    FOpenPocketBaseRecordBody Body;
    Body.SetStringField(TEXT("id"), TEXT("task00000000002"));
    Body.SetStringField(TEXT("title"), TEXT("Created integration task"));
    Body.SetNumberField(TEXT("score"), 3.0);
    State->Client->Collection(TEXT("sdk_tasks")).Create(
        MoveTemp(Body),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bCreateSucceeded = Result.IsSuccess();
            if (!Result.IsSuccess())
            {
                CompleteCrudFailure(State, TEXT("Create"), Result.GetError());
                return;
            }
            State->CreatedRecordId = Result.GetValue().Id;
            UpdateIntegrationRecord(State);
        });
}

void VerifyRolledBackBatchRecord(
    const TSharedRef<FPinnedServerState, ESPMode::ThreadSafe>& State)
{
    State->Client->Collection(TEXT("sdk_tasks")).GetOne(
        TEXT("task00000000005"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bBatchRollbackSucceeded =
                !Result.IsSuccess() && Result.GetError().HttpStatus == 404;
            if (!State->bBatchRollbackSucceeded)
            {
                State->Errors.Add(Result.IsSuccess()
                    ? TEXT("Batch rollback failed: the first record was committed.")
                    : DescribeIntegrationError(TEXT("Verify batch rollback"), Result.GetError()));
            }
            ++State->CompletionCount;
        });
}

void RunFailingIntegrationBatch(
    const TSharedRef<FPinnedServerState, ESPMode::ThreadSafe>& State)
{
    FOpenPocketBaseRecordBody FirstBody;
    FirstBody.SetStringField(TEXT("id"), TEXT("task00000000005"));
    FirstBody.SetStringField(TEXT("title"), TEXT("Must be rolled back"));
    FOpenPocketBaseRecordBody InvalidBody;
    InvalidBody.SetStringField(TEXT("id"), TEXT("task00000000006"));

    FOpenPocketBaseBatchRequest Batch;
    Batch.AddCreate(TEXT("sdk_tasks"), MoveTemp(FirstBody));
    Batch.AddCreate(TEXT("sdk_tasks"), MoveTemp(InvalidBody));
    State->Client->SendBatch(
        MoveTemp(Batch),
        [State](TOpenPocketBaseResult<FOpenPocketBaseBatchResult>&& Result)
        {
            if (Result.IsSuccess())
            {
                State->Errors.Add(TEXT("Invalid batch unexpectedly succeeded."));
                ++State->CompletionCount;
                return;
            }
            if (Result.GetError().HttpStatus != 400)
            {
                State->Errors.Add(DescribeIntegrationError(TEXT("Failing batch"), Result.GetError()));
                ++State->CompletionCount;
                return;
            }
            VerifyRolledBackBatchRecord(State);
        });
}

void RunSuccessfulIntegrationBatch(
    const TSharedRef<FPinnedServerState, ESPMode::ThreadSafe>& State)
{
    FOpenPocketBaseRecordBody CreateBody;
    CreateBody.SetStringField(TEXT("id"), TEXT("task00000000003"));
    CreateBody.SetStringField(TEXT("title"), TEXT("Batch create"));
    FOpenPocketBaseRecordBody UpdateBody;
    UpdateBody.SetStringField(TEXT("title"), TEXT("Batch update"));
    FOpenPocketBaseRecordBody UpsertBody;
    UpsertBody.SetStringField(TEXT("id"), TEXT("task00000000004"));
    UpsertBody.SetStringField(TEXT("title"), TEXT("Batch upsert"));

    FOpenPocketBaseBatchRequest Batch;
    Batch.AddCreate(TEXT("sdk_tasks"), MoveTemp(CreateBody));
    Batch.AddUpdate(TEXT("sdk_tasks"), TEXT("task00000000003"), MoveTemp(UpdateBody));
    Batch.AddUpsert(TEXT("sdk_tasks"), MoveTemp(UpsertBody));
    Batch.AddDelete(TEXT("sdk_tasks"), TEXT("task00000000004"));
    Batch.AddDelete(TEXT("sdk_tasks"), TEXT("task00000000003"));
    State->Client->SendBatch(
        MoveTemp(Batch),
        [State](TOpenPocketBaseResult<FOpenPocketBaseBatchResult>&& Result)
        {
            State->bBatchSucceeded = Result.IsSuccess() && Result.GetValue().Results.Num() == 5;
            if (!Result.IsSuccess())
            {
                State->Errors.Add(DescribeIntegrationError(TEXT("Successful batch"), Result.GetError()));
                ++State->CompletionCount;
                return;
            }
            RunFailingIntegrationBatch(State);
        });
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBasePinnedServerTest,
    "OpenPocketBase.Integration.V03911.MinimalVerticalSlice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBasePinnedServerTest::RunTest(const FString& Parameters)
{
    const FString BaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENPOCKETBASE_TEST_URL"));
    if (BaseUrl.IsEmpty())
    {
        AddInfo(TEXT("OPENPOCKETBASE_TEST_URL is not set; the pinned-server test was not requested."));
        return true;
    }

    const TSharedRef<FPinnedServerState, ESPMode::ThreadSafe> State =
        MakeShared<FPinnedServerState, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = BaseUrl;
    Config.ProfileName = TEXT("integration-v03911");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, Error);
    if (!TestNotNull(TEXT("The integration client is created"), State->Client.Get()))
    {
        AddError(Error.ServerMessage);
        return false;
    }

    State->Client->Collection(TEXT("sdk_users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("correct-horse-battery"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result)
        {
            State->bAuthSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                State->AuthRecordId = Result.GetValue().Record.Id;
                CreateIntegrationRecord(State);
                RunSuccessfulIntegrationBatch(State);
                State->Client->RefreshAuth(
                    [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& RefreshResult)
                    {
                        State->bRefreshSucceeded = RefreshResult.IsSuccess();
                        if (!RefreshResult.IsSuccess())
                        {
                            State->Errors.Add(DescribeIntegrationError(
                                TEXT("Auth Refresh"),
                                RefreshResult.GetError()));
                        }
                        ++State->CompletionCount;
                    });
            }
            else
            {
                State->Errors.Add(DescribeIntegrationError(TEXT("Password login"), Result.GetError()));
                ++State->CompletionCount;
            }
            ++State->CompletionCount;
        });

    State->Client->Collection(TEXT("sdk_tasks")).GetOne(
        TEXT("task00000000001"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bGetOneSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                Result.GetValue().Data.JsonObject->TryGetStringField(TEXT("title"), State->RecordTitle);
            }
            else
            {
                State->Errors.Add(DescribeIntegrationError(TEXT("Get One"), Result.GetError()));
            }
            ++State->CompletionCount;
        });

    FOpenPocketBaseListOptions ListOptions;
    ListOptions.Page = 1;
    ListOptions.PerPage = 30;
    State->Client->Collection(TEXT("sdk_tasks")).GetList(
        ListOptions,
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecordPage>&& Result)
        {
            State->bGetListSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                State->ListItems = Result.GetValue().Items.Num();
            }
            else
            {
                State->Errors.Add(DescribeIntegrationError(TEXT("Get List"), Result.GetError()));
            }
            ++State->CompletionCount;
        });

    FOpenPocketBaseFullListOptions FullListOptions;
    FullListOptions.ListOptions.PerPage = 30;
    FullListOptions.ListOptions.bSkipTotal = true;
    FullListOptions.MaxPages = 1;
    State->Client->Collection(TEXT("sdk_tasks")).GetFullList(
        FullListOptions,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFullListResult>&& Result)
        {
            State->bGetFullListSucceeded = Result.IsSuccess() &&
                Result.GetValue().Items.Num() >= 1 && Result.GetValue().bReachedEnd;
            if (!Result.IsSuccess())
            {
                State->Errors.Add(DescribeIntegrationError(TEXT("Get Full List"), Result.GetError()));
            }
            ++State->CompletionCount;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyPinnedServer(State, this));
    return true;
}

namespace
{
struct FPinnedRemainingAuthState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    bool bCompleted = false;
    bool bMethodsSucceeded = false;
    bool bOtpRequestSucceeded = false;
    FString Error;
};

class FVerifyPinnedRemainingAuth final : public IAutomationLatentCommand
{
public:
    FVerifyPinnedRemainingAuth(
        TSharedRef<FPinnedRemainingAuthState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted)
        {
            return false;
        }
        if (!State->Error.IsEmpty())
        {
            Test->AddError(State->Error);
        }
        Test->TestTrue(TEXT("Auth-method discovery succeeds against v0.39.11"),
            State->bMethodsSucceeded);
        Test->TestTrue(TEXT("OTP request succeeds against v0.39.11"),
            State->bOtpRequestSucceeded);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FPinnedRemainingAuthState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBasePinnedRemainingAuthTest,
    "OpenPocketBase.Integration.V03911.AuthMethodsAndOtpRequest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBasePinnedRemainingAuthTest::RunTest(const FString& Parameters)
{
    const FString BaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENPOCKETBASE_TEST_URL"));
    if (BaseUrl.IsEmpty())
    {
        AddInfo(TEXT("OPENPOCKETBASE_TEST_URL is not set; the pinned auth-method test was not requested."));
        return true;
    }

    const TSharedRef<FPinnedRemainingAuthState, ESPMode::ThreadSafe> State =
        MakeShared<FPinnedRemainingAuthState, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = BaseUrl;
    Config.ProfileName = TEXT("integration-v03911-remaining-auth");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, Error);
    if (!TestNotNull(TEXT("The remaining-auth integration client is created"), State->Client.Get()))
    {
        AddError(Error.ServerMessage);
        return false;
    }

    const FOpenPocketBaseCollectionService Auth = State->Client->Collection(TEXT("sdk_users"));
    Auth.ListAuthMethods(
        [State, Auth](TOpenPocketBaseResult<FOpenPocketBaseAuthMethods>&& Methods) mutable
        {
            State->bMethodsSucceeded = Methods.IsSuccess() &&
                Methods.GetValue().Password.bEnabled && Methods.GetValue().Otp.bEnabled;
            if (!Methods.IsSuccess())
            {
                State->Error = DescribeIntegrationError(
                    TEXT("List auth methods"), Methods.GetError());
                State->bCompleted = true;
                return;
            }
            Auth.RequestOtp(
                TEXT("player@example.com"),
                [State](TOpenPocketBaseResult<FOpenPocketBaseOtpRequest>&& Otp)
                {
                    State->bOtpRequestSucceeded = Otp.IsSuccess() &&
                        !Otp.GetValue().OtpId.IsEmpty();
                    if (!Otp.IsSuccess())
                    {
                        State->Error = DescribeIntegrationError(
                            TEXT("Request OTP"), Otp.GetError());
                    }
                    State->bCompleted = true;
                });
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyPinnedRemainingAuth(State, this));
    return true;
}

namespace
{
struct FPinnedUploadState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    FString TempFile;
    bool bCompleted = false;
    bool bCreateSucceeded = false;
    bool bAppendSucceeded = false;
    bool bProtectedAccessEnforced = false;
    bool bTokenSucceeded = false;
    bool bDownloadSucceeded = false;
    bool bDeleteSucceeded = false;
    int32 CreatedFiles = 0;
    int32 UpdatedFiles = 0;
    FString UploadedFileName;
    TArray<uint8> ExpectedDownloadBytes;
    TArray<FString> Errors;
};

void FinishPinnedUpload(
    const TSharedRef<FPinnedUploadState, ESPMode::ThreadSafe>& State,
    const bool bDeleteRecord)
{
    if (!bDeleteRecord)
    {
        State->bCompleted = true;
        return;
    }

    State->Client->Collection(TEXT("sdk_tasks")).Delete(
        TEXT("task00000000007"),
        [State](TOpenPocketBaseResult<bool>&& Result)
        {
            State->bDeleteSucceeded = Result.IsSuccess() && Result.GetValue();
            if (!Result.IsSuccess())
            {
                State->Errors.Add(DescribeIntegrationError(TEXT("Delete upload record"), Result.GetError()));
            }
            State->bCompleted = true;
        });
}

int32 GetFileCount(const FOpenPocketBaseRecord& Record)
{
    const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
    return Record.Data.JsonObject.IsValid() &&
        Record.Data.JsonObject->TryGetArrayField(TEXT("attachments"), Files) && Files != nullptr
        ? Files->Num()
        : 0;
}

FString GetFirstFileName(const FOpenPocketBaseRecord& Record)
{
    const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
    FString FileName;
    if (Record.Data.JsonObject.IsValid() &&
        Record.Data.JsonObject->TryGetArrayField(TEXT("attachments"), Files) &&
        Files != nullptr && !Files->IsEmpty() && (*Files)[0].IsValid())
    {
        (*Files)[0]->TryGetString(FileName);
    }
    return FileName;
}

void DownloadPinnedProtectedFile(
    const TSharedRef<FPinnedUploadState, ESPMode::ThreadSafe>& State,
    FOpenPocketBaseFileToken Token)
{
    FOpenPocketBaseFileDownloadOptions Options;
    Options.MaxBytes = 1024;
    State->Client->Files().Download(
        TEXT("sdk_tasks"),
        TEXT("task00000000007"),
        State->UploadedFileName,
        Options,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&& Result)
        {
            State->bDownloadSucceeded = Result.IsSuccess() &&
                Result.GetValue().Bytes == State->ExpectedDownloadBytes;
            if (!Result.IsSuccess())
            {
                State->Errors.Add(DescribeIntegrationError(
                    TEXT("Protected download"),
                    Result.GetError()));
            }
            FinishPinnedUpload(State, true);
        },
        MoveTemp(Token));
}

void RequestPinnedFileToken(
    const TSharedRef<FPinnedUploadState, ESPMode::ThreadSafe>& State)
{
    State->Client->Files().GetToken(
        [State](TOpenPocketBaseResult<FOpenPocketBaseFileToken>&& Result)
        {
            State->bTokenSucceeded = Result.IsSuccess() && Result.GetValue().IsSet();
            if (!Result.IsSuccess())
            {
                State->Errors.Add(DescribeIntegrationError(
                    TEXT("Protected-file token"),
                    Result.GetError()));
                FinishPinnedUpload(State, true);
                return;
            }
            DownloadPinnedProtectedFile(State, MoveTemp(Result.GetValue()));
        });
}

void VerifyPinnedFileRequiresToken(
    const TSharedRef<FPinnedUploadState, ESPMode::ThreadSafe>& State)
{
    FOpenPocketBaseFileDownloadOptions Options;
    Options.MaxBytes = 1024;
    State->Client->Files().Download(
        TEXT("sdk_tasks"),
        TEXT("task00000000007"),
        State->UploadedFileName,
        Options,
        [State](TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&& Result)
        {
            State->bProtectedAccessEnforced = !Result.IsSuccess() &&
                Result.GetError().HttpStatus == 404;
            if (!State->bProtectedAccessEnforced)
            {
                State->Errors.Add(TEXT("Protected file was available without a file token."));
            }
            RequestPinnedFileToken(State);
        });
}

class FVerifyPinnedUpload final : public IAutomationLatentCommand
{
public:
    FVerifyPinnedUpload(
        TSharedRef<FPinnedUploadState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted)
        {
            return false;
        }

        for (const FString& Error : State->Errors)
        {
            Test->AddError(Error);
        }
        Test->TestTrue(TEXT("A disk-path upload succeeds against v0.39.11"), State->bCreateSucceeded);
        Test->TestTrue(TEXT("A byte-array append succeeds against v0.39.11"), State->bAppendSucceeded);
        Test->TestTrue(TEXT("Protected access requires a file token"), State->bProtectedAccessEnforced);
        Test->TestTrue(TEXT("Protected-file token acquisition succeeds"), State->bTokenSucceeded);
        Test->TestTrue(TEXT("A protected streamed download succeeds"), State->bDownloadSucceeded);
        Test->TestEqual(TEXT("Create stores one file"), State->CreatedFiles, 1);
        Test->TestEqual(TEXT("Append retains both files"), State->UpdatedFiles, 2);
        Test->TestTrue(TEXT("The upload fixture record is deleted"), State->bDeleteSucceeded);
        IFileManager::Get().Delete(*State->TempFile, false, true);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FPinnedUploadState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBasePinnedUploadTest,
    "OpenPocketBase.Integration.V03911.MultipartUpload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBasePinnedUploadTest::RunTest(const FString& Parameters)
{
    const FString BaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENPOCKETBASE_TEST_URL"));
    if (BaseUrl.IsEmpty())
    {
        AddInfo(TEXT("OPENPOCKETBASE_TEST_URL is not set; the pinned upload test was not requested."));
        return true;
    }

    const TSharedRef<FPinnedUploadState, ESPMode::ThreadSafe> State =
        MakeShared<FPinnedUploadState, ESPMode::ThreadSafe>();
    State->TempFile = FPaths::CreateTempFilename(
        *FPaths::ProjectIntermediateDir(),
        TEXT("OpenPocketBaseUpload-"),
        TEXT(".txt"));
    const TArray<uint8> DiskBytes = {
        'P', 'o', 'c', 'k', 'e', 't', 'B', 'a', 's', 'e', ' ', 'd', 'i', 's', 'k', ' ',
        'u', 'p', 'l', 'o', 'a', 'd'};
    State->ExpectedDownloadBytes = DiskBytes;
    if (!TestTrue(TEXT("The upload fixture is written"), FFileHelper::SaveArrayToFile(DiskBytes, *State->TempFile)))
    {
        return false;
    }

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = BaseUrl;
    Config.ProfileName = TEXT("integration-v03911-upload");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, Error);
    if (!TestNotNull(TEXT("The upload client is created"), State->Client.Get()))
    {
        IFileManager::Get().Delete(*State->TempFile, false, true);
        AddError(Error.ServerMessage);
        return false;
    }

    State->Client->Collection(TEXT("sdk_users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("correct-horse-battery"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& AuthResult)
        {
            if (!AuthResult.IsSuccess())
            {
                State->Errors.Add(DescribeIntegrationError(TEXT("Upload login"), AuthResult.GetError()));
                FinishPinnedUpload(State, false);
                return;
            }

            FOpenPocketBaseRecordBody Body;
            Body.SetStringField(TEXT("id"), TEXT("task00000000007"));
            Body.SetStringField(TEXT("title"), TEXT("Multipart integration task"));
            FOpenPocketBaseFileInput File;
            File.FieldName = TEXT("attachments");
            File.FileName = TEXT("disk-proof.txt");
            File.ContentType = TEXT("text/plain");
            File.FilePath = State->TempFile;
            State->Client->Collection(TEXT("sdk_tasks")).CreateWithFiles(
                MoveTemp(Body),
                {MoveTemp(File)},
                [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& CreateResult)
                {
                    State->bCreateSucceeded = CreateResult.IsSuccess();
                    if (!CreateResult.IsSuccess())
                    {
                        State->Errors.Add(DescribeIntegrationError(
                            TEXT("Multipart create"),
                            CreateResult.GetError()));
                        FinishPinnedUpload(State, false);
                        return;
                    }
                    State->CreatedFiles = GetFileCount(CreateResult.GetValue());
                    State->UploadedFileName = GetFirstFileName(CreateResult.GetValue());

                    FOpenPocketBaseRecordBody UpdateBody;
                    UpdateBody.SetStringField(TEXT("title"), TEXT("Multipart integration updated"));
                    FOpenPocketBaseFileInput InlineFile;
                    InlineFile.FieldName = TEXT("attachments");
                    InlineFile.FileName = TEXT("memory-proof.txt");
                    InlineFile.ContentType = TEXT("text/plain");
                    InlineFile.Modifier = EOpenPocketBaseFieldModifier::Append;
                    InlineFile.bUseFilePath = false;
                    InlineFile.Bytes = {'m', 'e', 'm', 'o', 'r', 'y'};
                    State->Client->Collection(TEXT("sdk_tasks")).UpdateWithFiles(
                        TEXT("task00000000007"),
                        MoveTemp(UpdateBody),
                        {MoveTemp(InlineFile)},
                        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& UpdateResult)
                        {
                            State->bAppendSucceeded = UpdateResult.IsSuccess();
                            if (!UpdateResult.IsSuccess())
                            {
                                State->Errors.Add(DescribeIntegrationError(
                                    TEXT("Multipart append"),
                                    UpdateResult.GetError()));
                                FinishPinnedUpload(State, true);
                                return;
                            }
                            State->UpdatedFiles = GetFileCount(UpdateResult.GetValue());
                            VerifyPinnedFileRequiresToken(State);
                        });
                });
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyPinnedUpload(State, this));
    return true;
}

namespace
{
struct FPinnedRealtimeState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    FOpenPocketBaseSubscriptionHandle Subscription;
    bool bMutationStarted = false;
    bool bCreateCompleted = false;
    bool bCreateSucceeded = false;
    bool bEventReceived = false;
    bool bDeleteCompleted = false;
    bool bDeleteSucceeded = false;
    TArray<FString> Errors;
};

class FVerifyPinnedRealtime final : public IAutomationLatentCommand
{
public:
    FVerifyPinnedRealtime(
        TSharedRef<FPinnedRealtimeState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCreateCompleted || !State->bDeleteCompleted)
        {
            return false;
        }
        for (const FString& Error : State->Errors)
        {
            Test->AddError(Error);
        }
        Test->TestTrue(TEXT("The authenticated realtime mutation succeeds"),
            State->bCreateSucceeded);
        Test->TestTrue(TEXT("The shared SSE manager receives the create event"),
            State->bEventReceived);
        Test->TestTrue(TEXT("The realtime fixture record is deleted"),
            State->bDeleteSucceeded);
        State->Subscription.Unsubscribe();
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FPinnedRealtimeState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

void DeletePinnedRealtimeRecord(
    const TSharedRef<FPinnedRealtimeState, ESPMode::ThreadSafe>& State)
{
    State->Client->Collection(TEXT("sdk_tasks")).Delete(
        TEXT("task00000000008"),
        [State](TOpenPocketBaseResult<bool>&& Result)
        {
            State->bDeleteSucceeded = Result.IsSuccess() && Result.GetValue();
            if (!Result.IsSuccess())
            {
                State->Errors.Add(DescribeIntegrationError(
                    TEXT("Realtime cleanup"),
                    Result.GetError()));
            }
            State->bDeleteCompleted = true;
        });
}

void CreatePinnedRealtimeRecord(
    const TSharedRef<FPinnedRealtimeState, ESPMode::ThreadSafe>& State)
{
    if (State->bMutationStarted)
    {
        return;
    }
    State->bMutationStarted = true;
    FOpenPocketBaseRecordBody Body;
    Body.SetStringField(TEXT("id"), TEXT("task00000000008"));
    Body.SetStringField(TEXT("title"), TEXT("Realtime integration task"));
    State->Client->Collection(TEXT("sdk_tasks")).Create(
        MoveTemp(Body),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bCreateSucceeded = Result.IsSuccess();
            State->bCreateCompleted = true;
            if (!Result.IsSuccess())
            {
                State->Errors.Add(DescribeIntegrationError(
                    TEXT("Realtime create"),
                    Result.GetError()));
                State->bDeleteCompleted = true;
            }
        });
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBasePinnedRealtimeTest,
    "OpenPocketBase.Integration.V03911.RealtimeSubscription",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBasePinnedRealtimeTest::RunTest(const FString& Parameters)
{
    const FString BaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENPOCKETBASE_TEST_URL"));
    if (BaseUrl.IsEmpty())
    {
        AddInfo(TEXT("OPENPOCKETBASE_TEST_URL is not set; the pinned realtime test was not requested."));
        return true;
    }

    const TSharedRef<FPinnedRealtimeState, ESPMode::ThreadSafe> State =
        MakeShared<FPinnedRealtimeState, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = BaseUrl;
    Config.ProfileName = TEXT("integration-v03911-realtime");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, Error);
    if (!TestNotNull(TEXT("The realtime integration client is created"), State->Client.Get()))
    {
        AddError(Error.ServerMessage);
        return false;
    }

    State->Client->Collection(TEXT("sdk_users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("correct-horse-battery"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& AuthResult)
        {
            if (!AuthResult.IsSuccess())
            {
                State->Errors.Add(DescribeIntegrationError(
                    TEXT("Realtime login"),
                    AuthResult.GetError()));
                State->bCreateCompleted = true;
                State->bDeleteCompleted = true;
                return;
            }

            FOpenPocketBaseRealtimeCallbacks Callbacks;
            Callbacks.OnConnectionStateChanged = [State](
                const EOpenPocketBaseRealtimeConnectionState ConnectionState)
            {
                if (ConnectionState == EOpenPocketBaseRealtimeConnectionState::Active)
                {
                    CreatePinnedRealtimeRecord(State);
                }
            };
            Callbacks.OnEvent = [State](const FOpenPocketBaseRealtimeEvent& Event)
            {
                if (Event.Action == EOpenPocketBaseRealtimeAction::Create &&
                    Event.bHasRecord && Event.Record.Id == TEXT("task00000000008"))
                {
                    State->bEventReceived = true;
                    State->Subscription.Unsubscribe();
                    DeletePinnedRealtimeRecord(State);
                }
            };
            Callbacks.OnError = [State](const FOpenPocketBaseError& RealtimeError)
            {
                State->Errors.Add(DescribeIntegrationError(
                    TEXT("Realtime stream"),
                    RealtimeError));
            };
            FOpenPocketBaseError SubscribeError;
            State->Subscription = State->Client->Collection(TEXT("sdk_tasks"))
                .SubscribeToRecords(MoveTemp(Callbacks), {}, SubscribeError);
            if (SubscribeError.IsSet())
            {
                State->Errors.Add(DescribeIntegrationError(
                    TEXT("Realtime subscribe"),
                    SubscribeError));
                State->bCreateCompleted = true;
                State->bDeleteCompleted = true;
            }
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyPinnedRealtime(State, this));
    return true;
}

namespace
{
struct FPinnedRealtimeChaosState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    FOpenPocketBaseSubscriptionHandle Subscription;
    int32 ActiveCount = 0;
    int32 DisconnectErrorCount = 0;
    int32 ResyncCount = 0;
    bool bMutationStarted = false;
    bool bCreateCompleted = false;
    bool bCreateSucceeded = false;
    bool bEventReceived = false;
    bool bDeleteCompleted = false;
    bool bDeleteSucceeded = false;
    TArray<FString> Errors;
};

class FVerifyPinnedRealtimeChaos final : public IAutomationLatentCommand
{
public:
    FVerifyPinnedRealtimeChaos(
        TSharedRef<FPinnedRealtimeChaosState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCreateCompleted || !State->bDeleteCompleted)
        {
            return false;
        }
        for (const FString& Error : State->Errors)
        {
            Test->AddError(Error);
        }
        Test->TestTrue(TEXT("The proxy forces a second active connection"),
            State->ActiveCount >= 2);
        Test->TestTrue(TEXT("The dropped stream reports a reconnect error"),
            State->DisconnectErrorCount >= 1);
        Test->TestTrue(TEXT("The dropped stream reports a resynchronization gap"),
            State->ResyncCount >= 1);
        Test->TestTrue(TEXT("The post-reconnect mutation succeeds"),
            State->bCreateSucceeded);
        Test->TestTrue(TEXT("The byte-fragmented stream delivers the record event"),
            State->bEventReceived);
        Test->TestTrue(TEXT("The chaos fixture record is deleted"),
            State->bDeleteSucceeded);
        State->Subscription.Unsubscribe();
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FPinnedRealtimeChaosState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

void DeletePinnedRealtimeChaosRecord(
    const TSharedRef<FPinnedRealtimeChaosState, ESPMode::ThreadSafe>& State)
{
    State->Client->Collection(TEXT("sdk_tasks")).Delete(
        TEXT("task00000000009"),
        [State](TOpenPocketBaseResult<bool>&& Result)
        {
            State->bDeleteSucceeded = Result.IsSuccess() && Result.GetValue();
            if (!Result.IsSuccess())
            {
                State->Errors.Add(DescribeIntegrationError(
                    TEXT("Realtime chaos cleanup"),
                    Result.GetError()));
            }
            State->bDeleteCompleted = true;
        });
}

void CreatePinnedRealtimeChaosRecord(
    const TSharedRef<FPinnedRealtimeChaosState, ESPMode::ThreadSafe>& State)
{
    if (State->bMutationStarted)
    {
        return;
    }
    State->bMutationStarted = true;
    FOpenPocketBaseRecordBody Body;
    Body.SetStringField(TEXT("id"), TEXT("task00000000009"));
    Body.SetStringField(TEXT("title"), TEXT("Realtime chaos integration task"));
    State->Client->Collection(TEXT("sdk_tasks")).Create(
        MoveTemp(Body),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bCreateSucceeded = Result.IsSuccess();
            State->bCreateCompleted = true;
            if (!Result.IsSuccess())
            {
                State->Errors.Add(DescribeIntegrationError(
                    TEXT("Realtime chaos create"),
                    Result.GetError()));
                State->bDeleteCompleted = true;
            }
        });
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBasePinnedRealtimeChaosTest,
    "OpenPocketBase.Integration.V03911.RealtimeFragmentationAndReconnect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBasePinnedRealtimeChaosTest::RunTest(const FString& Parameters)
{
    const FString BaseUrl = FPlatformMisc::GetEnvironmentVariable(
        TEXT("OPENPOCKETBASE_REALTIME_CHAOS_URL"));
    if (BaseUrl.IsEmpty())
    {
        AddInfo(TEXT("OPENPOCKETBASE_REALTIME_CHAOS_URL is not set; the realtime chaos test was not requested."));
        return true;
    }

    const TSharedRef<FPinnedRealtimeChaosState, ESPMode::ThreadSafe> State =
        MakeShared<FPinnedRealtimeChaosState, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = BaseUrl;
    Config.ProfileName = TEXT("integration-v03911-realtime-chaos");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, Error);
    if (!TestNotNull(TEXT("The realtime chaos client is created"), State->Client.Get()))
    {
        AddError(Error.ServerMessage);
        return false;
    }

    State->Client->Collection(TEXT("sdk_users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("correct-horse-battery"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& AuthResult)
        {
            if (!AuthResult.IsSuccess())
            {
                State->Errors.Add(DescribeIntegrationError(
                    TEXT("Realtime chaos login"),
                    AuthResult.GetError()));
                State->bCreateCompleted = true;
                State->bDeleteCompleted = true;
                return;
            }

            FOpenPocketBaseRealtimeCallbacks Callbacks;
            Callbacks.OnConnectionStateChanged = [State](
                const EOpenPocketBaseRealtimeConnectionState ConnectionState)
            {
                if (ConnectionState == EOpenPocketBaseRealtimeConnectionState::Active)
                {
                    ++State->ActiveCount;
                    if (State->ActiveCount >= 2)
                    {
                        CreatePinnedRealtimeChaosRecord(State);
                    }
                }
            };
            Callbacks.OnEvent = [State](const FOpenPocketBaseRealtimeEvent& Event)
            {
                if (Event.Action == EOpenPocketBaseRealtimeAction::Create &&
                    Event.bHasRecord && Event.Record.Id == TEXT("task00000000009"))
                {
                    State->bEventReceived = true;
                    State->Subscription.Unsubscribe();
                    DeletePinnedRealtimeChaosRecord(State);
                }
            };
            Callbacks.OnError = [State](const FOpenPocketBaseError& RealtimeError)
            {
                if (RealtimeError.Kind == EOpenPocketBaseErrorKind::Transport ||
                    RealtimeError.Kind == EOpenPocketBaseErrorKind::Timeout)
                {
                    ++State->DisconnectErrorCount;
                }
                else
                {
                    State->Errors.Add(DescribeIntegrationError(
                        TEXT("Realtime chaos stream"),
                        RealtimeError));
                }
            };
            Callbacks.OnResyncRequired = [State]()
            {
                ++State->ResyncCount;
            };
            FOpenPocketBaseError SubscribeError;
            State->Subscription = State->Client->Collection(TEXT("sdk_tasks"))
                .SubscribeToRecords(MoveTemp(Callbacks), {}, SubscribeError);
            if (SubscribeError.IsSet())
            {
                State->Errors.Add(DescribeIntegrationError(
                    TEXT("Realtime chaos subscribe"),
                    SubscribeError));
                State->bCreateCompleted = true;
                State->bDeleteCompleted = true;
            }
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyPinnedRealtimeChaos(State, this));
    return true;
}

#endif
