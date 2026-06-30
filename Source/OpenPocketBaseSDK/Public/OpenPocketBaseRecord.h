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

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseRecordBody
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Records")
    FJsonObjectWrapper Data;
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
struct OPENPOCKETBASESDK_API FOpenPocketBaseAuthResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FOpenPocketBaseRecord Record;
};
