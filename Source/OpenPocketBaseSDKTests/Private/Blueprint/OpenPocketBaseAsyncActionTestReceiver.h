#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseBatch.h"
#include "OpenPocketBaseRecord.h"

#include "OpenPocketBaseAsyncActionTestReceiver.generated.h"

UCLASS()
class UOpenPocketBaseAsyncActionTestReceiver final : public UObject
{
    GENERATED_BODY()

public:
    bool bFailed = false;
    FOpenPocketBaseError Error;

    UFUNCTION()
    void HandleRecordFailure(FOpenPocketBaseRecord Record, FOpenPocketBaseError InError);

    UFUNCTION()
    void HandleBatchFailure(FOpenPocketBaseBatchResult Result, FOpenPocketBaseError InError);
};
