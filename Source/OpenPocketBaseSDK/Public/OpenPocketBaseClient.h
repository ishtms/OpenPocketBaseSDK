#pragma once

#include "OpenPocketBaseBatch.h"
#include "OpenPocketBaseClientConfig.h"
#include "OpenPocketBaseFile.h"
#include "OpenPocketBaseRecord.h"
#include "OpenPocketBaseRequestHandle.h"
#include "OpenPocketBaseResult.h"
#include "OpenPocketBaseSession.h"
#include "Clock/OpenPocketBaseClock.h"
#include "Templates/SharedPointer.h"
#include "Templates/Function.h"
#include "SecureStorage/OpenPocketBaseSecureStore.h"
#include "Transport/OpenPocketBaseTransport.h"

class FOpenPocketBaseClient;
class FOpenPocketBaseFileService;

using FOpenPocketBaseRecordCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseRecord>&&)>;
using FOpenPocketBaseRecordPageCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseRecordPage>&&)>;
using FOpenPocketBaseFullListCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseFullListResult>&&)>;
using FOpenPocketBaseAuthCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&&)>;
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

    bool IsValid() const;

private:
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

    ~FOpenPocketBaseClient();

    FOpenPocketBaseCollectionService Collection(FString CollectionName);
    FOpenPocketBaseFileService Files();
    FString GetBaseUrl() const;
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
    bool IsShutdown() const;
    void Shutdown();

private:
    struct FImpl;

    FOpenPocketBaseClient(
        FOpenPocketBaseClientConfig Config,
        FString NormalizedBaseUrl,
        TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
        TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore,
        TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock);

    TUniquePtr<FImpl> Impl;

    friend class FOpenPocketBaseCollectionService;
    friend class FOpenPocketBaseFileService;
};
