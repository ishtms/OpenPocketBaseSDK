#pragma once

#include "CoreMinimal.h"
#include "JsonObjectConverter.h"
#include "JsonObjectWrapper.h"
#include "OpenPocketBaseError.h"

#include "OpenPocketBaseRecord.generated.h"

UENUM(BlueprintType)
enum class EOpenPocketBaseFieldState : uint8
{
    Found,
    Missing,
    Null,
    WrongType
};

UENUM(BlueprintType)
enum class EOpenPocketBaseFieldModifier : uint8
{
    Replace,
    Append,
    Prepend,
    Remove
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseRecordBody
{
    GENERATED_BODY()

    FOpenPocketBaseRecordBody();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records")
    FJsonObjectWrapper Data;

    void SetStringField(
        const FString& FieldName,
        const FString& Value,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    void SetNumberField(
        const FString& FieldName,
        double Value,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    void SetBooleanField(
        const FString& FieldName,
        bool bValue,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    void SetNullField(
        const FString& FieldName,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    void SetStringArrayField(
        const FString& FieldName,
        const TArray<FString>& Value,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    static FString MakeModifiedFieldName(
        const FString& FieldName,
        EOpenPocketBaseFieldModifier Modifier);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseRecord
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    FString CollectionId;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    FString CollectionName;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    FDateTime Created;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    FDateTime Updated;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    FJsonObjectWrapper Data;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    FJsonObjectWrapper Expanded;

    template <typename StructType>
    bool TryConvertToStruct(StructType& OutStruct, FOpenPocketBaseError& OutError) const
    {
        if (!Data.JsonObject.IsValid())
        {
            OutError = FOpenPocketBaseError();
            OutError.Kind = EOpenPocketBaseErrorKind::Serialization;
            OutError.ServerMessage = TEXT("Record data does not contain a parsed JSON object.");
            return false;
        }

        FText FailureReason;
        if (!FJsonObjectConverter::JsonObjectToUStruct(
                Data.JsonObject.ToSharedRef(),
                &OutStruct,
                0,
                0,
                false,
                &FailureReason))
        {
            OutError = FOpenPocketBaseError();
            OutError.Kind = EOpenPocketBaseErrorKind::Serialization;
            OutError.ServerMessage = FailureReason.ToString();
            return false;
        }

        OutError = FOpenPocketBaseError();
        return true;
    }
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseRequestOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client", meta = (ClampMin = "0.0"))
    double TotalTimeoutSeconds = 30.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client", meta = (ClampMin = "0.0"))
    double ActivityTimeoutSeconds = 15.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client", AdvancedDisplay)
    FString RequestKey;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client", AdvancedDisplay)
    bool bCancelPreviousRequestWithSameKey = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client", AdvancedDisplay)
    TMap<FString, FString> AdditionalHeaders;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client", AdvancedDisplay)
    FString TraceParent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client", AdvancedDisplay)
    bool bRetryEligibleReads = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client", AdvancedDisplay, meta = (ClampMin = "0", ClampMax = "5"))
    int32 MaxReadRetries = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client", AdvancedDisplay, meta = (ClampMin = "0.0", ClampMax = "30.0"))
    double RetryBaseDelaySeconds = 0.25;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client", AdvancedDisplay, meta = (ClampMin = "0.0", ClampMax = "60.0"))
    double RetryMaxDelaySeconds = 2.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client", AdvancedDisplay, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    double RetryJitterFraction = 0.2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client", AdvancedDisplay, meta = (ClampMin = "1024", ClampMax = "67108864"))
    int64 MaxResponseBytes = 8 * 1024 * 1024;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseRecordOptions
{
    GENERATED_BODY()

    FOpenPocketBaseRecordOptions() = default;

    FOpenPocketBaseRecordOptions(FOpenPocketBaseRequestOptions InRequestOptions)
        : RequestOptions(MoveTemp(InRequestOptions))
    {
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records")
    TArray<FString> Expand;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records")
    TArray<FString> Fields;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records", AdvancedDisplay)
    FOpenPocketBaseRequestOptions RequestOptions;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseListOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records", meta = (ClampMin = "1"))
    int32 Page = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records", meta = (ClampMin = "1"))
    int32 PerPage = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records")
    FString Filter;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records")
    TArray<FString> Sort;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records")
    TArray<FString> Expand;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records")
    TArray<FString> Fields;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records", AdvancedDisplay)
    bool bSkipTotal = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records", AdvancedDisplay)
    FOpenPocketBaseRequestOptions RequestOptions;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseFullListOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records")
    FOpenPocketBaseListOptions ListOptions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records", meta = (ClampMin = "0", ClampMax = "1000000"))
    int32 MaxItems = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records", meta = (ClampMin = "0", ClampMax = "10000"))
    int32 MaxPages = 0;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseRecordPage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    int32 Page = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    int32 PerPage = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    TArray<FOpenPocketBaseRecord> Items;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    bool bHasTotalItems = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    int64 TotalItems = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    bool bHasTotalPages = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    int32 TotalPages = 0;

    TOptional<int64> GetOptionalTotalItems() const
    {
        return bHasTotalItems ? TOptional<int64>(TotalItems) : TOptional<int64>();
    }

    TOptional<int32> GetOptionalTotalPages() const
    {
        return bHasTotalPages ? TOptional<int32>(TotalPages) : TOptional<int32>();
    }
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseFullListResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    TArray<FOpenPocketBaseRecord> Items;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    int32 PagesFetched = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    bool bReachedEnd = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    bool bReachedItemLimit = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    bool bReachedPageLimit = false;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseAuthResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FOpenPocketBaseRecord Record;
};
