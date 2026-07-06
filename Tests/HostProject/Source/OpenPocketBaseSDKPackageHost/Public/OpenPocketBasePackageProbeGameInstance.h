#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/IHttpRequest.h"
#include "OpenPocketBaseClient.h"

#include "OpenPocketBasePackageProbeGameInstance.generated.h"

class FOpenPocketBasePackageStreamingState;

UCLASS()
class OPENPOCKETBASESDKPACKAGEHOST_API UOpenPocketBasePackageProbeGameInstance final : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;
    virtual void Shutdown() override;

private:
    void FinishTlsProbe(bool bReachedTrustedHttp, const FOpenPocketBaseError& Error);
    void BeginTransferProbe();
    void DownloadTransferFile(FOpenPocketBaseFileToken Token);
    void DeleteTransferRecord(bool bTransferSucceeded, FOpenPocketBaseError Error);
    void FinishTransferProbe(bool bSucceeded, const FOpenPocketBaseError& Error);
    void BeginRealtimeManagerProbe();
    void DeleteRealtimeManagerRecord();
    void FinishRealtimeManagerProbe(bool bSucceeded, const FOpenPocketBaseError& Error);
    void BeginStreamingProbe();
    void HandleStreamingCancellationComplete(bool bSucceeded);
    void BeginStreamingTimeoutProbe();
    void HandleStreamingTimeoutComplete(bool bTimedOut);
    void FinishStreamingProbe(bool bSucceeded, const TCHAR* Message);

    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    FOpenPocketBaseRequestHandle Request;
    FOpenPocketBaseSubscriptionHandle RealtimeSubscription;
    FString TransferOrigin;
    FString TransferRecordId;
    FString TransferFileName;
    FString UploadPath;
    FString DownloadPath;
    TArray<uint8> ExpectedTransferBytes;
    bool bUploadProgressVerified = false;
    bool bDownloadProgressVerified = false;
    bool bRealtimeMutationStarted = false;
    bool bRealtimeManagerFinished = false;
    FHttpRequestPtr StreamingRequest;
    TSharedPtr<FOpenPocketBasePackageStreamingState, ESPMode::ThreadSafe> StreamingState;
    double StreamingTimeoutStartedAt = 0;
};
