#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseRecord.h"

#include "OpenPocketBaseBatch.generated.h"

UENUM(BlueprintType)
enum class EOpenPocketBaseBatchOperation : uint8
{
    Create,
    Update,
    Upsert,
    Delete
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseBatchEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    EOpenPocketBaseBatchOperation Operation = EOpenPocketBaseBatchOperation::Create;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    FString Collection;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    FString RecordId;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    FOpenPocketBaseRecordBody Body;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    TArray<FString> Expand;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    TArray<FString> Fields;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseBatchRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    TArray<FOpenPocketBaseBatchEntry> Entries;

    void AddCreate(
        FString Collection,
        FOpenPocketBaseRecordBody Body,
        TArray<FString> Expand = {},
        TArray<FString> Fields = {});

    void AddUpdate(
        FString Collection,
        FString RecordId,
        FOpenPocketBaseRecordBody Body,
        TArray<FString> Expand = {},
        TArray<FString> Fields = {});

    void AddUpsert(
        FString Collection,
        FOpenPocketBaseRecordBody Body,
        TArray<FString> Expand = {},
        TArray<FString> Fields = {});

    void AddDelete(FString Collection, FString RecordId);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseBatchOperationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    EOpenPocketBaseBatchOperation Operation = EOpenPocketBaseBatchOperation::Create;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    int32 HttpStatus = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    bool bHasRecord = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    FOpenPocketBaseRecord Record;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseBatchResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    TArray<FOpenPocketBaseBatchOperationResult> Results;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseBatchOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records|Batch", meta = (ClampMin = "1", ClampMax = "50"))
    int32 MaxOperations = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records|Batch", meta = (ClampMin = "1024", ClampMax = "16777216"))
    int64 MaxBodyBytes = 8 * 1024 * 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records|Batch", AdvancedDisplay)
    FOpenPocketBaseRequestOptions RequestOptions;
};
