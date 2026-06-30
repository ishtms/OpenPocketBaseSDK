#pragma once

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
using FOpenPocketBaseAuthCallback =
    TUniqueFunction<void(TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&&)>;

class OPENPOCKETBASESDK_API FOpenPocketBaseCollectionService
{
public:
    FOpenPocketBaseRequestHandle GetOne(
        FString RecordId,
        FOpenPocketBaseRecordCallback OnComplete,
        FOpenPocketBaseRequestOptions Options = {}) const;

    FOpenPocketBaseRequestHandle GetList(
        FOpenPocketBaseListOptions Options,
        FOpenPocketBaseRecordPageCallback OnComplete) const;

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
