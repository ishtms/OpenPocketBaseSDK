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
        meta = (DisplayName = "With Create", Keywords = "batch add record"))
    static FOpenPocketBaseBatchRequest WithCreate(
        FOpenPocketBaseBatchRequest Batch,
        FOpenPocketBaseCollection Collection,
        FOpenPocketBaseRecordBody Body,
        const TArray<FString>& Expand,
        const TArray<FString>& Fields);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Batch",
        meta = (DisplayName = "With Update", Keywords = "batch add record"))
    static FOpenPocketBaseBatchRequest WithUpdate(
        FOpenPocketBaseBatchRequest Batch,
        FOpenPocketBaseCollection Collection,
        const FString& RecordId,
        FOpenPocketBaseRecordBody Body,
        const TArray<FString>& Expand,
        const TArray<FString>& Fields);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Batch",
        meta = (DisplayName = "With Upsert", Keywords = "batch add record"))
    static FOpenPocketBaseBatchRequest WithUpsert(
        FOpenPocketBaseBatchRequest Batch,
        FOpenPocketBaseCollection Collection,
        FOpenPocketBaseRecordBody Body,
        const TArray<FString>& Expand,
        const TArray<FString>& Fields);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Batch",
        meta = (DisplayName = "With Delete", Keywords = "batch add remove record"))
    static FOpenPocketBaseBatchRequest WithDelete(
        FOpenPocketBaseBatchRequest Batch,
        FOpenPocketBaseCollection Collection,
        const FString& RecordId);
};
