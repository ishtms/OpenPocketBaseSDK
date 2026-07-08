#pragma once

#include "OpenPocketBaseBatch.h"
#include "OpenPocketBaseAuthentication.h"
#include "OpenPocketBaseCapability.h"
#include "OpenPocketBaseClientConfig.h"
#include "OpenPocketBaseFile.h"
#include "OpenPocketBaseRecord.h"
#include "OpenPocketBaseRealtime.h"
#include "OpenPocketBaseRequestHandle.h"
#include "OpenPocketBaseResult.h"
#include "OpenPocketBaseSession.h"
#include "Clock/OpenPocketBaseClock.h"
#include "OAuth/OpenPocketBaseOAuthBrowser.h"
#include "Templates/SharedPointer.h"
#include "Templates/Function.h"
#include "SecureStorage/OpenPocketBaseSecureStore.h"
#include "Transport/OpenPocketBaseTransport.h"

class FOpenPocketBaseClient;
class FOpenPocketBaseFileService;

namespace OpenPocketBase::Internal
{
class FAssistedOAuthOperation;
}

using FOpenPocketBaseRecordCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseRecord>&&)>;
using FOpenPocketBaseRecordPageCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseRecordPage>&&)>;
using FOpenPocketBaseFullListCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseFullListResult>&&)>;
using FOpenPocketBaseAuthCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&&)>;
using FOpenPocketBaseAuthMethodsCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseAuthMethods>&&)>;
using FOpenPocketBaseOtpRequestCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseOtpRequest>&&)>;
using FOpenPocketBaseAuthAttemptCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&&)>;
using FOpenPocketBaseOAuth2AuthorizationCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseOAuth2Authorization>&&)>;
using FOpenPocketBaseExternalAuthsCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<TArray<FOpenPocketBaseExternalAuth>>&&)>;
using FOpenPocketBaseBoolCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<bool>&&)>;
using FOpenPocketBaseBatchCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseBatchResult>&&)>;
using FOpenPocketBaseSessionRestoreCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseSessionRestoreResult>&&)>;
using FOpenPocketBaseFileTokenCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseFileToken>&&)>;
using FOpenPocketBaseFileDownloadCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&&)>;

class OPENPOCKETBASESDK_API FOpenPocketBaseFileService
{
public:
    bool TryBuildUrl(
        FString Collection,
        FString RecordId,
        FString FileName,
        FOpenPocketBaseFileUrlOptions Options,
        FString& OutUrl,
        FOpenPocketBaseError& OutError) const;

    FOpenPocketBaseRequestHandle GetToken(
        FOpenPocketBaseFileTokenCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle Download(
        FString Collection,
        FString RecordId,
        FString FileName,
        FOpenPocketBaseFileDownloadOptions Options,
        FOpenPocketBaseFileDownloadCallback OnComplete,
        FOpenPocketBaseFileToken Token = {},
        FOpenPocketBaseTransferProgressCallback OnProgress = {}) const;

    bool IsValid() const;

private:
    explicit FOpenPocketBaseFileService(
        TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient);

    TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;

    friend class FOpenPocketBaseClient;
};

class OPENPOCKETBASESDK_API FOpenPocketBaseCollectionService
{
public:
    FOpenPocketBaseRequestHandle GetOne(
        FString RecordId,
        FOpenPocketBaseRecordCallback OnComplete,
        FOpenPocketBaseRecordOptions Options = {}) const;

    FOpenPocketBaseRequestHandle GetList(
        FOpenPocketBaseListOptions Options,
        FOpenPocketBaseRecordPageCallback OnComplete) const;

    FOpenPocketBaseRequestHandle GetFullList(
        FOpenPocketBaseFullListOptions Options,
        FOpenPocketBaseFullListCallback OnComplete) const;

    FOpenPocketBaseRequestHandle GetFirstListItem(
        FString Filter,
        FOpenPocketBaseRecordCallback OnComplete,
        FOpenPocketBaseRecordOptions Options = {}) const;

    FOpenPocketBaseRequestHandle Create(
        FOpenPocketBaseRecordBody Body,
        FOpenPocketBaseRecordCallback OnComplete,
        FOpenPocketBaseRecordOptions Options = {}) const;

    FOpenPocketBaseRequestHandle CreateWithFiles(
        FOpenPocketBaseRecordBody Body,
        TArray<FOpenPocketBaseFileInput> Files,
        FOpenPocketBaseRecordCallback OnComplete,
        FOpenPocketBaseRecordOptions Options = {},
        FOpenPocketBaseUploadLimits Limits = {},
        FOpenPocketBaseTransferProgressCallback OnProgress = {}) const;

    FOpenPocketBaseRequestHandle Update(
        FString RecordId,
        FOpenPocketBaseRecordBody Body,
        FOpenPocketBaseRecordCallback OnComplete,
        FOpenPocketBaseRecordOptions Options = {}) const;

    FOpenPocketBaseRequestHandle UpdateWithFiles(
        FString RecordId,
        FOpenPocketBaseRecordBody Body,
        TArray<FOpenPocketBaseFileInput> Files,
        FOpenPocketBaseRecordCallback OnComplete,
        FOpenPocketBaseRecordOptions Options = {},
        FOpenPocketBaseUploadLimits Limits = {},
        FOpenPocketBaseTransferProgressCallback OnProgress = {}) const;

    FOpenPocketBaseRequestHandle Delete(
        FString RecordId,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle AuthWithPassword(
        FString Identity,
        FString Password,
        FOpenPocketBaseAuthCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle ListAuthMethods(
        FOpenPocketBaseAuthMethodsCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle RequestOtp(
        FString Email,
        FOpenPocketBaseOtpRequestCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle AuthenticateWithPassword(
        FString Identity,
        FString Password,
        FOpenPocketBaseAuthAttemptCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle AuthenticateWithOtp(
        FString OtpId,
        FString Password,
        FOpenPocketBaseMfaContinuation Mfa,
        FOpenPocketBaseAuthAttemptCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle BeginOAuth2(
        FOpenPocketBaseOAuth2StartOptions Options,
        FOpenPocketBaseOAuth2AuthorizationCallback OnComplete) const;

    FOpenPocketBaseRequestHandle CompleteOAuth2(
        FOpenPocketBaseOAuth2Callback Callback,
        FOpenPocketBaseAuthAttemptCallback OnComplete) const;

    FOpenPocketBaseRequestHandle AuthenticateWithOAuth2(
        FOpenPocketBaseAssistedOAuth2Options Options,
        FOpenPocketBaseAuthAttemptCallback OnComplete) const;

    FOpenPocketBaseRequestHandle RequestPasswordReset(
        FString Email,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle ConfirmPasswordReset(
        FString Token,
        FString Password,
        FString PasswordConfirm,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle RequestVerification(
        FString Email,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle ConfirmVerification(
        FString Token,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle RequestEmailChange(
        FString NewEmail,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle ConfirmEmailChange(
        FString Token,
        FString Password,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle ListExternalAuths(
        FString RecordId,
        FOpenPocketBaseExternalAuthsCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle UnlinkExternalAuth(
        FString RecordId,
        FString Provider,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseSubscriptionHandle SubscribeToRecords(
        FOpenPocketBaseRealtimeCallbacks Callbacks,
        FOpenPocketBaseRealtimeOptions Options,
        FOpenPocketBaseError& OutError) const;

    FOpenPocketBaseSubscriptionHandle SubscribeToRecord(
        FString RecordId,
        FOpenPocketBaseRealtimeCallbacks Callbacks,
        FOpenPocketBaseRealtimeOptions Options,
        FOpenPocketBaseError& OutError) const;

    bool IsValid() const;

private:
    FOpenPocketBaseRequestHandle SendAccountPost(
        FString Route,
        TMap<FString, FString> BodyFields,
        bool bUseAuth,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options,
        TUniqueFunction<bool(FOpenPocketBaseError&)> OnSucceeded = {}) const;

    FOpenPocketBaseCollectionService(
        TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient,
        FString InCollection);

    TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    FString Collection;

    friend class FOpenPocketBaseClient;
};

class OPENPOCKETBASESDK_API FOpenPocketBaseClient final
    : public TSharedFromThis<FOpenPocketBaseClient, ESPMode::ThreadSafe>
{
public:
    static TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Create(
        const FOpenPocketBaseClientConfig& Config,
        FOpenPocketBaseError& OutError);

    static TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Create(
        const FOpenPocketBaseClientConfig& Config,
        TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
        FOpenPocketBaseError& OutError);

    static TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Create(
        const FOpenPocketBaseClientConfig& Config,
        TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
        TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore,
        FOpenPocketBaseError& OutError);

    static TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Create(
        const FOpenPocketBaseClientConfig& Config,
        TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
        TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore,
        TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock,
        FOpenPocketBaseError& OutError);

    static TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Create(
        const FOpenPocketBaseClientConfig& Config,
        TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
        TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore,
        TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock,
        TSharedRef<IOpenPocketBaseOAuthBrowser, ESPMode::ThreadSafe> OAuthBrowser,
        FOpenPocketBaseError& OutError);

    ~FOpenPocketBaseClient();

    FOpenPocketBaseCollectionService Collection(FString CollectionName);
    FOpenPocketBaseFileService Files();
    FString GetBaseUrl() const;
    FOpenPocketBaseCapabilityInfo GetCapability(EOpenPocketBaseCapability Capability) const;
    FOpenPocketBaseCapabilityReport GetCapabilityReport() const;
    bool IsAuthenticated() const;
    bool GetCurrentAuthRecord(FOpenPocketBaseRecord& OutRecord) const;
    bool GetCurrentSession(FOpenPocketBaseSessionSnapshot& OutSession) const;
    FOpenPocketBaseSessionChanged& OnSessionChanged();
    FOpenPocketBaseRequestHandle RefreshAuth(
        FOpenPocketBaseAuthCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    FOpenPocketBaseRequestHandle RestoreSession(
        bool bVerifyWithServer,
        FOpenPocketBaseSessionRestoreCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});
    void Logout();
    FOpenPocketBaseRequestHandle SendBatch(
        FOpenPocketBaseBatchRequest Batch,
        FOpenPocketBaseBatchCallback OnComplete,
        FOpenPocketBaseBatchOptions Options = {});
    FOpenPocketBaseSubscriptionHandle Subscribe(
        FString Topic,
        FOpenPocketBaseRealtimeCallbacks Callbacks,
        FOpenPocketBaseRealtimeOptions Options,
        FOpenPocketBaseError& OutError);
    void UnsubscribeTopic(const FString& Topic);
    void UnsubscribeAllRealtime();
    bool IsShutdown() const;
    void Shutdown();

private:
    struct FImpl;

    FOpenPocketBaseClient(
        FOpenPocketBaseClientConfig Config,
        FString NormalizedBaseUrl,
        TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
        TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore,
        TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock,
        TSharedRef<IOpenPocketBaseOAuthBrowser, ESPMode::ThreadSafe> OAuthBrowser);

    TUniquePtr<FImpl> Impl;

    friend class FOpenPocketBaseCollectionService;
    friend class FOpenPocketBaseFileService;
    friend class OpenPocketBase::Internal::FAssistedOAuthOperation;
};
