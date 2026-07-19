#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "OpenPocketBaseAdminClient.h"
#include "Transport/OpenPocketBaseTransport.h"

namespace
{
FOpenPocketBaseCollectionRef AdminCollectionRef(
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

FOpenPocketBaseAuthCollectionRef AdminAuthCollectionRef(const FString& Name)
{
    FOpenPocketBaseAuthCollectionRef Ref;
    static_cast<FOpenPocketBaseCollectionRef&>(Ref) =
        AdminCollectionRef(Name, EOpenPocketBaseCollectionType::Auth);
    return Ref;
}

TArray<uint8> AdminUtf8(const FString& Value)
{
    const FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

class FAdminTransport final : public IOpenPocketBaseTransport
{
public:
    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        if (Request.BodyStream.IsValid())
        {
            Request.Body.SetNumUninitialized(Request.BodyLength);
            Request.BodyStream->Serialize(Request.Body.GetData(), Request.BodyLength);
            Request.BodyStream.Reset();
        }
        const int32 Index = Requests.Add(MoveTemp(Request));
        FOpenPocketBaseHttpResponse Response = MoveTemp(Responses[0]);
        Responses.RemoveAt(0, EAllowShrinking::No);
        Response.RequestId = Requests[Index].RequestId;
        Response.EffectiveUrl = Requests[Index].Url;
        OnComplete(MoveTemp(Response));
        return {};
    }

    void AddJson(const FString& Json, const int32 Status = 200)
    {
        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = Status;
        Response.Headers.Add(TEXT("Content-Type"), TEXT("application/json"));
        Response.Body = AdminUtf8(Json);
        Responses.Add(MoveTemp(Response));
    }

    void AddEmpty(const int32 Status = 204)
    {
        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = Status;
        Responses.Add(MoveTemp(Response));
    }

    void AddBinary(TArray<uint8> Body)
    {
        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = 200;
        Response.Headers.Add(TEXT("Content-Type"), TEXT("application/zip"));
        Response.Body = MoveTemp(Body);
        Responses.Add(MoveTemp(Response));
    }

    TArray<FOpenPocketBaseHttpRequest> Requests;
    TArray<FOpenPocketBaseHttpResponse> Responses;
};

struct FAdminTestState
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FAdminTransport, ESPMode::ThreadSafe> Transport;
    bool bCompleted = false;
    bool bSucceeded = true;
    bool bSettingsRedacted = false;
    bool bBackupExact = false;
    bool bSqlDataClean = false;
    bool bSqlErrorSanitized = false;
    int32 RejectedSqlPolicyCases = 0;
    bool bImpersonationSeparated = false;
    bool bImpersonationRecordClean = false;
    bool bImpersonationRefreshBlocked = false;
    TArray<FString> Errors;
};

class FAdminFlow final : public TSharedFromThis<FAdminFlow, ESPMode::ThreadSafe>
{
public:
    explicit FAdminFlow(TSharedRef<FAdminTestState, ESPMode::ThreadSafe> InState)
        : State(MoveTemp(InState))
    {
    }

    void Start()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->AuthenticateSuperuser(
            TEXT("fixture@example.test"),
            TEXT("ephemeral-not-a-credential"),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminIdentity>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Authenticate"));
                if (Result.IsSuccess())
                {
                    Self->ListCollections();
                }
            });
    }

private:
    void Continue(const bool bSuccess, const TCHAR* Operation)
    {
        State->bSucceeded = State->bSucceeded && bSuccess;
        if (!bSuccess)
        {
            State->Errors.Add(Operation);
            State->bCompleted = true;
        }
    }

    FOpenPocketBaseAdminDocument Document(const TCHAR* Name) const
    {
        FOpenPocketBaseAdminDocument Value;
        Value.Data.JsonObject = MakeShared<FJsonObject>();
        Value.Data.JsonObject->SetStringField(TEXT("name"), Name);
        return Value;
    }

    void ListCollections()
    {
        FOpenPocketBaseAdminListOptions Options;
        Options.Page = 2;
        Options.PerPage = 20;
        Options.Sort = {TEXT("name")};
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->ListCollections(
            MoveTemp(Options),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminPage>&& Result)
            {
                Self->Continue(Result.IsSuccess() && Result.GetValue().Items.Num() == 1,
                    TEXT("List Collections"));
                if (Result.IsSuccess()) Self->GetCollection();
            });
    }

    void GetCollection()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->GetCollection(
            AdminCollectionRef(TEXT("sdk_tasks")),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Get Collection"));
                if (Result.IsSuccess()) Self->CreateCollection();
            });
    }

    void CreateCollection()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->CreateCollection(
            Document(TEXT("sdk_admin_temp")),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Create Collection"));
                if (Result.IsSuccess()) Self->UpdateCollection();
            });
    }

    void UpdateCollection()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->DynamicUpdateCollection(
            TEXT("sdk_admin_temp"),
            Document(TEXT("sdk_admin_temp_2")),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Update Collection"));
                if (Result.IsSuccess()) Self->DeleteCollection();
            });
    }

    void DeleteCollection()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->DynamicDeleteCollection(
            TEXT("sdk_admin_temp_2"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Delete Collection"));
                if (Result.IsSuccess()) Self->ImportCollections();
            });
    }

    void ImportCollections()
    {
        FOpenPocketBaseAdminDocument Body;
        Body.Data.JsonObject = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> Collections;
        Collections.Add(MakeShared<FJsonValueObject>(
            Document(TEXT("sdk_imported")).Data.JsonObject));
        Body.Data.JsonObject->SetArrayField(TEXT("collections"), MoveTemp(Collections));
        Body.Data.JsonObject->SetBoolField(TEXT("deleteMissing"), false);
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->ImportCollections(
            MoveTemp(Body),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Import Collections"));
                if (Result.IsSuccess()) Self->GetSettings();
            });
    }

    void GetSettings()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->GetSettings(
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Get Settings"));
                if (Result.IsSuccess())
                {
                    const TSharedPtr<FJsonObject>* Smtp = nullptr;
                    FString Password;
                    Self->State->bSettingsRedacted =
                        Result.GetValue().Data.JsonObject->TryGetObjectField(TEXT("smtp"), Smtp) &&
                        Smtp != nullptr && (*Smtp)->TryGetStringField(TEXT("password"), Password) &&
                        Password == TEXT("[REDACTED]");
                    Self->UpdateSettings();
                }
            });
    }

    void UpdateSettings()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->UpdateSettings(
            Document(TEXT("settings")),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Update Settings"));
                if (Result.IsSuccess()) Self->TestS3();
            });
    }

    void TestS3()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->TestS3(
            Document(TEXT("s3")),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Test S3"));
                if (Result.IsSuccess()) Self->TestEmail();
            });
    }

    void TestEmail()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->TestEmail(
            Document(TEXT("email")),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Test Email"));
                if (Result.IsSuccess()) Self->ListLogs();
            });
    }

    void ListLogs()
    {
        FOpenPocketBaseAdminListOptions Options;
        Options.PerPage = 10;
        Options.Sort = {TEXT("-created")};
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->ListLogs(
            MoveTemp(Options),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminPage>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("List Logs"));
                if (Result.IsSuccess()) Self->GetLog();
            });
    }

    void GetLog()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->GetLog(
            TEXT("log000000000001"),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Get Log"));
                if (Result.IsSuccess()) Self->ListBackups();
            });
    }

    void ListBackups()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->ListBackups(
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminBackupList>&& Result)
            {
                Self->Continue(Result.IsSuccess() && Result.GetValue().Items.Num() == 1,
                    TEXT("List Backups"));
                if (Result.IsSuccess()) Self->CreateBackup();
            });
    }

    void CreateBackup()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->CreateBackup(
            TEXT("sdk_test.zip"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Create Backup"));
                if (Result.IsSuccess()) Self->UploadBackup();
            });
    }

    void UploadBackup()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->UploadBackup(
            FOpenPocketBaseAdminBackupInput::FromBytes(
                {'P', 'K', 3, 4}, TEXT("sdk_upload.zip")),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Upload Backup"));
                if (Result.IsSuccess()) Self->DownloadBackup();
            });
    }

    void DownloadBackup()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->DownloadBackup(
            TEXT("sdk_test.zip"),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminBackupDownload>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Download Backup"));
                if (Result.IsSuccess())
                {
                    Self->State->bBackupExact =
                        Result.GetValue().Bytes == TArray<uint8>({'P', 'K', 3, 4});
                    Self->RestoreBackup();
                }
            });
    }

    void RestoreBackup()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->RestoreBackup(
            TEXT("sdk_test.zip"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Restore Backup"));
                if (Result.IsSuccess()) Self->DeleteBackup();
            });
    }

    void DeleteBackup()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->DeleteBackup(
            TEXT("sdk_test.zip"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Delete Backup"));
                if (Result.IsSuccess()) Self->ListCrons();
            });
    }

    void ListCrons()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->ListCrons(
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminDocumentList>&& Result)
            {
                Self->Continue(Result.IsSuccess() && Result.GetValue().Items.Num() == 1,
                    TEXT("List Crons"));
                if (Result.IsSuccess()) Self->RunCron();
            });
    }

    void RunCron()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->RunCron(
            TEXT("fixture_job"),
            [Self](TOpenPocketBaseResult<bool>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Run Cron"));
                if (Result.IsSuccess()) Self->RunSql();
            });
    }

    void RunSql()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->RunSql(
            TEXT("SELECT id FROM sdk_tasks LIMIT 1"),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminSqlResult>&& Result)
            {
                if (Result.IsSuccess())
                {
                    const FJsonObjectWrapper& Data = Result.GetValue().Data;
                    Self->State->bSqlDataClean = Data.JsonObject.IsValid() &&
                        Data.JsonObject->HasField(TEXT("rows")) &&
                        !Data.JsonObject->HasField(TEXT("execTime")) &&
                        !Data.JsonObject->HasField(TEXT("affectedRows")) &&
                        !Data.JsonObject->HasField(TEXT("columns"));
                }
                Self->Continue(Result.IsSuccess() && Result.GetValue().RowCount == 1,
                    TEXT("Run SQL"));
                if (Result.IsSuccess()) Self->RejectSqlPolicyCase(0);
            });
    }

    void RejectSqlPolicyCase(const int32 Index)
    {
        const TArray<FString> Queries = {
            TEXT("WITH target AS (SELECT id FROM sdk_tasks) DELETE FROM sdk_tasks"),
            TEXT("PRAGMA user_version = 7"),
            TEXT("SELECT 1; DELETE FROM sdk_tasks")};
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->RunSql(
            Queries[Index],
            [Self, Index](TOpenPocketBaseResult<FOpenPocketBaseAdminSqlResult>&& Result)
            {
                const bool bRejected = !Result.IsSuccess() &&
                    Result.GetError().Kind == EOpenPocketBaseErrorKind::InvalidArgument;
                Self->Continue(bRejected, TEXT("Reject SQL Write Policy Bypass"));
                if (!bRejected)
                {
                    return;
                }
                ++Self->State->RejectedSqlPolicyCases;
                if (Index + 1 < 3) Self->RejectSqlPolicyCase(Index + 1);
                else Self->RunSensitiveSqlFailure();
            });
    }

    void RunSensitiveSqlFailure()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->RunSql(
            TEXT("SELECT secret_password FROM private_table"),
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminSqlResult>&& Result)
            {
                Self->State->bSqlErrorSanitized = !Result.IsSuccess() &&
                    !Result.GetError().ServerMessage.Contains(TEXT("secret_password"));
                Self->Continue(Self->State->bSqlErrorSanitized,
                    TEXT("Sanitize SQL Failure"));
                if (!Result.IsSuccess()) Self->Impersonate();
            });
    }

    void Impersonate()
    {
        const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->Impersonate(
            AdminAuthCollectionRef(TEXT("sdk_users")),
            TEXT("user00000000001"),
            3600,
            [Self](TOpenPocketBaseResult<FOpenPocketBaseAdminImpersonationResult>&& Result)
            {
                Self->Continue(Result.IsSuccess(), TEXT("Impersonate"));
                if (Result.IsSuccess())
                {
                    const FOpenPocketBaseRecord& Record = Result.GetValue().Record;
                    FString DisplayName;
                    Self->State->bImpersonationRecordClean =
                        Record.Data.JsonObject.IsValid() &&
                        Record.Data.JsonObject->TryGetStringField(
                            TEXT("displayName"), DisplayName) &&
                        DisplayName == TEXT("Player") &&
                        !Record.Data.JsonObject->HasField(TEXT("id")) &&
                        !Record.Data.JsonObject->HasField(TEXT("collectionId")) &&
                        !Record.Data.JsonObject->HasField(TEXT("collectionName"));
                    FOpenPocketBaseSessionSnapshot Session;
                    Self->State->bImpersonationSeparated =
                        Result.GetValue().Client.IsValid() &&
                        Result.GetValue().Client != Self->State->Client->GetCoreClient() &&
                        Result.GetValue().Client->IsAuthenticated() &&
                        Result.GetValue().Client->GetCurrentSession(Session) &&
                        Session.PersistenceState ==
                            EOpenPocketBaseSessionPersistenceState::MemoryOnly;
                    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe>
                        ImpersonatedClient = Result.GetValue().Client;
                    ImpersonatedClient->RefreshAuth(
                        [Self, ImpersonatedClient](
                            TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& RefreshResult)
                        {
                            Self->State->bImpersonationRefreshBlocked =
                                !RefreshResult.IsSuccess() &&
                                RefreshResult.GetError().Kind ==
                                    EOpenPocketBaseErrorKind::Unsupported;
                            ImpersonatedClient->Shutdown();
                            Self->State->bCompleted = true;
                        });
                }
            });
    }

    TSharedRef<FAdminTestState, ESPMode::ThreadSafe> State;
};

class FVerifyAdminClient final : public IAutomationLatentCommand
{
public:
    FVerifyAdminClient(
        TSharedRef<FAdminTestState, ESPMode::ThreadSafe> InState,
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
        Test->TestTrue(TEXT("Every privileged operation succeeds"), State->bSucceeded);
        Test->TestTrue(TEXT("Settings secrets are redacted"), State->bSettingsRedacted);
        Test->TestTrue(TEXT("Backup bytes remain exact"), State->bBackupExact);
        Test->TestTrue(TEXT("SQL data excludes typed result metadata"), State->bSqlDataClean);
        Test->TestTrue(TEXT("SQL failures do not expose query or result material"),
            State->bSqlErrorSanitized);
        Test->TestEqual(TEXT("Default SQL policy rejects non-SELECT and stacked statements"),
            State->RejectedSqlPolicyCases, 3);
        Test->TestTrue(TEXT("Impersonation uses a separate authenticated client"),
            State->bImpersonationSeparated);
        Test->TestTrue(TEXT("Impersonated record data excludes typed system fields"),
            State->bImpersonationRecordClean);
        Test->TestTrue(TEXT("Impersonation cannot persist or refresh its token"),
            State->bImpersonationRefreshBlocked);
        Test->TestEqual(TEXT("All pinned privileged requests are sent"),
            State->Transport->Requests.Num(), 25);
        if (State->Transport->Requests.Num() == 25)
        {
            const TArray<FString> ExpectedMethods = {
                TEXT("POST"), TEXT("GET"), TEXT("GET"), TEXT("POST"), TEXT("PATCH"),
                TEXT("DELETE"), TEXT("PUT"), TEXT("GET"), TEXT("PATCH"), TEXT("POST"),
                TEXT("POST"), TEXT("GET"), TEXT("GET"), TEXT("GET"), TEXT("POST"),
                TEXT("POST"), TEXT("POST"), TEXT("GET"), TEXT("POST"), TEXT("DELETE"),
                TEXT("GET"), TEXT("POST"), TEXT("POST"), TEXT("POST"), TEXT("POST")};
            for (int32 Index = 0; Index < ExpectedMethods.Num(); ++Index)
            {
                Test->TestEqual(TEXT("The privileged method matches the pinned contract"),
                    State->Transport->Requests[Index].Method, ExpectedMethods[Index]);
            }
            Test->TestTrue(TEXT("Collection list uses bounded pagination"),
                State->Transport->Requests[1].Url.Contains(TEXT("page=2")) &&
                State->Transport->Requests[1].Url.Contains(TEXT("perPage=20")));
            Test->TestTrue(TEXT("Backup upload is generic multipart"),
                State->Transport->Requests[15].Headers.FindRef(TEXT("Content-Type"))
                    .StartsWith(TEXT("multipart/form-data; boundary=")));
            Test->TestTrue(TEXT("Backup token is limited to the required query parameter"),
                State->Transport->Requests[17].Url.EndsWith(
                    TEXT("?token=mock-file-token")) &&
                !State->Transport->Requests[17].Headers.Contains(TEXT("Authorization")));
        }
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FAdminTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAdminClientTest,
    "OpenPocketBase.Admin.CoversPinnedPrivilegedRoutes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAdminClientTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FAdminTestState, ESPMode::ThreadSafe> State =
        MakeShared<FAdminTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FAdminTransport, ESPMode::ThreadSafe>();
    State->Transport->AddJson(
        TEXT("{\"token\":\"mock-superuser-token\",\"record\":{\"id\":\"super0000000001\","
             "\"collectionId\":\"pbc_3142635823\",\"collectionName\":\"_superusers\","
             "\"email\":\"fixture@example.test\"}}"));
    State->Transport->AddJson(TEXT("{\"page\":2,\"perPage\":20,\"totalItems\":1,"
                                       "\"totalPages\":1,\"items\":[{\"id\":\"c1\"}]}"));
    State->Transport->AddJson(TEXT("{\"id\":\"c1\",\"name\":\"sdk_tasks\"}"));
    State->Transport->AddJson(TEXT("{\"id\":\"c2\",\"name\":\"sdk_admin_temp\"}"));
    State->Transport->AddJson(TEXT("{\"id\":\"c2\",\"name\":\"sdk_admin_temp_2\"}"));
    State->Transport->AddEmpty();
    State->Transport->AddEmpty();
    State->Transport->AddJson(TEXT("{\"smtp\":{\"password\":\"must-redact\"}}"));
    State->Transport->AddJson(TEXT("{\"smtp\":{\"password\":\"must-redact\"}}"));
    State->Transport->AddEmpty();
    State->Transport->AddEmpty();
    State->Transport->AddJson(TEXT("{\"page\":1,\"perPage\":10,\"totalItems\":1,"
                                       "\"totalPages\":1,\"items\":[{\"id\":\"l1\"}]}"));
    State->Transport->AddJson(TEXT("{\"id\":\"l1\",\"message\":\"ok\"}"));
    State->Transport->AddJson(
        TEXT("[{\"key\":\"sdk_test.zip\",\"size\":4,"
             "\"modified\":\"2026-08-23 00:00:00.000Z\"}]"));
    State->Transport->AddEmpty();
    State->Transport->AddEmpty();
    State->Transport->AddJson(TEXT("{\"token\":\"mock-file-token\"}"));
    State->Transport->AddBinary({'P', 'K', 3, 4});
    State->Transport->AddEmpty();
    State->Transport->AddEmpty();
    State->Transport->AddJson(
        TEXT("[{\"id\":\"fixture_job\",\"expression\":\"0 * * * *\"}]"));
    State->Transport->AddEmpty();
    State->Transport->AddJson(
        TEXT("{\"execTime\":1,\"affectedRows\":0,\"columns\":[{\"name\":\"id\","
             "\"type\":\"TEXT\",\"nullable\":false}],\"rows\":[[\"task00000000001\"]]}"));
    State->Transport->AddJson(
        TEXT("{\"status\":400,\"message\":\"query leaked SELECT secret_password\","
             "\"data\":{}}"),
        400);
    State->Transport->AddJson(
        TEXT("{\"token\":\"mock-impersonation-token\",\"record\":{"
             "\"id\":\"user00000000001\",\"collectionId\":\"users_collection\","
             "\"collectionName\":\"sdk_users\",\"displayName\":\"Player\"}}"));

    FOpenPocketBaseClientConfig CoreConfig;
    CoreConfig.BaseUrl = TEXT("https://pb.example.test");
    FOpenPocketBaseAdminPolicy Policy;
    Policy.bEnablePrivilegedRequests = true;
    Policy.bAllowBackupRestore = true;
    Policy.bAllowImpersonation = true;
    Policy.MaxBackupBytes = 1024 * 1024;
    FOpenPocketBaseClientDependencies Dependencies;
    Dependencies.Transport = State->Transport;
    FOpenPocketBaseAdminClientResult ClientResult =
        FOpenPocketBaseAdminClient::Create(CoreConfig, Policy, MoveTemp(Dependencies));
    if (!TestTrue(TEXT("The privileged client is created"), ClientResult.IsSuccess()))
    {
        AddError(ClientResult.GetError().ServerMessage);
        return false;
    }
    State->Client = ClientResult.TakeValue();

    const TSharedRef<FAdminFlow, ESPMode::ThreadSafe> Flow =
        MakeShared<FAdminFlow, ESPMode::ThreadSafe>(State);
    Flow->Start();
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyAdminClient(State, this));
    return true;
}

#endif
