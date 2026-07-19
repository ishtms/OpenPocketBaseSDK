#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseRecord.h"

#include "OpenPocketBaseFile.generated.h"

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseFileInput
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    FOpenPocketBaseFileFieldRef Field;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    FString FileName;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    FString ContentType = TEXT("application/octet-stream");

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    bool bUseFilePath = true;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    FString FilePath;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    TArray<uint8> Bytes;

    UPROPERTY(Transient)
    FString DynamicFieldName;

    static FOpenPocketBaseFileInput FromPath(
        FOpenPocketBaseFileFieldRef Field,
        FString FilePath,
        FString FileName = {},
        FString ContentType = TEXT("application/octet-stream"),
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);
    static FOpenPocketBaseFileInput FromBytes(
        FOpenPocketBaseFileFieldRef Field,
        TArray<uint8> Bytes,
        FString FileName,
        FString ContentType = TEXT("application/octet-stream"),
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);
    static FOpenPocketBaseFileInput DynamicFromPath(
        FString FieldName,
        FString FilePath,
        FString FileName = {},
        FString ContentType = TEXT("application/octet-stream"),
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);
    static FOpenPocketBaseFileInput DynamicFromBytes(
        FString FieldName,
        TArray<uint8> Bytes,
        FString FileName,
        FString ContentType = TEXT("application/octet-stream"),
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    FString GetFieldName() const;
    bool IsValid() const;
    bool BelongsTo(const FOpenPocketBaseCollectionRef& Collection) const;
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

UENUM(BlueprintType)
enum class EOpenPocketBaseTransferPhase : uint8
{
    Uploading,
    Downloading,
    Finalizing
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseTransferProgress
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    int64 TransferredBytes = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    bool bHasTotalBytes = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    int64 TotalBytes = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    int32 Attempt = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    EOpenPocketBaseTransferPhase Phase = EOpenPocketBaseTransferPhase::Uploading;
};

using FOpenPocketBaseTransferProgressCallback =
    TFunction<void(const FOpenPocketBaseTransferProgress&)>;

UENUM(BlueprintType)
enum class EOpenPocketBaseThumbnailMode : uint8
{
    None,
    CropCenter,
    CropTop,
    CropBottom,
    Fit
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseThumbnailOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files", meta = (ClampMin = "0", ClampMax = "16384"))
    int32 Width = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files", meta = (ClampMin = "0", ClampMax = "16384"))
    int32 Height = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files")
    EOpenPocketBaseThumbnailMode Mode = EOpenPocketBaseThumbnailMode::None;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseFileUrlOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files")
    FOpenPocketBaseThumbnailOptions Thumbnail;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files")
    bool bForceDownload = false;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseFileToken
{
    GENERATED_BODY()

public:
    bool IsSet() const
    {
        return !Value.IsEmpty();
    }

private:
    UPROPERTY(Transient, meta = (AllowPrivateAccess = "true"))
    FString Value;

    friend class FOpenPocketBaseFileService;
};

UENUM(BlueprintType)
enum class EOpenPocketBaseFileDownloadTarget : uint8
{
    Memory,
    File
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseFileDownloadOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files")
    EOpenPocketBaseFileDownloadTarget Target = EOpenPocketBaseFileDownloadTarget::Memory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files", meta = (EditCondition = "Target == EOpenPocketBaseFileDownloadTarget::File"))
    FString DestinationPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files", meta = (EditCondition = "Target == EOpenPocketBaseFileDownloadTarget::File"))
    bool bReplaceExisting = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files", meta = (ClampMin = "1", ClampMax = "17179869184"))
    int64 MaxBytes = 64 * 1024 * 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files")
    FOpenPocketBaseFileUrlOptions UrlOptions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Files")
    FOpenPocketBaseRequestOptions RequestOptions;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseFileDownloadResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    TArray<uint8> Bytes;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    bool bSavedToFile = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    FString DestinationPath;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    int32 HttpStatus = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    FString ContentType;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    int64 ContentLength = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    FString FileName;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    FString ETag;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Files")
    FString LastModified;
};
