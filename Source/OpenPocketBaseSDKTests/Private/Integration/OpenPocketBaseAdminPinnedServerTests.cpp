#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "OpenPocketBaseAdminClient.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FOpenPocketBaseCollectionRef PinnedAdminCollectionRef(
    const FString& Name,
    const EOpenPocketBaseCollectionType Type = EOpenPocketBaseCollectionType::Base)
{
    FOpenPocketBaseCollectionRef Ref;
    Ref.SchemaId = FGuid::NewGuid();
    Ref.CollectionId = Name + TEXT("_id");
    Ref.Name = Name;
    Ref.Type = Type;
    return Ref;
}

FOpenPocketBaseAuthCollectionRef PinnedAdminAuthCollectionRef(const FString& Name)
{
    FOpenPocketBaseAuthCollectionRef Ref;
    static_cast<FOpenPocketBaseCollectionRef&>(Ref) =
        PinnedAdminCollectionRef(Name, EOpenPocketBaseCollectionType::Auth);
    return Ref;
}

struct FAdminCredentials
{
    FString Email;
    FString Password;
};

bool TryReadAdminCredentials(const FString& Path, FAdminCredentials& OutCredentials)
{
    FString Json;
    TSharedPtr<FJsonObject> Root;
    if (Path.IsEmpty() || !FFileHelper::LoadFileToString(Json, *Path) ||
        Json.Len() > 2048 ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) ||
        !Root.IsValid() ||
        !Root->TryGetStringField(TEXT("email"), OutCredentials.Email) ||
        !Root->TryGetStringField(TEXT("password"), OutCredentials.Password) ||
        OutCredentials.Email.IsEmpty() || OutCredentials.Password.Len() < 20)
    {
        OutCredentials = {};
        return false;
    }
    return true;
}

FString DescribeAdminIntegrationError(
    const TCHAR* Operation,
    const FOpenPocketBaseError& Error)
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

struct FPinnedAdminState
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Client;
    FOpenPocketBaseAdminDocument TasksCollection;
    TArray<uint8> BackupBytes;
    FString LogId;
    FString CronId;
    TArray<FString> Errors;
    bool bCompleted = false;
    int32 SuccessfulContracts = 0;
    int32 ExpectedFailureContracts = 0;
};

class FPinnedAdminFlow final : public TSharedFromThis<FPinnedAdminFlow, ESPMode::ThreadSafe>
{
public:
    explicit FPinnedAdminFlow(TSharedRef<FPinnedAdminState, ESPMode::ThreadSafe> InState)
        : State(MoveTemp(InState))
    {
    }

    void Start(FString Email, FString Password)
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->AuthenticateSuperuser(
            MoveTemp(Email),
            MoveTemp(Password),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminIdentity>&& Result)
            {
                if (!Self->Require(Result, TEXT("Authenticate Superuser"))) return;
                Self->ListCollections();
            });
    }

private:
    template <typename ValueType>
    bool Require(const TOpenPocketBaseResult<ValueType>& Result, const TCHAR* Operation)
    {
        if (!Result.IsSuccess())
        {
            State->Errors.Add(DescribeAdminIntegrationError(Operation, Result.GetError()));
            State->bCompleted = true;
            return false;
        }
        ++State->SuccessfulContracts;
        return true;
    }

    template <typename ValueType>
    bool RequireExpectedFailure(
        const TOpenPocketBaseResult<ValueType>& Result,
        const TCHAR* Operation)
    {
        if (Result.IsSuccess() || Result.GetError().HttpStatus <= 0)
        {
            State->Errors.Add(FString::Printf(
                TEXT("%s did not return the expected server rejection."),
                Operation));
            State->bCompleted = true;
            return false;
        }
        ++State->ExpectedFailureContracts;
        return true;
    }

    void ListCollections()
    {
        FOpenPocketBaseAdminCollectionListOptions Options;
        Options.PerPage = 100;
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->ListCollections(
            MoveTemp(Options),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminPage>&& Result)
            {
                if (!Self->Require(Result, TEXT("List Collections"))) return;
                bool bFoundTasks = false;
                for (const FJsonObjectWrapper& Item : Result.GetValue().Items)
                {
                    FString Name;
                    if (Item.JsonObject.IsValid() &&
                        Item.JsonObject->TryGetStringField(TEXT("name"), Name) &&
                        Name == TEXT("sdk_tasks"))
                    {
                        bFoundTasks = true;
                        break;
                    }
                }
                if (!bFoundTasks)
                {
                    Self->State->Errors.Add(TEXT("The seeded sdk_tasks collection was not listed."));
                    Self->State->bCompleted = true;
                    return;
                }
                Self->GetCollection();
            });
    }

    void GetCollection()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->GetCollection(
            PinnedAdminCollectionRef(TEXT("sdk_tasks")),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>&& Result)
            {
                if (!Self->Require(Result, TEXT("Get Collection"))) return;
                Self->State->TasksCollection = Result.GetValue();
                Self->ImportCollections();
            });
    }

    void ImportCollections()
    {
        FOpenPocketBaseAdminDocument Body;
        Body.Data.JsonObject = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> Collections;
        Collections.Add(MakeShared<FJsonValueObject>(
            State->TasksCollection.Data.JsonObject.ToSharedRef()));
        Body.Data.JsonObject->SetArrayField(TEXT("collections"), MoveTemp(Collections));
        Body.Data.JsonObject->SetBoolField(TEXT("deleteMissing"), false);
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->ImportCollections(
            MoveTemp(Body),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                if (!Self->Require(Result, TEXT("Import Collections"))) return;
                Self->GetSettings();
            });
    }

    void GetSettings()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->GetSettings(
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>&& Result)
            {
                if (!Self->Require(Result, TEXT("Get Settings"))) return;
                Self->UpdateSettings();
            });
    }

    void UpdateSettings()
    {
        FOpenPocketBaseAdminDocument Body;
        Body.Data.JsonObject = MakeShared<FJsonObject>();
        const TSharedRef<FJsonObject> Meta = MakeShared<FJsonObject>();
        Meta->SetStringField(TEXT("appName"), TEXT("OpenPocketBase integration fixture"));
        Body.Data.JsonObject->SetObjectField(TEXT("meta"), Meta);
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->UpdateSettings(
            MoveTemp(Body),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>&& Result)
            {
                if (!Self->Require(Result, TEXT("Update Settings"))) return;
                Self->TestS3();
            });
    }

    FOpenPocketBaseAdminDocument EmptyDocument() const
    {
        FOpenPocketBaseAdminDocument Body;
        Body.Data.JsonObject = MakeShared<FJsonObject>();
        return Body;
    }

    void TestS3()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->TestS3(
            EmptyDocument(),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                if (!Self->RequireExpectedFailure(Result, TEXT("Test S3"))) return;
                Self->TestEmail();
            });
    }

    void TestEmail()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->TestEmail(
            EmptyDocument(),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                if (!Self->RequireExpectedFailure(Result, TEXT("Test Email"))) return;
                Self->ListLogs();
            });
    }

    void ListLogs()
    {
        FOpenPocketBaseAdminLogListOptions Options;
        Options.PerPage = 20;
        Options.ThenSortBy(
            EOpenPocketBaseAdminLogSortField::Created,
            EOpenPocketBaseSortDirection::Descending);
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->ListLogs(
            MoveTemp(Options),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminPage>&& Result)
            {
                if (!Self->Require(Result, TEXT("List Logs"))) return;
                for (const FJsonObjectWrapper& Item : Result.GetValue().Items)
                {
                    if (Item.JsonObject.IsValid() &&
                        Item.JsonObject->TryGetStringField(TEXT("id"), Self->State->LogId))
                    {
                        break;
                    }
                }
                if (Self->State->LogId.IsEmpty())
                {
                    Self->GetMissingLog();
                    return;
                }
                Self->GetLog();
            });
    }

    void GetMissingLog()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->GetLog(
            TEXT("missinglogentry"),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>&& Result)
            {
                if (!Self->RequireExpectedFailure(Result, TEXT("Get Missing Log"))) return;
                Self->CreateCollection();
            });
    }

    void GetLog()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->GetLog(
            State->LogId,
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>&& Result)
            {
                if (!Self->Require(Result, TEXT("Get Log"))) return;
                Self->CreateCollection();
            });
    }

    void CreateCollection()
    {
        FOpenPocketBaseAdminDocument Body;
        Body.Data.JsonObject = MakeShared<FJsonObject>();
        Body.Data.JsonObject->SetStringField(TEXT("name"), TEXT("sdk_admin_temp"));
        Body.Data.JsonObject->SetStringField(TEXT("type"), TEXT("base"));
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->CreateCollection(
            MoveTemp(Body),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>&& Result)
            {
                if (!Self->Require(Result, TEXT("Create Collection"))) return;
                Self->UpdateCollection();
            });
    }

    void UpdateCollection()
    {
        FOpenPocketBaseAdminDocument Body;
        Body.Data.JsonObject = MakeShared<FJsonObject>();
        Body.Data.JsonObject->SetStringField(TEXT("name"), TEXT("sdk_admin_temp_renamed"));
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->DynamicUpdateCollection(
            TEXT("sdk_admin_temp"),
            MoveTemp(Body),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>&& Result)
            {
                if (!Self->Require(Result, TEXT("Update Collection"))) return;
                Self->DeleteCollection();
            });
    }

    void DeleteCollection()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->DynamicDeleteCollection(
            TEXT("sdk_admin_temp_renamed"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                if (!Self->Require(Result, TEXT("Delete Collection"))) return;
                Self->ListBackups();
            });
    }

    void ListBackups()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->ListBackups(
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminBackupList>&& Result)
            {
                if (!Self->Require(Result, TEXT("List Backups"))) return;
                Self->CreateBackup();
            });
    }

    void CreateBackup()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->CreateBackup(
            TEXT("sdk_admin_fixture.zip"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                if (!Self->Require(Result, TEXT("Create Backup"))) return;
                Self->DownloadBackup();
            });
    }

    void DownloadBackup()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->DownloadBackup(
            TEXT("sdk_admin_fixture.zip"),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminBackupDownload>&& Result)
            {
                if (!Self->Require(Result, TEXT("Download Backup"))) return;
                Self->State->BackupBytes = Result.GetValue().Bytes;
                if (Self->State->BackupBytes.Num() < 4)
                {
                    Self->State->Errors.Add(TEXT("The backup download was unexpectedly empty."));
                    Self->State->bCompleted = true;
                    return;
                }
                Self->DeleteCreatedBackup();
            });
    }

    void DeleteCreatedBackup()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->DeleteBackup(
            TEXT("sdk_admin_fixture.zip"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                if (!Self->Require(Result, TEXT("Delete Backup"))) return;
                Self->UploadBackup();
            });
    }

    void UploadBackup()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->UploadBackup(
            FOpenPocketBaseAdminBackupInput::FromBytes(
                MoveTemp(State->BackupBytes), TEXT("sdk_admin_fixture.zip")),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                if (!Self->Require(Result, TEXT("Upload Backup"))) return;
                Self->DeleteUploadedBackup();
            });
    }

    void DeleteUploadedBackup()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->DeleteBackup(
            TEXT("sdk_admin_fixture.zip"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                if (!Self->Require(Result, TEXT("Delete Uploaded Backup"))) return;
                Self->RejectMissingRestore();
            });
    }

    void RejectMissingRestore()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->RestoreBackup(
            TEXT("missing_restore.zip"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                if (!Self->RequireExpectedFailure(Result, TEXT("Restore Missing Backup"))) return;
                Self->ListCrons();
            });
    }

    void ListCrons()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->ListCrons(
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminDocumentList>&& Result)
            {
                if (!Self->Require(Result, TEXT("List Crons"))) return;
                for (const FJsonObjectWrapper& Item : Result.GetValue().Items)
                {
                    if (Item.JsonObject.IsValid() &&
                        Item.JsonObject->TryGetStringField(TEXT("id"), Self->State->CronId))
                    {
                        break;
                    }
                }
                if (Self->State->CronId.IsEmpty())
                {
                    Self->State->Errors.Add(TEXT("PocketBase returned no cron to run."));
                    Self->State->bCompleted = true;
                    return;
                }
                Self->RunCron();
            });
    }

    void RunCron()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->RunCron(
            State->CronId,
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                if (!Self->Require(Result, TEXT("Run Cron"))) return;
                Self->RunSql();
            });
    }

    void RunSql()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->RunSql(
            TEXT("SELECT name FROM _collections WHERE name = 'sdk_tasks' LIMIT 1"),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminSqlResult>&& Result)
            {
                if (!Self->Require(Result, TEXT("Run SQL"))) return;
                if (Result.GetValue().RowCount != 1)
                {
                    Self->State->Errors.Add(TEXT("The SQL result did not contain sdk_tasks."));
                    Self->State->bCompleted = true;
                    return;
                }
                Self->Impersonate();
            });
    }

    void Impersonate()
    {
        const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->Impersonate(
            PinnedAdminAuthCollectionRef(TEXT("sdk_users")),
            TEXT("user00000000001"),
            60,
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminImpersonationResult>&& Result)
            {
                if (!Self->Require(Result, TEXT("Impersonate"))) return;
                if (!Result.GetValue().Client.IsValid() ||
                    Result.GetValue().Client == Self->State->Client->GetCoreClient() ||
                    !Result.GetValue().Client->IsAuthenticated())
                {
                    Self->State->Errors.Add(TEXT("Impersonation did not create an isolated session."));
                }
                Self->State->bCompleted = true;
            });
    }

    TSharedRef<FPinnedAdminState, ESPMode::ThreadSafe> State;
};

class FVerifyPinnedAdmin final : public IAutomationLatentCommand
{
public:
    FVerifyPinnedAdmin(
        TSharedRef<FPinnedAdminState, ESPMode::ThreadSafe> InState,
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
        Test->TestEqual(TEXT("Every privileged route contract was exercised"),
            State->SuccessfulContracts + State->ExpectedFailureContracts, 24);
        Test->TestTrue(TEXT("Unsafe external actions and restore used server rejections"),
            State->ExpectedFailureContracts >= 3);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FPinnedAdminState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBasePinnedAdminTest,
    "OpenPocketBase.Integration.V03911.PrivilegedAdmin",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBasePinnedAdminTest::RunTest(const FString& Parameters)
{
    const FString BaseUrl = FPlatformMisc::GetEnvironmentVariable(
        TEXT("OPENPOCKETBASE_TEST_URL"));
    const FString CredentialPath = FPlatformMisc::GetEnvironmentVariable(
        TEXT("OPENPOCKETBASE_ADMIN_CREDENTIAL_FILE"));
    if (BaseUrl.IsEmpty() || CredentialPath.IsEmpty())
    {
        AddInfo(TEXT("Pinned privileged environment variables are not set; the test was not requested."));
        return true;
    }
    FAdminCredentials Credentials;
    if (!TryReadAdminCredentials(CredentialPath, Credentials))
    {
        AddError(TEXT("The bounded one-run privileged credential file could not be read."));
        return false;
    }
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = BaseUrl;
    Config.ProfileName = TEXT("integration-admin-v03911");
    FOpenPocketBaseAdminPolicy Policy;
    Policy.bEnablePrivilegedRequests = true;
    Policy.bAllowDestructiveCollectionImport = true;
    Policy.bAllowBackupRestore = true;
    Policy.bAllowImpersonation = true;
    Policy.MaxBackupBytes = 64 * 1024 * 1024;
    const TSharedRef<FPinnedAdminState, ESPMode::ThreadSafe> State =
        MakeShared<FPinnedAdminState, ESPMode::ThreadSafe>();
    FOpenPocketBaseAdminClientResult ClientResult =
        FOpenPocketBaseAdminClient::Create(Config, Policy);
    if (!TestTrue(TEXT("The pinned privileged client is created"), ClientResult.IsSuccess()))
    {
        AddError(ClientResult.GetError().ServerMessage);
        return false;
    }
    State->Client = ClientResult.TakeValue();
    const TSharedRef<FPinnedAdminFlow, ESPMode::ThreadSafe> Flow =
        MakeShared<FPinnedAdminFlow, ESPMode::ThreadSafe>(State);
    Flow->Start(MoveTemp(Credentials.Email), MoveTemp(Credentials.Password));
    Credentials = {};
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyPinnedAdmin(State, this));
    return true;
}

#endif
