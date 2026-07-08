#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "OpenPocketBaseAdminBlueprintClient.h"
#include "OpenPocketBaseBlueprintClient.h"

#include "OpenPocketBaseAdminAsyncActions.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseAdminIdentityActionSuccess,
    FOpenPocketBaseAdminIdentity,
    Identity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseAdminDocumentActionSuccess,
    FOpenPocketBaseAdminDocument,
    Document);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseAdminPageActionSuccess,
    FOpenPocketBaseAdminPage,
    Page);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseAdminDocumentListActionSuccess,
    FOpenPocketBaseAdminDocumentList,
    Documents);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseAdminBackupListActionSuccess,
    FOpenPocketBaseAdminBackupList,
    Backups);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseAdminBackupDownloadActionSuccess,
    FOpenPocketBaseAdminBackupDownload,
    Backup);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseAdminSqlActionSuccess,
    FOpenPocketBaseAdminSqlResult,
    Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseAdminImpersonationActionSuccess,
    UOpenPocketBaseClient*,
    Client,
    FOpenPocketBaseRecord,
    Record);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOpenPocketBaseAdminActionSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseAdminActionFailed,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOpenPocketBaseAdminActionCancelled);

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
    FOpenPocketBaseAdminActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Admin",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Authenticate PocketBase Superuser",
            AdvancedDisplay = "Options",
            DevelopmentOnly))
    static UOpenPocketBaseAuthenticateSuperuserAsyncAction* AuthenticateSuperuser(
        const UObject* WorldContextObject,
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
    FOpenPocketBaseAdminActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Admin",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "List PocketBase Collections",
            DevelopmentOnly))
    static UOpenPocketBaseAdminPageAsyncAction* ListCollections(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseAdminListOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Admin",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "List PocketBase Logs",
            DevelopmentOnly))
    static UOpenPocketBaseAdminPageAsyncAction* ListLogs(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseAdminListOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    enum class EOperation : uint8
    {
        Collections,
        Logs
    };

    EOperation Operation = EOperation::Collections;
    FOpenPocketBaseAdminListOptions Options;
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
    FOpenPocketBaseAdminActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminActionCancelled Cancelled;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Get PocketBase Collection", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentAsyncAction* GetCollection(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString Collection,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Create PocketBase Collection", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentAsyncAction* CreateCollection(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Update PocketBase Collection", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentAsyncAction* UpdateCollection(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString Collection,
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Get PocketBase Settings", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentAsyncAction* GetSettings(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Update PocketBase Settings", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentAsyncAction* UpdateSettings(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Get PocketBase Log", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentAsyncAction* GetLog(
        const UObject* WorldContextObject,
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
        CreateCollection,
        UpdateCollection,
        GetSettings,
        UpdateSettings,
        GetLog
    };

    EOperation Operation = EOperation::GetCollection;
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
    FOpenPocketBaseAdminActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminActionCancelled Cancelled;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Delete PocketBase Collection", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* DeleteCollection(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString Collection,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Import PocketBase Collections", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* ImportCollections(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Test PocketBase S3 Settings", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* TestS3(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Test PocketBase Email Settings", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* TestEmail(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseAdminDocument Body,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Create PocketBase Backup", AdvancedDisplay = "Name,Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* CreateBackup(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString Name,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Upload PocketBase Backup", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* UploadBackup(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FOpenPocketBaseFileInput File,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Restore PocketBase Backup", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* RestoreBackup(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString Key,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Delete PocketBase Backup", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* DeleteBackup(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString Key,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Run PocketBase Cron", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminCommandAsyncAction* RunCron(
        const UObject* WorldContextObject,
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
    FString Target;
    FOpenPocketBaseAdminDocument Body;
    FOpenPocketBaseFileInput File;
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
    FOpenPocketBaseAdminActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminActionCancelled Cancelled;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "List PocketBase Backups", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminBackupListAsyncAction* ListBackups(
        const UObject* WorldContextObject,
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
    FOpenPocketBaseAdminActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminActionCancelled Cancelled;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Download PocketBase Backup", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminBackupDownloadAsyncAction* DownloadBackup(
        const UObject* WorldContextObject,
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
    FOpenPocketBaseAdminActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminActionCancelled Cancelled;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "List PocketBase Crons", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminDocumentListAsyncAction* ListCrons(
        const UObject* WorldContextObject,
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
    FOpenPocketBaseAdminActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminActionCancelled Cancelled;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Run PocketBase SQL", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminSqlAsyncAction* RunSql(
        const UObject* WorldContextObject,
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
    FOpenPocketBaseAdminActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAdminActionCancelled Cancelled;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (
        BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
        DisplayName = "Impersonate PocketBase User", AdvancedDisplay = "Options",
        DevelopmentOnly))
    static UOpenPocketBaseAdminImpersonateAsyncAction* Impersonate(
        const UObject* WorldContextObject,
        UOpenPocketBaseAdminClient* PocketBaseAdminClient,
        FString AuthCollection,
        FString RecordId,
        int64 DurationSeconds,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString AuthCollection;
    FString RecordId;
    int64 DurationSeconds = 0;
    FOpenPocketBaseRequestOptions Options;
};
