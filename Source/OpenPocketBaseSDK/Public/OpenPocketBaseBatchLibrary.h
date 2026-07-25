#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseBatch.h"
#include "OpenPocketBaseBlueprintClient.h"

#include "OpenPocketBaseBatchLibrary.generated.h"

UCLASS()
class OPENPOCKETBASESDK_API UOpenPocketBaseBatchLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Batch",
        meta = (DisplayName = "New Batch", NativeMakeFunc))
    static FOpenPocketBaseBatchRequest NewBatch();

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Batch",
        meta = (DisplayName = "Batch Options", NativeMakeFunc))
    static FOpenPocketBaseBatchOptions NewBatchOptions(
        int32 MaxOperations = 50,
        int64 MaxBodyBytes = 8388608);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Batch",
        meta = (
            BlueprintInternalUseOnly = "true",
            DisplayName = "Batch Options",
            ToolTip = "Preserves batch options created by an older Open PocketBase Make Struct node during automatic Blueprint migration.",
            Keywords = "pocketbase batch options migrate legacy"))
    static FOpenPocketBaseBatchOptions MakeLegacyBatchOptions(
        int32 MaxOperations,
        int64 MaxBodyBytes,
        FOpenPocketBaseRequestOptions RequestOptions);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Batch",
        meta = (DisplayName = "With Request Options"))
    static FOpenPocketBaseBatchOptions BatchOptionsWithRequestOptions(
        FOpenPocketBaseBatchOptions Options,
        FOpenPocketBaseRequestOptions RequestOptions);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Batch",
        meta = (DisplayName = "With Create", ToolTip = "Returns a copy of the batch with one create-record operation added.", Keywords = "pocketbase batch add create record"))
    static FOpenPocketBaseBatchRequest WithCreate(
        FOpenPocketBaseBatchRequest Batch,
        UPARAM(meta = (OpenPocketBaseCollectionAccess = "Write")) FOpenPocketBaseCollection Collection,
        FOpenPocketBaseRecordBody Body,
        FOpenPocketBaseRecordOptions ResponseOptions);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Batch",
        meta = (DisplayName = "With Update", Keywords = "batch add record"))
    static FOpenPocketBaseBatchRequest WithUpdate(
        FOpenPocketBaseBatchRequest Batch,
        UPARAM(meta = (OpenPocketBaseCollectionAccess = "Write")) FOpenPocketBaseCollection Collection,
        const FString& RecordId,
        FOpenPocketBaseRecordBody Body,
        FOpenPocketBaseRecordOptions ResponseOptions);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Batch",
        meta = (DisplayName = "With Upsert", Keywords = "batch add record"))
    static FOpenPocketBaseBatchRequest WithUpsert(
        FOpenPocketBaseBatchRequest Batch,
        UPARAM(meta = (OpenPocketBaseCollectionAccess = "Write")) FOpenPocketBaseCollection Collection,
        const FString& RecordId,
        FOpenPocketBaseRecordBody Body,
        FOpenPocketBaseRecordOptions ResponseOptions);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Batch",
        meta = (DisplayName = "With Delete", Keywords = "batch add remove record"))
    static FOpenPocketBaseBatchRequest WithDelete(
        FOpenPocketBaseBatchRequest Batch,
        UPARAM(meta = (OpenPocketBaseCollectionAccess = "Write")) FOpenPocketBaseCollection Collection,
        const FString& RecordId);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Batch",
        meta = (DisplayName = "Break Open Pocket Base Batch Result", NativeBreakFunc))
    static void BreakBatchResult(
        const FOpenPocketBaseBatchResult& Result,
        TArray<FOpenPocketBaseBatchOperationResult>& Results);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Batch",
        meta = (DisplayName = "Break Open Pocket Base Batch Operation Result", NativeBreakFunc))
    static void BreakBatchOperationResult(
        const FOpenPocketBaseBatchOperationResult& Result,
        EOpenPocketBaseBatchOperation& Operation,
        int32& HttpStatus,
        bool& bHasReturnedRecord,
        FOpenPocketBaseRecord& Record);
};
