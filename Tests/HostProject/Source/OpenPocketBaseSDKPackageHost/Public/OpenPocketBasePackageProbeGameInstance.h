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
    void FinishProbe(bool bReachedTrustedHttp, const FOpenPocketBaseError& Error);

    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    FOpenPocketBaseRequestHandle Request;
};
