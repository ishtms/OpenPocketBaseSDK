// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"
#include "JsonObjectWrapper.h"
#include "OpenPocketBaseFile.h"
#include "OpenPocketBaseRecord.h"

#include "OpenPocketBaseAdminTypes.generated.h"

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminPolicy
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin")
    bool bEnablePrivilegedRequests = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin")
    bool bAllowInShipping = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin")
    bool bAllowDestructiveCollectionImport = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin")
    bool bAllowBackupRestore = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin")
    bool bAllowSqlWrites = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin")
    bool bAllowImpersonation = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin", meta = (ClampMin = "1", ClampMax = "500"))
    int32 MaxPageSize = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin", meta = (ClampMin = "1024", ClampMax = "67108864"))
    int64 MaxRequestBytes = 8 * 1024 * 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin", meta = (ClampMin = "1024", ClampMax = "67108864"))
    int64 MaxResponseBytes = 8 * 1024 * 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin", meta = (ClampMin = "1024", ClampMax = "67108864"))
    int64 MaxBackupBytes = 64 * 1024 * 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin", meta = (ClampMin = "1", ClampMax = "1000"))
    int32 MaxSqlRows = 1000;
};

struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminBackupInput
{
    static FOpenPocketBaseAdminBackupInput FromPath(
        FString FilePath,
        FString FileName = {});
    static FOpenPocketBaseAdminBackupInput FromBytes(
        TArray<uint8> Bytes,
        FString FileName);

    bool IsValid() const;
    FOpenPocketBaseFileInput ToFileInput() &&;

private:
    FString FilePath;
    FString FileName;
    TArray<uint8> Bytes;
    bool bUseFilePath = false;
};

UENUM(BlueprintType)
enum class EOpenPocketBaseAdminCollectionTextField : uint8
{
    Id,
    Name
};

UENUM(BlueprintType)
enum class EOpenPocketBaseAdminCollectionDateField : uint8
{
    Created,
    Updated
};

UENUM(BlueprintType)
enum class EOpenPocketBaseAdminCollectionSortField : uint8
{
    Random,
    Id,
    Created,
    Updated,
    Name,
    Type,
    System
};

UENUM(BlueprintType)
enum class EOpenPocketBaseAdminCollectionProjectionField : uint8
{
    Id,
    Created,
    Updated,
    Name,
    Type,
    System,
    Fields,
    Indexes,
    ListRule,
    ViewRule,
    CreateRule,
    UpdateRule,
    DeleteRule,
    ViewQuery,
    AuthRule,
    ManageRule,
    AuthAlert,
    OAuth2,
    PasswordAuth,
    Mfa UMETA(DisplayName = "MFA"),
    Otp UMETA(DisplayName = "OTP"),
    AuthToken,
    PasswordResetToken,
    EmailChangeToken,
    VerificationToken,
    FileToken,
    VerificationTemplate,
    ResetPasswordTemplate,
    ConfirmEmailChangeTemplate
};

UENUM(BlueprintType)
enum class EOpenPocketBaseAdminLogTextField : uint8
{
    Id,
    Message
};

UENUM(BlueprintType)
enum class EOpenPocketBaseAdminLogDateField : uint8
{
    Created,
    Updated
};

UENUM(BlueprintType)
enum class EOpenPocketBaseAdminLogSortField : uint8
{
    Random,
    RowId UMETA(DisplayName = "Row ID"),
    Id,
    Created,
    Updated,
    Level,
    Message
};

UENUM(BlueprintType)
enum class EOpenPocketBaseAdminLogProjectionField : uint8
{
    Id,
    Created,
    Updated,
    Level,
    Message,
    Data
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminCollectionFilter
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin|Collections")
    FString Expression;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin|Collections")
    bool bValid = true;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin|Collections")
    FString ErrorMessage;

    bool IsEmpty() const;
    bool IsValid() const;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminLogFilter
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin|Logs")
    FString Expression;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin|Logs")
    bool bValid = true;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin|Logs")
    FString ErrorMessage;

    bool IsEmpty() const;
    bool IsValid() const;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminCollectionSort
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin|Collections")
    EOpenPocketBaseAdminCollectionSortField Field =
        EOpenPocketBaseAdminCollectionSortField::Created;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin|Collections")
    EOpenPocketBaseSortDirection Direction = EOpenPocketBaseSortDirection::Ascending;

    FString ToQueryValue() const;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminLogSort
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin|Logs")
    EOpenPocketBaseAdminLogSortField Field = EOpenPocketBaseAdminLogSortField::Created;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin|Logs")
    EOpenPocketBaseSortDirection Direction = EOpenPocketBaseSortDirection::Ascending;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin|Logs")
    FString DynamicDataField;

    FString ToQueryValue() const;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminCollectionListOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin", meta = (ClampMin = "1"))
    int32 Page = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin", meta = (ClampMin = "1", ClampMax = "500"))
    int32 PerPage = 30;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    FOpenPocketBaseAdminCollectionFilter Filter;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    TArray<FOpenPocketBaseAdminCollectionSort> Sort;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    TArray<EOpenPocketBaseAdminCollectionProjectionField> Fields;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin")
    FOpenPocketBaseRequestOptions RequestOptions;

    FOpenPocketBaseAdminCollectionListOptions& Where(
        FOpenPocketBaseAdminCollectionFilter InFilter);
    FOpenPocketBaseAdminCollectionListOptions& ThenSortBy(
        EOpenPocketBaseAdminCollectionSortField Field,
        EOpenPocketBaseSortDirection Direction = EOpenPocketBaseSortDirection::Ascending);
    FOpenPocketBaseAdminCollectionListOptions& Select(
        EOpenPocketBaseAdminCollectionProjectionField Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminLogListOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin", meta = (ClampMin = "1"))
    int32 Page = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin", meta = (ClampMin = "1", ClampMax = "500"))
    int32 PerPage = 30;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    FOpenPocketBaseAdminLogFilter Filter;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    TArray<FOpenPocketBaseAdminLogSort> Sort;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    TArray<EOpenPocketBaseAdminLogProjectionField> Fields;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin")
    FOpenPocketBaseRequestOptions RequestOptions;

    FOpenPocketBaseAdminLogListOptions& Where(FOpenPocketBaseAdminLogFilter InFilter);
    FOpenPocketBaseAdminLogListOptions& ThenSortBy(
        EOpenPocketBaseAdminLogSortField Field,
        EOpenPocketBaseSortDirection Direction = EOpenPocketBaseSortDirection::Ascending);
    FOpenPocketBaseAdminLogListOptions& ThenSortByDynamicDataField(
        FString DataField,
        EOpenPocketBaseSortDirection Direction = EOpenPocketBaseSortDirection::Ascending);
    FOpenPocketBaseAdminLogListOptions& Select(EOpenPocketBaseAdminLogProjectionField Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseDynamicAdminListOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin|Advanced", meta = (ClampMin = "1"))
    int32 Page = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin|Advanced", meta = (ClampMin = "1", ClampMax = "500"))
    int32 PerPage = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin|Advanced")
    FString DynamicFilter;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin|Advanced")
    TArray<FString> DynamicSort;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin|Advanced")
    TArray<FString> DynamicFields;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin|Advanced")
    FOpenPocketBaseRequestOptions RequestOptions;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminDocument
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin")
    FJsonObjectWrapper Data;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminPage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    int32 Page = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    int32 PerPage = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    int64 TotalItems = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    int32 TotalPages = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    TArray<FJsonObjectWrapper> Items;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminDocumentList
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    TArray<FJsonObjectWrapper> Items;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminIdentity
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    FString Email;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminBackup
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    FString Key;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    int64 Size = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    FDateTime Modified;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminBackupList
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    TArray<FOpenPocketBaseAdminBackup> Items;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminBackupDownload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    TArray<uint8> Bytes;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    FString ContentType;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminSqlColumn
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    FString Type;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    bool bNullable = false;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminSqlResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    int64 ExecutionTimeMilliseconds = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    int64 AffectedRows = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    int32 RowCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    TArray<FOpenPocketBaseAdminSqlColumn> Columns;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Admin")
    FJsonObjectWrapper Data;
};
