#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "OpenPocketBaseBlueprintClient.h"

#include "OpenPocketBaseRecordAsyncActions.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseRecordActionSuccess,
    FOpenPocketBaseRecord,
    Record,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOpenPocketBaseRecordTransferActionOutput,
    FOpenPocketBaseRecord,
    Record,
    FOpenPocketBaseTransferProgress,
    TransferProgress,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseRecordPageActionSuccess,
    FOpenPocketBaseRecordPage,
    Page,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseFullListActionSuccess,
    FOpenPocketBaseFullListResult,
    Result,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseAuthActionSuccess,
    FOpenPocketBaseAuthResult,
    AuthResult,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOpenPocketBaseAuthMfaActionOutput,
    FOpenPocketBaseAuthResult,
    AuthResult,
    FOpenPocketBaseMfaContinuation,
    MfaContinuation,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseAuthMethodsActionSuccess,
    FOpenPocketBaseAuthMethods,
    AuthMethods,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseOtpRequestActionSuccess,
    FOpenPocketBaseOtpRequest,
    OtpRequest,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseOAuth2AuthorizationActionSuccess,
    FOpenPocketBaseOAuth2Authorization,
    Authorization,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseExternalAuthsActionSuccess,
    FOpenPocketBaseExternalAuthList,
    ExternalAuths,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseHealthActionSuccess,
    FOpenPocketBaseHealthResult,
    HealthResult,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseCustomRouteActionSuccess,
    FOpenPocketBaseCustomRouteResponse,
    Response,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseSessionRestoreActionSuccess,
    FOpenPocketBaseSessionRestoreResult,
    RestoreResult,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseActionSuccess,
    FOpenPocketBaseError,
    Error);

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
    static FOpenPocketBaseError MakeCancelledError();

    UPROPERTY(Transient)
    TObjectPtr<UOpenPocketBaseClient> Client;

    FOpenPocketBaseRequestHandle RequestHandle;
    bool bTerminal = false;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseHealthAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseHealthActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseHealthActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseHealthActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Utilities",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Check Health"))
    static UOpenPocketBaseHealthAsyncAction* CheckHealth(
        UOpenPocketBaseClient* PocketBaseClient,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseRequestOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseCustomRouteAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseCustomRouteActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseCustomRouteActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseCustomRouteActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Utilities|Advanced",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Send Dynamic Custom Route (Advanced)"))
    static UOpenPocketBaseCustomRouteAsyncAction* SendCustomRoute(
        UOpenPocketBaseClient* PocketBaseClient,
        FOpenPocketBaseCustomRouteRequest Request);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseCustomRouteRequest Request;
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
    FOpenPocketBaseRecordActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Records",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Get Record"))
    static UOpenPocketBaseGetRecordAsyncAction* GetRecord(
        FOpenPocketBaseCollection Collection,
        FString RecordId,
        FOpenPocketBaseRecordOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseCollectionRef Collection;
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
    FOpenPocketBaseRecordActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Records",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Get First Record"))
    static UOpenPocketBaseGetFirstRecordAsyncAction* GetFirstRecord(
        FOpenPocketBaseCollection Collection,
        FOpenPocketBaseFilter Filter,
        FOpenPocketBaseRecordOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseCollectionRef Collection;
    FOpenPocketBaseFilter Filter;
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
    FOpenPocketBaseRecordActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Records",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Create Record"))
    static UOpenPocketBaseCreateRecordAsyncAction* CreateRecord(
        FOpenPocketBaseWritableCollection Collection,
        FOpenPocketBaseRecordBody Body,
        FOpenPocketBaseRecordOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseWritableCollectionRef Collection;
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
    FOpenPocketBaseRecordTransferActionOutput Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordTransferActionOutput Progress;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordTransferActionOutput Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordTransferActionOutput Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Files",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Create Record with Files",
            AutoCreateRefTerm = "Files",
            AdvancedDisplay = "Limits"))
    static UOpenPocketBaseCreateRecordWithFilesAsyncAction* CreateRecordWithFiles(
        FOpenPocketBaseWritableCollection Collection,
        FOpenPocketBaseRecordBody Body,
        TArray<FOpenPocketBaseFileInput> Files,
        FOpenPocketBaseRecordOptions Options,
        FOpenPocketBaseUploadLimits Limits);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseWritableCollectionRef Collection;
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
    FOpenPocketBaseRecordActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Records",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Update Record"))
    static UOpenPocketBaseUpdateRecordAsyncAction* UpdateRecord(
        FOpenPocketBaseWritableCollection Collection,
        FString RecordId,
        FOpenPocketBaseRecordBody Body,
        FOpenPocketBaseRecordOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseWritableCollectionRef Collection;
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
    FOpenPocketBaseRecordTransferActionOutput Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordTransferActionOutput Progress;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordTransferActionOutput Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordTransferActionOutput Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Files",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Update Record with Files",
            AutoCreateRefTerm = "Files",
            AdvancedDisplay = "Limits"))
    static UOpenPocketBaseUpdateRecordWithFilesAsyncAction* UpdateRecordWithFiles(
        FOpenPocketBaseWritableCollection Collection,
        FString RecordId,
        FOpenPocketBaseRecordBody Body,
        TArray<FOpenPocketBaseFileInput> Files,
        FOpenPocketBaseRecordOptions Options,
        FOpenPocketBaseUploadLimits Limits);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseWritableCollectionRef Collection;
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
    FOpenPocketBaseActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Records",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Delete Record"))
    static UOpenPocketBaseDeleteRecordAsyncAction* DeleteRecord(
        FOpenPocketBaseWritableCollection Collection,
        FString RecordId,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseWritableCollectionRef Collection;
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
    FOpenPocketBaseRecordPageActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseRecordPageActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Records",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "List Records"))
    static UOpenPocketBaseListRecordsAsyncAction* ListRecords(
        FOpenPocketBaseCollection Collection,
        FOpenPocketBaseListOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseCollectionRef Collection;
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
    FOpenPocketBaseFullListActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseFullListActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Records",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Get Full Record List"))
    static UOpenPocketBaseGetFullListAsyncAction* GetFullList(
        FOpenPocketBaseCollection Collection,
        FOpenPocketBaseFullListOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseCollectionRef Collection;
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
    FOpenPocketBaseAuthActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Refresh Session"))
    static UOpenPocketBaseRefreshAuthAsyncAction* RefreshAuth(
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
    FOpenPocketBaseAuthMethodsActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMethodsActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "List Authentication Methods"))
    static UOpenPocketBaseListAuthMethodsAsyncAction* ListAuthenticationMethods(
        FOpenPocketBaseAuthCollection AuthCollection,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseAuthCollectionRef AuthCollection;
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
    FOpenPocketBaseOtpRequestActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseOtpRequestActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Request One-Time Password"))
    static UOpenPocketBaseRequestOtpAsyncAction* RequestOneTimePassword(
        FOpenPocketBaseAuthCollection AuthCollection,
        FString Email,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseAuthCollectionRef AuthCollection;
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
    FOpenPocketBaseAuthMfaActionOutput Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMfaActionOutput MfaRequired;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMfaActionOutput Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMfaActionOutput Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Log In with One-Time Password",
            AdvancedDisplay = "Mfa"))
    static UOpenPocketBaseOtpAuthAsyncAction* LogInWithOneTimePassword(
        FOpenPocketBaseAuthCollection AuthCollection,
        FString OtpId,
        FString OneTimePassword,
        FOpenPocketBaseMfaContinuation Mfa,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseAuthCollectionRef AuthCollection;
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
    FOpenPocketBaseOAuth2AuthorizationActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseOAuth2AuthorizationActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Begin Manual OAuth2"))
    static UOpenPocketBaseBeginOAuth2AsyncAction* BeginManualOAuth2(
        FOpenPocketBaseAuthCollection AuthCollection,
        FOpenPocketBaseOAuth2StartOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseAuthCollectionRef AuthCollection;
    FOpenPocketBaseOAuth2StartOptions Options;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseCompleteOAuth2AsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMfaActionOutput Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMfaActionOutput MfaRequired;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMfaActionOutput Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMfaActionOutput Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Complete Manual OAuth2"))
    static UOpenPocketBaseCompleteOAuth2AsyncAction* CompleteManualOAuth2(
        FOpenPocketBaseAuthCollection AuthCollection,
        FOpenPocketBaseOAuth2Callback Callback);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseAuthCollectionRef AuthCollection;
    FOpenPocketBaseOAuth2Callback Callback;
};

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseAssistedOAuth2AsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMfaActionOutput Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMfaActionOutput MfaRequired;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMfaActionOutput Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMfaActionOutput Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Log In with OAuth2"))
    static UOpenPocketBaseAssistedOAuth2AsyncAction* LogInWithOAuth2(
        FOpenPocketBaseAuthCollection AuthCollection,
        FOpenPocketBaseAssistedOAuth2Options Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseAuthCollectionRef AuthCollection;
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
    FOpenPocketBaseActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Request Password Reset"))
    static UOpenPocketBaseAccountAsyncAction* RequestPasswordReset(
        FOpenPocketBaseAuthCollection AuthCollection,
        FString Email,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Confirm Password Reset"))
    static UOpenPocketBaseAccountAsyncAction* ConfirmPasswordReset(
        FOpenPocketBaseAuthCollection AuthCollection,
        FString Token,
        FString NewPassword,
        FString ConfirmPassword,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Request Email Verification"))
    static UOpenPocketBaseAccountAsyncAction* RequestVerification(
        FOpenPocketBaseAuthCollection AuthCollection,
        FString Email,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Confirm Email Verification"))
    static UOpenPocketBaseAccountAsyncAction* ConfirmVerification(
        FOpenPocketBaseAuthCollection AuthCollection,
        FString Token,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Request Email Change"))
    static UOpenPocketBaseAccountAsyncAction* RequestEmailChange(
        FOpenPocketBaseAuthCollection AuthCollection,
        FString NewEmail,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Confirm Email Change"))
    static UOpenPocketBaseAccountAsyncAction* ConfirmEmailChange(
        FOpenPocketBaseAuthCollection AuthCollection,
        FString Token,
        FString CurrentPassword,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Unlink External Auth"))
    static UOpenPocketBaseAccountAsyncAction* UnlinkExternalAuth(
        FOpenPocketBaseAuthCollection AuthCollection,
        FString RecordId,
        FString Provider,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    static UOpenPocketBaseAccountAsyncAction* CreateAction(
        EOpenPocketBaseAccountActionKind Kind,
        FOpenPocketBaseAuthCollection AuthCollection,
        FString Primary,
        FString Secondary,
        FString Tertiary,
        FOpenPocketBaseRequestOptions Options);

    EOpenPocketBaseAccountActionKind Kind =
        EOpenPocketBaseAccountActionKind::RequestPasswordReset;
    FOpenPocketBaseAuthCollectionRef AuthCollection;
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
    FOpenPocketBaseExternalAuthsActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseExternalAuthsActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "List Linked External Auths"))
    static UOpenPocketBaseListExternalAuthsAsyncAction* ListLinkedExternalAuths(
        FOpenPocketBaseAuthCollection AuthCollection,
        FString RecordId,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseAuthCollectionRef AuthCollection;
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
    FOpenPocketBaseSessionRestoreActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseSessionRestoreActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Session",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Restore Session"))
    static UOpenPocketBaseRestoreSessionAsyncAction* RestoreSession(
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
    FOpenPocketBaseAuthMfaActionOutput Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMfaActionOutput MfaRequired;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMfaActionOutput Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseAuthMfaActionOutput Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Log In with Password"))
    static UOpenPocketBasePasswordAuthAsyncAction* LogInWithPassword(
        FOpenPocketBaseAuthCollection AuthCollection,
        FString Identity,
        FString Password,
        FOpenPocketBaseRequestOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseAuthCollectionRef AuthCollection;
    FString Identity;
    FString Password;
    FOpenPocketBaseRequestOptions Options;
};
