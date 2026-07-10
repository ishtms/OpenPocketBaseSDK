#pragma once

#include "CoreMinimal.h"
#include "AsyncActions/OpenPocketBaseRecordAsyncActions.h"
#include "OpenPocketBaseBatch.h"

#include "OpenPocketBaseBatchAsyncAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOpenPocketBaseBatchActionSuccess,
    FOpenPocketBaseBatchResult,
    Result,
    FOpenPocketBaseError,
    Error);

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class OPENPOCKETBASESDK_API UOpenPocketBaseSendBatchAsyncAction final
    : public UOpenPocketBaseAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseBatchActionSuccess Success;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseBatchActionSuccess Failed;

    UPROPERTY(BlueprintAssignable)
    FOpenPocketBaseBatchActionSuccess Cancelled;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Records|Batch",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Send Batch"))
    static UOpenPocketBaseSendBatchAsyncAction* SendBatch(
        UOpenPocketBaseClient* PocketBaseClient,
        FOpenPocketBaseBatchRequest Batch,
        FOpenPocketBaseBatchOptions Options);

    virtual void Activate() override;

protected:
    virtual void BroadcastCancelled() override;

private:
    FOpenPocketBaseBatchRequest Batch;
    FOpenPocketBaseBatchOptions Options;
};
