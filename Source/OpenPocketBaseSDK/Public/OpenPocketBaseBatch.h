#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseRecord.h"

#include "OpenPocketBaseBatch.generated.h"

class UOpenPocketBaseClient;

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
    FOpenPocketBaseWritableCollectionRef Collection;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    FString RecordId;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    FOpenPocketBaseRecordBody Body;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    FOpenPocketBaseRecordOptions ResponseOptions;

    UPROPERTY(Transient)
    FString DynamicCollection;

    UPROPERTY(Transient)
    TArray<FString> DynamicExpand;

    UPROPERTY(Transient)
    TArray<FString> DynamicFields;

    FString GetCollectionName() const;
    FString GetExpandQuery() const;
    FString GetFieldsQuery() const;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseBatchRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Batch")
    TArray<FOpenPocketBaseBatchEntry> Entries;

    UPROPERTY(Transient)
    TObjectPtr<UOpenPocketBaseClient> Client;

    UPROPERTY(Transient)
    bool bValid = true;

    UPROPERTY(Transient)
    FString ErrorMessage;

    bool IsValid() const;
    UOpenPocketBaseClient* GetClient() const;
    void BindClient(UOpenPocketBaseClient* InClient);
    void Invalidate(FString Message);

    FOpenPocketBaseBatchRequest& AddCreate(
        FOpenPocketBaseWritableCollectionRef Collection,
        FOpenPocketBaseRecordBody Body,
        FOpenPocketBaseRecordOptions ResponseOptions = {});

    FOpenPocketBaseBatchRequest& AddUpdate(
        FOpenPocketBaseWritableCollectionRef Collection,
        FString RecordId,
        FOpenPocketBaseRecordBody Body,
        FOpenPocketBaseRecordOptions ResponseOptions = {});

    FOpenPocketBaseBatchRequest& AddUpsert(
        FOpenPocketBaseWritableCollectionRef Collection,
        FOpenPocketBaseRecordBody Body,
        FOpenPocketBaseRecordOptions ResponseOptions = {});

    FOpenPocketBaseBatchRequest& AddDelete(
        FOpenPocketBaseWritableCollectionRef Collection,
        FString RecordId);

    FOpenPocketBaseBatchRequest& AddDynamicCreate(
        FString Collection,
        FOpenPocketBaseRecordBody Body,
        TArray<FString> Expand = {},
        TArray<FString> Fields = {});
    FOpenPocketBaseBatchRequest& AddDynamicUpdate(
        FString Collection,
        FString RecordId,
        FOpenPocketBaseRecordBody Body,
        TArray<FString> Expand = {},
        TArray<FString> Fields = {});
    FOpenPocketBaseBatchRequest& AddDynamicUpsert(
        FString Collection,
        FOpenPocketBaseRecordBody Body,
        TArray<FString> Expand = {},
        TArray<FString> Fields = {});
    FOpenPocketBaseBatchRequest& AddDynamicDelete(FString Collection, FString RecordId);
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records|Batch")
    FOpenPocketBaseRequestOptions RequestOptions;
};
