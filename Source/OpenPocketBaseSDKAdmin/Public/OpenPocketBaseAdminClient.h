#pragma once

#include "OpenPocketBaseAdminTypes.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseResult.h"

class FOpenPocketBaseAdminRequestState;

class OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminRequestHandle
{
public:
    FOpenPocketBaseAdminRequestHandle() = default;
    explicit FOpenPocketBaseAdminRequestHandle(
        TSharedPtr<FOpenPocketBaseAdminRequestState, ESPMode::ThreadSafe> InState);

    void Cancel() const;
    bool IsActive() const;

private:
    TSharedPtr<FOpenPocketBaseAdminRequestState, ESPMode::ThreadSafe> State;
};

struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminImpersonationResult
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    FOpenPocketBaseRecord Record;
};

class FOpenPocketBaseAdminClient;
using FOpenPocketBaseAdminClientRef =
    TSharedRef<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe>;
using FOpenPocketBaseAdminClientResult =
    TOpenPocketBaseResult<FOpenPocketBaseAdminClientRef>;

using FOpenPocketBaseAdminIdentityCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseAdminIdentity>&&)>;
using FOpenPocketBaseAdminDocumentCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>&&)>;
using FOpenPocketBaseAdminPageCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseAdminPage>&&)>;
using FOpenPocketBaseAdminDocumentListCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseAdminDocumentList>&&)>;
using FOpenPocketBaseAdminBackupListCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseAdminBackupList>&&)>;
using FOpenPocketBaseAdminBackupDownloadCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseAdminBackupDownload>&&)>;
using FOpenPocketBaseAdminSqlCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseAdminSqlResult>&&)>;
using FOpenPocketBaseAdminImpersonationCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseAdminImpersonationResult>&&)>;

class OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminClient final
    : public TSharedFromThis<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe>
{
public:
    static FOpenPocketBaseAdminClientResult Create(
        const FOpenPocketBaseClientConfig& CoreConfig,
        const FOpenPocketBaseAdminPolicy& Policy,
        FOpenPocketBaseClientDependencies Dependencies = {});

    FOpenPocketBaseAdminRequestHandle AuthenticateSuperuser(
        FString Email,
        FString Password,
        FOpenPocketBaseAdminIdentityCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});

    bool IsAuthenticated() const;
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> GetCoreClient() const;
    void Logout();
    void Shutdown();

    FOpenPocketBaseAdminRequestHandle ListCollections(
        FOpenPocketBaseAdminListOptions Options,
        FOpenPocketBaseAdminPageCallback OnComplete);
    FOpenPocketBaseAdminRequestHandle GetCollection(
        FOpenPocketBaseCollectionRef Collection,
        FOpenPocketBaseAdminDocumentCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle CreateCollection(
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseAdminDocumentCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle UpdateCollection(
        FOpenPocketBaseCollectionRef Collection,
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseAdminDocumentCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle DeleteCollection(
        FOpenPocketBaseCollectionRef Collection,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle DynamicGetCollection(
        FString Collection,
        FOpenPocketBaseAdminDocumentCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle DynamicUpdateCollection(
        FString Collection,
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseAdminDocumentCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle DynamicDeleteCollection(
        FString Collection,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle ImportCollections(
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});

    FOpenPocketBaseAdminRequestHandle GetSettings(
        FOpenPocketBaseAdminDocumentCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle UpdateSettings(
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseAdminDocumentCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle TestS3(
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle TestEmail(
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});

    FOpenPocketBaseAdminRequestHandle ListLogs(
        FOpenPocketBaseAdminListOptions Options,
        FOpenPocketBaseAdminPageCallback OnComplete);
    FOpenPocketBaseAdminRequestHandle GetLog(
        FString LogId,
        FOpenPocketBaseAdminDocumentCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});

    FOpenPocketBaseAdminRequestHandle ListBackups(
        FOpenPocketBaseAdminBackupListCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle CreateBackup(
        FString Name,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle UploadBackup(
        FOpenPocketBaseAdminBackupInput Backup,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle DownloadBackup(
        FString Key,
        FOpenPocketBaseAdminBackupDownloadCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle RestoreBackup(
        FString Key,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle DeleteBackup(
        FString Key,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});

    FOpenPocketBaseAdminRequestHandle ListCrons(
        FOpenPocketBaseAdminDocumentListCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle RunCron(
        FString CronId,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle RunSql(
        FString Query,
        FOpenPocketBaseAdminSqlCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle Impersonate(
        FOpenPocketBaseAuthCollectionRef AuthCollection,
        FString RecordId,
        int64 DurationSeconds,
        FOpenPocketBaseAdminImpersonationCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseAdminRequestHandle DynamicImpersonate(
        FString AuthCollection,
        FString RecordId,
        int64 DurationSeconds,
        FOpenPocketBaseAdminImpersonationCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});

private:
    FOpenPocketBaseAdminClient(
        FOpenPocketBaseClientConfig InCoreConfig,
        FOpenPocketBaseAdminPolicy InPolicy,
        TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InCoreClient,
        TSharedPtr<IOpenPocketBaseTransport, ESPMode::ThreadSafe> InInjectedTransport);

    FOpenPocketBaseClientConfig CoreConfig;
    FOpenPocketBaseAdminPolicy Policy;
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> CoreClient;
    TSharedPtr<IOpenPocketBaseTransport, ESPMode::ThreadSafe> InjectedTransport;
};
