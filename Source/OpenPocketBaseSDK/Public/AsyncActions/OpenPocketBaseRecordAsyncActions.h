#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "OpenPocketBaseBlueprintClient.h"

#include "OpenPocketBaseRecordAsyncActions.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseRecordActionSuccess,
    FOpenPocketBaseRecord,
    Record);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseRecordPageActionSuccess,
    FOpenPocketBaseRecordPage,
    Page);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseFullListActionSuccess,
    FOpenPocketBaseFullListResult,
    Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseAuthActionSuccess,
    FOpenPocketBaseAuthResult,
    AuthResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseAuthMethodsActionSuccess,
    FOpenPocketBaseAuthMethods,
    AuthMethods);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseOtpRequestActionSuccess,
    FOpenPocketBaseOtpRequest,
    OtpRequest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseOAuth2AuthorizationActionSuccess,
    FOpenPocketBaseOAuth2Authorization,
    Authorization);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseExternalAuthsActionSuccess,
    FOpenPocketBaseExternalAuthList,
    ExternalAuths);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseMfaRequiredAction,
    FOpenPocketBaseMfaContinuation,
    Continuation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseSessionRestoreActionSuccess,
    FOpenPocketBaseSessionRestoreResult,
    RestoreResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOpenPocketBaseActionSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseActionFailed,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOpenPocketBaseActionCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseTransferProgressAction,
    FOpenPocketBaseTransferProgress,
    TransferProgress);

UCLASS(Abstract)
class OPENPOCKETBASESDK_API UOpenPocketBaseAsyncActionBase : public UCancellableAsyncAction
{
    GENERATED_BODY()

public:
    virtual void Cancel() override;

protected:
    virtual void BroadcastCancelled() PURE_VIRTUAL(UOpenPocketBaseAsyncActionBase::BroadcastCancelled, );
    bool TryBeginTerminal();
    void Finish();

    UPROPERTY(Transient)
    TObjectPtr<UOpenPocketBaseClient> Client;

    FOpenPocketBaseRequestHandle RequestHandle;
    bool bTerminal = false;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseGetRecordAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Records",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Get Record"))
    static UOpenPocketBaseGetRecordAsyncAction* GetRecord(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString Collection,
        FString RecordId,
        FOpenPocketBaseRecordOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString Collection;
    FString RecordId;
    FOpenPocketBaseRecordOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseGetFirstRecordAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Records",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Get First Record"))
    static UOpenPocketBaseGetFirstRecordAsyncAction* GetFirstRecord(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString Collection,
        FString Filter,
        FOpenPocketBaseRecordOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString Collection;
    FString Filter;
    FOpenPocketBaseRecordOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseCreateRecordAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Records",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Create Record"))
    static UOpenPocketBaseCreateRecordAsyncAction* CreateRecord(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString Collection,
        FOpenPocketBaseRecordBody Body,
        FOpenPocketBaseRecordOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString Collection;
    FOpenPocketBaseRecordBody Body;
    FOpenPocketBaseRecordOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseCreateRecordWithFilesAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordActionSuccess Success;

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
            DisplayName = "Create Record with Files",
            AutoCreateRefTerm = "Files",
            AdvancedDisplay = "Options,Limits"))
    static UOpenPocketBaseCreateRecordWithFilesAsyncAction* CreateRecordWithFiles(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString Collection,
        FOpenPocketBaseRecordBody Body,
        TArray<FOpenPocketBaseFileInput> Files,
        FOpenPocketBaseRecordOptions Options,
        FOpenPocketBaseUploadLimits Limits);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString Collection;
    FOpenPocketBaseRecordBody Body;
    TArray<FOpenPocketBaseFileInput> Files;
    FOpenPocketBaseRecordOptions Options;
    FOpenPocketBaseUploadLimits Limits;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseUpdateRecordAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Records",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Update Record"))
    static UOpenPocketBaseUpdateRecordAsyncAction* UpdateRecord(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString Collection,
        FString RecordId,
        FOpenPocketBaseRecordBody Body,
        FOpenPocketBaseRecordOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString Collection;
    FString RecordId;
    FOpenPocketBaseRecordBody Body;
    FOpenPocketBaseRecordOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseUpdateRecordWithFilesAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordActionSuccess Success;

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
            DisplayName = "Update Record with Files",
            AutoCreateRefTerm = "Files",
            AdvancedDisplay = "Options,Limits"))
    static UOpenPocketBaseUpdateRecordWithFilesAsyncAction* UpdateRecordWithFiles(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString Collection,
        FString RecordId,
        FOpenPocketBaseRecordBody Body,
        TArray<FOpenPocketBaseFileInput> Files,
        FOpenPocketBaseRecordOptions Options,
        FOpenPocketBaseUploadLimits Limits);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString Collection;
    FString RecordId;
    FOpenPocketBaseRecordBody Body;
    TArray<FOpenPocketBaseFileInput> Files;
    FOpenPocketBaseRecordOptions Options;
    FOpenPocketBaseUploadLimits Limits;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseDeleteRecordAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Records",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Delete Record"))
    static UOpenPocketBaseDeleteRecordAsyncAction* DeleteRecord(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString Collection,
        FString RecordId,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString Collection;
    FString RecordId;
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseListRecordsAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordPageActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Records",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "List Records"))
    static UOpenPocketBaseListRecordsAsyncAction* ListRecords(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString Collection,
        FOpenPocketBaseListOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString Collection;
    FOpenPocketBaseListOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseGetFullListAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseFullListActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Records",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Get Full Record List"))
    static UOpenPocketBaseGetFullListAsyncAction* GetFullList(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString Collection,
        FOpenPocketBaseFullListOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString Collection;
    FOpenPocketBaseFullListOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseRefreshAuthAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Refresh Auth"))
    static UOpenPocketBaseRefreshAuthAsyncAction* RefreshAuth(
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
class OPENPOCKETBASESDK_API UOpenPocketBaseListAuthMethodsAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMethodsActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "List Authentication Methods"))
    static UOpenPocketBaseListAuthMethodsAsyncAction* ListAuthenticationMethods(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString AuthCollection,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString AuthCollection;
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseRequestOtpAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseOtpRequestActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Request One-Time Password"))
    static UOpenPocketBaseRequestOtpAsyncAction* RequestOneTimePassword(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString AuthCollection,
        FString Email,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString AuthCollection;
    FString Email;
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseOtpAuthAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseMfaRequiredAction MfaRequired;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Log In with One-Time Password",
            AdvancedDisplay = "Mfa,Options"))
    static UOpenPocketBaseOtpAuthAsyncAction* LogInWithOneTimePassword(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString AuthCollection,
        FString OtpId,
        FString Password,
        FOpenPocketBaseMfaContinuation Mfa,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString AuthCollection;
    FString OtpId;
    FString Password;
    FOpenPocketBaseMfaContinuation Mfa;
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseBeginOAuth2AsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseOAuth2AuthorizationActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Begin Manual OAuth2"))
    static UOpenPocketBaseBeginOAuth2AsyncAction* BeginManualOAuth2(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString AuthCollection,
        FOpenPocketBaseOAuth2StartOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString AuthCollection;
    FOpenPocketBaseOAuth2StartOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseCompleteOAuth2AsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseMfaRequiredAction MfaRequired;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Complete Manual OAuth2"))
    static UOpenPocketBaseCompleteOAuth2AsyncAction* CompleteManualOAuth2(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString AuthCollection,
        FOpenPocketBaseOAuth2Callback Callback);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString AuthCollection;
    FOpenPocketBaseOAuth2Callback Callback;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseAssistedOAuth2AsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseMfaRequiredAction MfaRequired;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Log In with OAuth2"))
    static UOpenPocketBaseAssistedOAuth2AsyncAction* LogInWithOAuth2(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString AuthCollection,
        FOpenPocketBaseAssistedOAuth2Options Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString AuthCollection;
    FOpenPocketBaseAssistedOAuth2Options Options;
};

enum class EOpenPocketBaseAccountActionKind : uint8
{
    RequestPasswordReset,
    ConfirmPasswordReset,
    RequestVerification,
    ConfirmVerification,
    RequestEmailChange,
    ConfirmEmailChange,
    UnlinkExternalAuth
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseAccountAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Request Password Reset"))
    static UOpenPocketBaseAccountAsyncAction* RequestPasswordReset(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString AuthCollection,
        FString Email,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Confirm Password Reset"))
    static UOpenPocketBaseAccountAsyncAction* ConfirmPasswordReset(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString AuthCollection,
        FString Token,
        FString Password,
        FString PasswordConfirm,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Request Email Verification"))
    static UOpenPocketBaseAccountAsyncAction* RequestVerification(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString AuthCollection,
        FString Email,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Confirm Email Verification"))
    static UOpenPocketBaseAccountAsyncAction* ConfirmVerification(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString AuthCollection,
        FString Token,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Request Email Change"))
    static UOpenPocketBaseAccountAsyncAction* RequestEmailChange(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString AuthCollection,
        FString NewEmail,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Confirm Email Change"))
    static UOpenPocketBaseAccountAsyncAction* ConfirmEmailChange(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString AuthCollection,
        FString Token,
        FString Password,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Unlink External Auth"))
    static UOpenPocketBaseAccountAsyncAction* UnlinkExternalAuth(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString AuthCollection,
        FString RecordId,
        FString Provider,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    static UOpenPocketBaseAccountAsyncAction* CreateAction(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        EOpenPocketBaseAccountActionKind Kind,
        FString AuthCollection,
        FString Primary,
        FString Secondary,
        FString Tertiary,
        FOpenPocketBaseRequestOptions Options);

    EOpenPocketBaseAccountActionKind Kind =
        EOpenPocketBaseAccountActionKind::RequestPasswordReset;
    FString AuthCollection;
    FString Primary;
    FString Secondary;
    FString Tertiary;
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseListExternalAuthsAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseExternalAuthsActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "List Linked External Auths"))
    static UOpenPocketBaseListExternalAuthsAsyncAction* ListLinkedExternalAuths(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString AuthCollection,
        FString RecordId,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString AuthCollection;
    FString RecordId;
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseRestoreSessionAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseSessionRestoreActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Session",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Restore Session"))
    static UOpenPocketBaseRestoreSessionAsyncAction* RestoreSession(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        bool bVerifyWithServer,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    bool bVerifyWithServer = true;
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBasePasswordAuthAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseMfaRequiredAction MfaRequired;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionFailed Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionCancelled Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Log In with Password"))
    static UOpenPocketBasePasswordAuthAsyncAction* LogInWithPassword(
        const UObject* WorldContextObject,
        UOpenPocketBaseClient* PocketBaseClient,
        FString AuthCollection,
        FString Identity,
        FString Password,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FString AuthCollection;
    FString Identity;
    FString Password;
    FOpenPocketBaseRequestOptions Options;
};
