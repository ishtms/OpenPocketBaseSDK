#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseBatch.h"

#include "OpenPocketBaseBatchLibrary.generated.h"

UCLASS()
class OPENPOCKETBASESDK_API UOpenPocketBaseBatchLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records|Batch")
    static void AddCreate(
        UPARAM(ref) FOpenPocketBaseBatchRequest& Batch,
        const FString& Collection,
        FOpenPocketBaseRecordBody Body,
        const TArray<FString>& Expand,
        const TArray<FString>& Fields);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records|Batch")
    static void AddUpdate(
        UPARAM(ref) FOpenPocketBaseBatchRequest& Batch,
        const FString& Collection,
        const FString& RecordId,
        FOpenPocketBaseRecordBody Body,
        const TArray<FString>& Expand,
        const TArray<FString>& Fields);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records|Batch")
    static void AddUpsert(
        UPARAM(ref) FOpenPocketBaseBatchRequest& Batch,
        const FString& Collection,
        FOpenPocketBaseRecordBody Body,
        const TArray<FString>& Expand,
        const TArray<FString>& Fields);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records|Batch")
    static void AddDelete(
        UPARAM(ref) FOpenPocketBaseBatchRequest& Batch,
        const FString& Collection,
        const FString& RecordId);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records|Batch")
    static void Clear(UPARAM(ref) FOpenPocketBaseBatchRequest& Batch);
};
