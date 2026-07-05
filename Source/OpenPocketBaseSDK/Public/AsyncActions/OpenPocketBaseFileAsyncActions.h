#pragma once

#include "AsyncActions/OpenPocketBaseRecordAsyncActions.h"
#include "OpenPocketBaseFile.h"

#include "OpenPocketBaseFileAsyncActions.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseFileTokenActionSuccess,
    FOpenPocketBaseFileToken,
    Token);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseFileDownloadActionSuccess,
    FOpenPocketBaseFileDownloadResult,
    Result);

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseGetFileTokenAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseFileTokenActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Files",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Get Protected File Token",
            AdvancedDisplay = "Options"))
    static UOpenPocketBaseGetFileTokenAsyncAction* GetProtectedFileToken(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseDownloadFileAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseFileDownloadActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseTransferProgressAction Progress;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Files",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Download File",
            AdvancedDisplay = "Token"))
    static UOpenPocketBaseDownloadFileAsyncAction* DownloadFile(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString Collection,
        FString RecordId,
        FString FileName,
        FOpenPocketBaseFileDownloadOptions Options,
        FOpenPocketBaseFileToken Token);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString Collection;
    FString RecordId;
    FString FileName;
    FOpenPocketBaseFileDownloadOptions Options;
    FOpenPocketBaseFileToken Token;
};
