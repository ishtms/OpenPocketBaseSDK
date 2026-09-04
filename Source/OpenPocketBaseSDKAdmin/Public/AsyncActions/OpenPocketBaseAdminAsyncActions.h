// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "OpenPocketBaseAdminBlueprintClient.h"
#include "OpenPocketBaseBlueprintClient.h"

#include "OpenPocketBaseAdminAsyncActions.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseAdminIdentityActionSuccess,
    FOpenPocketBaseAdminIdentity,
    Identity,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseAdminDocumentActionSuccess,
    FOpenPocketBaseAdminDocument,
    Document,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseAdminPageActionSuccess,
    FOpenPocketBaseAdminPage,
    Page,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseAdminDocumentListActionSuccess,
    FOpenPocketBaseAdminDocumentList,
    Documents,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseAdminBackupListActionSuccess,
    FOpenPocketBaseAdminBackupList,
    Backups,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseAdminBackupDownloadActionSuccess,
    FOpenPocketBaseAdminBackupDownload,
    Backup,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseAdminSqlActionSuccess,
    FOpenPocketBaseAdminSqlResult,
    Result,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOpenPocketBaseAdminImpersonationActionSuccess,
    UOpenPocketBaseClient*,
    Client,
    FOpenPocketBaseRecord,
    Record,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseAdminActionSuccess,
    FOpenPocketBaseError,
    Error);

UCLASS(Abstract)
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAdminAsyncActionBase
    : public UCancellableAsyncAction
{
    GENERATED_BODY()

public:
    virtual void Cancel() override;

protected:
    virtual void BroadcastCancelled() PURE_VIRTUAL(
        UOpenPocketBaseAdminAsyncActionBase::BroadcastCancelled, );
    bool TryBeginTerminal();
    void Finish();
    static FOpenPocketBaseError MakeCancelledError();
    bool TryGetNativeClient(
        TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe>& OutClient);

    UPROPERTY(Transient)
    TObjectPtr<UOpenPocketBaseAdminClient> AdminClient;

    FOpenPocketBaseAdminRequestHandle RequestHandle;
    bool bTerminal = false;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAuthenticateSuperuserAsyncAction final
    : public UOpenPocketBaseAdminAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminIdentityActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminIdentityActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminIdentityActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Admin",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Authenticate PocketBase Superuser",
            DevelopmentOnly))
    static UOpenPocketBaseAuthenticateSuperuserAsyncAction* AuthenticateSuperuser(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString Email,
        FString Password,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString Email;
    FString Password;
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAdminPageAsyncAction final
    : public UOpenPocketBaseAdminAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminPageActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminPageActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminPageActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Admin",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "List PocketBase Collections",
            DevelopmentOnly))
    static UOpenPocketBaseAdminPageAsyncAction* ListCollections(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseAdminCollectionListOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Admin|Advanced",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "List Dynamic PocketBase Collections (Advanced)",
            DevelopmentOnly))
    static UOpenPocketBaseAdminPageAsyncAction* ListDynamicCollections(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseDynamicAdminListOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Admin",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "List PocketBase Logs",
            DevelopmentOnly))
    static UOpenPocketBaseAdminPageAsyncAction* ListLogs(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseAdminLogListOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Admin|Advanced",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "List Dynamic PocketBase Logs (Advanced)",
            DevelopmentOnly))
    static UOpenPocketBaseAdminPageAsyncAction* ListDynamicLogs(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseDynamicAdminListOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    enum class EOperation : uint8
    {
        Collections,
        DynamicCollections,
        Logs,
        DynamicLogs
    };

    EOperation Operation = EOperation::Collections;
    FOpenPocketBaseAdminCollectionListOptions CollectionOptions;
    FOpenPocketBaseAdminLogListOptions LogOptions;
    FOpenPocketBaseDynamicAdminListOptions DynamicOptions;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAdminDocumentAsyncAction final
    : public UOpenPocketBaseAdminAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminDocumentActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminDocumentActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminDocumentActionSuccess Cancelled;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Get PocketBase Collection",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentAsyncAction* GetCollection(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseCollectionRef Collection,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin|Advanced", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Get Dynamic PocketBase Collection (Advanced)",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentAsyncAction* GetDynamicCollection(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString CollectionNameOrId,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Create PocketBase Collection",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentAsyncAction* CreateCollection(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Update PocketBase Collection",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentAsyncAction* UpdateCollection(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseCollectionRef Collection,
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin|Advanced", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Update Dynamic PocketBase Collection (Advanced)",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentAsyncAction* UpdateDynamicCollection(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString CollectionNameOrId,
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Get PocketBase Settings",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentAsyncAction* GetSettings(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Update PocketBase Settings",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentAsyncAction* UpdateSettings(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Get PocketBase Log",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentAsyncAction* GetLog(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString LogId,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    enum class EOperation : uint8
    {
        GetCollection,
        GetDynamicCollection,
        CreateCollection,
        UpdateCollection,
        UpdateDynamicCollection,
        GetSettings,
        UpdateSettings,
        GetLog
    };

    EOperation Operation = EOperation::GetCollection;
    FOpenPocketBaseCollectionRef Collection;
    FString Target;
    FOpenPocketBaseAdminDocument Body;
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAdminCommandAsyncAction final
    : public UOpenPocketBaseAdminAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminActionSuccess Cancelled;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Delete PocketBase Collection",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* DeleteCollection(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseCollectionRef Collection,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin|Advanced", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Delete Dynamic PocketBase Collection (Advanced)",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* DeleteDynamicCollection(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString CollectionNameOrId,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Import PocketBase Collections",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* ImportCollections(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Test PocketBase S3 Settings",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* TestS3(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Test PocketBase Email Settings",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* TestEmail(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Create PocketBase Backup", AdvancedDisplay = "Name",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* CreateBackup(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString Name,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Upload PocketBase Backup From Path",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* UploadBackupFromPath(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString FilePath,
        FString FileName,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Upload PocketBase Backup From Bytes",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* UploadBackupFromBytes(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        TArray<uint8> Bytes,
        FString FileName,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Restore PocketBase Backup",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* RestoreBackup(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString Key,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Delete PocketBase Backup",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* DeleteBackup(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString Key,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Run PocketBase Cron",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* RunCron(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString CronId,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    enum class EOperation : uint8
    {
        DeleteCollection,
        DeleteDynamicCollection,
        ImportCollections,
        TestS3,
        TestEmail,
        CreateBackup,
        UploadBackup,
        RestoreBackup,
        DeleteBackup,
        RunCron
    };

    EOperation Operation = EOperation::DeleteCollection;
    FOpenPocketBaseCollectionRef Collection;
    FString Target;
    FOpenPocketBaseAdminDocument Body;
    FOpenPocketBaseAdminBackupInput Backup;
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAdminBackupListAsyncAction final
    : public UOpenPocketBaseAdminAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminBackupListActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminBackupListActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminBackupListActionSuccess Cancelled;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "List PocketBase Backups",
        DevelopmentOnly))
    static UOpenPocketBaseAdminBackupListAsyncAction* ListBackups(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAdminBackupDownloadAsyncAction final
    : public UOpenPocketBaseAdminAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminBackupDownloadActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminBackupDownloadActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminBackupDownloadActionSuccess Cancelled;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Download PocketBase Backup",
        DevelopmentOnly))
    static UOpenPocketBaseAdminBackupDownloadAsyncAction* DownloadBackup(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString Key,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString Key;
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAdminDocumentListAsyncAction final
    : public UOpenPocketBaseAdminAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminDocumentListActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminDocumentListActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminDocumentListActionSuccess Cancelled;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "List PocketBase Crons",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentListAsyncAction* ListCrons(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAdminSqlAsyncAction final
    : public UOpenPocketBaseAdminAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminSqlActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminSqlActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminSqlActionSuccess Cancelled;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Run PocketBase SQL",
        DevelopmentOnly))
    static UOpenPocketBaseAdminSqlAsyncAction* RunSql(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString Query,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString Query;
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAdminImpersonateAsyncAction final
    : public UOpenPocketBaseAdminAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminImpersonationActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminImpersonationActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminImpersonationActionSuccess Cancelled;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Impersonate PocketBase User",
        DevelopmentOnly))
    static UOpenPocketBaseAdminImpersonateAsyncAction* Impersonate(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseAuthCollectionRef AuthCollection,
        FString RecordId,
        int64 DurationSeconds,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin|Advanced", meta = (
        BlueprintInternalUseOnly = "true",
        DisplayName = "Impersonate Dynamic PocketBase User (Advanced)",
        DevelopmentOnly))
    static UOpenPocketBaseAdminImpersonateAsyncAction* ImpersonateDynamicUser(
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString AuthCollectionNameOrId,
        FString RecordId,
        int64 DurationSeconds,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseAuthCollectionRef AuthCollection;
    FString DynamicAuthCollection;
    bool bDynamicCollection = false;
    FString RecordId;
    int64 DurationSeconds = 0;
    FOpenPocketBaseRequestOptions Options;
};
