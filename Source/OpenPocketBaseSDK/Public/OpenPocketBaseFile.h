#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseRecord.h"

#include "OpenPocketBaseFile.generated.h"

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseFileInput
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files")
    FString FieldName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files")
    FString FileName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files")
    FString ContentType = TEXT("application/octet-stream");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files")
    EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files")
    bool bUseFilePath = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files", meta = (EditCondition = "bUseFilePath"))
    FString FilePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files", meta = (EditCondition = "!bUseFilePath"))
    TArray<uint8> Bytes;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseUploadLimits
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files", meta = (ClampMin = "1", ClampMax = "100"))
    int32 MaxFiles = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files", meta = (ClampMin = "1", ClampMax = "67108864"))
    int64 MaxInlineFileBytes = 8 * 1024 * 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files", meta = (ClampMin = "1", ClampMax = "17179869184"))
    int64 MaxSourceFileBytes = 2LL * 1024 * 1024 * 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files", meta = (ClampMin = "1", ClampMax = "17179869184"))
    int64 MaxTotalBodyBytes = 4LL * 1024 * 1024 * 1024;
};
