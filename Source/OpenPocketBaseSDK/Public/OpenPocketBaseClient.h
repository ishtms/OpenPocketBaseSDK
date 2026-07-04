#pragma once

#include "OpenPocketBaseBatch.h"
#include "OpenPocketBaseClientConfig.h"
#include "OpenPocketBaseRecord.h"
#include "OpenPocketBaseRequestHandle.h"
#include "OpenPocketBaseResult.h"
#include "Templates/SharedPointer.h"
#include "Templates/Function.h"
#include "Transport/OpenPocketBaseTransport.h"

class FOpenPocketBaseClient;

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

    FOpenPocketBaseRequestHandle Update(
        FString RecordId,
        FOpenPocketBaseRecordBody Body,
        FOpenPocketBaseRecordCallback OnComplete,
        FOpenPocketBaseRecordOptions Options = {}) const;

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

    ~FOpenPocketBaseClient();

    FOpenPocketBaseCollectionService Collection(FString CollectionName);
    FString GetBaseUrl() const;
    bool IsAuthenticated() const;
    bool GetCurrentAuthRecord(FOpenPocketBaseRecord& OutRecord) const;
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
        TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport);

    TUniquePtr<FImpl> Impl;

    friend class FOpenPocketBaseCollectionService;
};
