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

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminListOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin", meta = (ClampMin = "1"))
    int32 Page = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin", meta = (ClampMin = "1", ClampMax = "500"))
    int32 PerPage = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin")
    FString Filter;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin")
    TArray<FString> Sort;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin")
    TArray<FString> Fields;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Admin")
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
