#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "OpenPocketBaseClient.h"

#include "OpenPocketBasePackageProbeGameInstance.generated.h"

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

    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    FOpenPocketBaseRequestHandle Request;
    FString TransferOrigin;
    FString TransferRecordId;
    FString TransferFileName;
    FString UploadPath;
    FString DownloadPath;
    TArray<uint8> ExpectedTransferBytes;
    bool bUploadProgressVerified = false;
    bool bDownloadProgressVerified = false;
};
