#pragma once

#include "OpenPocketBaseBatch.h"
#include "OpenPocketBaseAuthentication.h"
#include "OpenPocketBaseCapability.h"
#include "OpenPocketBaseClientConfig.h"
#include "OpenPocketBaseCustomRoute.h"
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

using FOpenPocketBaseClientRef =
    TSharedRef<FOpenPocketBaseClient, ESPMode::ThreadSafe>;
using FOpenPocketBaseClientResult =
    TOpenPocketBaseResult<FOpenPocketBaseClientRef>;
using FOpenPocketBaseFileUrlResult =
    TOpenPocketBaseResult<FString>;
using FOpenPocketBaseSubscriptionResult =
    TOpenPocketBaseResult<FOpenPocketBaseSubscriptionHandle>;

struct OPENPOCKETBASESDK_API FOpenPocketBaseClientDependencies
{
    TSharedPtr<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport;
    TSharedPtr<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore;
    TSharedPtr<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock;
    TSharedPtr<IOpenPocketBaseOAuthBrowser, ESPMode::ThreadSafe> OAuthBrowser;
};

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
using FOpenPocketBaseHealthCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseHealthResult>&&)>;
using FOpenPocketBaseCustomRouteCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseCustomRouteResponse>&&)>;
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
    FOpenPocketBaseFileUrlResult BuildUrl(
        FString Collection,
        FString RecordId,
        FString FileName,
        FOpenPocketBaseFileUrlOptions Options = {}) const;

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
        FOpenPocketBaseFilter Filter,
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

    FOpenPocketBaseRequestHandle ListAuthMethods(
        FOpenPocketBaseAuthMethodsCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle RequestOtp(
        FString Email,
        FOpenPocketBaseOtpRequestCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle AuthWithPassword(
        FString Identity,
        FString Password,
        FOpenPocketBaseAuthAttemptCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle AuthWithOtp(
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

    FOpenPocketBaseRequestHandle AuthWithOAuth2(
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

    FOpenPocketBaseSubscriptionResult SubscribeToRecords(
        FOpenPocketBaseRealtimeCallbacks Callbacks,
        FOpenPocketBaseRealtimeOptions Options = {}) const;

    FOpenPocketBaseSubscriptionResult SubscribeToRecord(
        FString RecordId,
        FOpenPocketBaseRealtimeCallbacks Callbacks,
        FOpenPocketBaseRealtimeOptions Options = {}) const;

    bool IsValid() const;

private:
    bool ValidateRecordOptions(
        const FOpenPocketBaseRecordOptions& Options,
        FOpenPocketBaseError& OutError) const;
    bool ValidateListOptions(
        const FOpenPocketBaseListOptions& Options,
        FOpenPocketBaseError& OutError) const;
    bool ValidateBody(
        const FOpenPocketBaseRecordBody& Body,
        FOpenPocketBaseError& OutError) const;

    FOpenPocketBaseRequestHandle SendAccountPost(
        FString Route,
        TMap<FString, FString> BodyFields,
        bool bUseAuth,
        FOpenPocketBaseBoolCallback OnComplete,
        FOpenPocketBaseRequestOptions Options,
        TUniqueFunction<bool(FOpenPocketBaseError&)> OnSucceeded = {}) const;

    FOpenPocketBaseCollectionService(
        TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient,
        FOpenPocketBaseCollectionRef InCollection);
    FOpenPocketBaseCollectionService(
        TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InClient,
        FString InCollection);

    TWeakPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    FString Collection;
    FOpenPocketBaseCollectionRef Reference;

    friend class FOpenPocketBaseClient;
};

class OPENPOCKETBASESDK_API FOpenPocketBaseClient final
    : public TSharedFromThis<FOpenPocketBaseClient, ESPMode::ThreadSafe>
{
public:
    static FOpenPocketBaseClientResult Create(
        const FOpenPocketBaseClientConfig& Config,
        FOpenPocketBaseClientDependencies Dependencies = {});

    static FOpenPocketBaseClientResult CreateEphemeralAuthenticated(
        const FOpenPocketBaseClientConfig& Config,
        FString Token,
        FString AuthCollection,
        const FOpenPocketBaseRecord& AuthRecord,
        FOpenPocketBaseClientDependencies Dependencies = {});

    ~FOpenPocketBaseClient();

    FOpenPocketBaseCollectionService Collection(FOpenPocketBaseCollectionRef CollectionReference);
    FOpenPocketBaseCollectionService DynamicCollection(FString CollectionName);
    FOpenPocketBaseFileService Files();

    FOpenPocketBaseRequestHandle Health(
        FOpenPocketBaseHealthCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {});

    FOpenPocketBaseRequestHandle SendCustomRoute(
        FOpenPocketBaseCustomRouteRequest Request,
        FOpenPocketBaseCustomRouteCallback OnComplete);
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
    FOpenPocketBaseSubscriptionResult Subscribe(
        FString Topic,
        FOpenPocketBaseRealtimeCallbacks Callbacks = {},
        FOpenPocketBaseRealtimeOptions Options = {});
    void UnsubscribeTopic(const FString& Topic);
    void UnsubscribeAllRealtime();
    bool IsShutdown() const;
    void Shutdown();

private:
    struct FImpl;

    static TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> CreateInternal(
        const FOpenPocketBaseClientConfig& Config,
        TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
        TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore,
        TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock,
        TSharedRef<IOpenPocketBaseOAuthBrowser, ESPMode::ThreadSafe> OAuthBrowser,
        FOpenPocketBaseError& OutError);

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
