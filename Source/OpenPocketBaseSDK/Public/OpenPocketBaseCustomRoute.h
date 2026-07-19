#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "JsonObjectWrapper.h"
#include "OpenPocketBaseFile.h"
#include "OpenPocketBaseRecord.h"

#include "OpenPocketBaseCustomRoute.generated.h"

UENUM(BlueprintType)
enum class EOpenPocketBaseCustomRouteMethod : uint8
{
    Get,
    Post,
    Put,
    Patch,
    Delete
};

UENUM(BlueprintType)
enum class EOpenPocketBaseCustomBodyFormat : uint8
{
    None,
    Json,
    Form,
    Multipart,
    Raw,
    Binary
};

UENUM(BlueprintType)
enum class EOpenPocketBaseJsonRootType : uint8
{
    None,
    Object,
    Array,
    Scalar
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseHealthResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    bool bHealthy = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    int32 HttpStatus = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    int32 Code = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    FString Message;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    double DurationSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseCustomRouteRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Utilities")
    EOpenPocketBaseCustomRouteMethod Method = EOpenPocketBaseCustomRouteMethod::Get;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Utilities")
    FString Path;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Utilities")
    TMap<FString, FString> Query;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Utilities")
    bool bUseAuth = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Utilities")
    EOpenPocketBaseCustomBodyFormat BodyFormat = EOpenPocketBaseCustomBodyFormat::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Utilities")
    FJsonObjectWrapper JsonBody;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Utilities")
    TMap<FString, FString> FormFields;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Utilities")
    TArray<FOpenPocketBaseFileInput> Files;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Utilities")
    TArray<uint8> Body;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Utilities")
    FString ContentType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Utilities", AdvancedDisplay)
    FOpenPocketBaseUploadLimits UploadLimits;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Utilities", AdvancedDisplay, meta = (ClampMin = "0", ClampMax = "67108864"))
    int64 MaxRequestBytes = 8 * 1024 * 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Utilities")
    FOpenPocketBaseRequestOptions Options;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseCustomRouteResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    int32 HttpStatus = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    FString ContentType;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    FString RequestId;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    TArray<uint8> Body;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    bool bHasJson = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    EOpenPocketBaseJsonRootType JsonRootType = EOpenPocketBaseJsonRootType::None;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    FJsonObjectWrapper JsonBody;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    double DurationSeconds = 0.0;

    const TSharedPtr<FJsonValue>& GetParsedJson() const
    {
        return ParsedJson;
    }

private:
    TSharedPtr<FJsonValue> ParsedJson;

    friend class FOpenPocketBaseClient;
};

namespace OpenPocketBase::DynamicRoute
{
OPENPOCKETBASESDK_API FOpenPocketBaseCustomRouteRequest NoBody(
    EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    bool bUseAuth = false,
    TMap<FString, FString> Query = {},
    FOpenPocketBaseRequestOptions Options = {});

OPENPOCKETBASESDK_API FOpenPocketBaseCustomRouteRequest Json(
    EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    FJsonObjectWrapper Body,
    bool bUseAuth = false,
    TMap<FString, FString> Query = {},
    FOpenPocketBaseRequestOptions Options = {});

OPENPOCKETBASESDK_API FOpenPocketBaseCustomRouteRequest Form(
    EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    TMap<FString, FString> Fields,
    bool bUseAuth = false,
    TMap<FString, FString> Query = {},
    FOpenPocketBaseRequestOptions Options = {});

OPENPOCKETBASESDK_API FOpenPocketBaseCustomRouteRequest Multipart(
    EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    TMap<FString, FString> Fields,
    TArray<FOpenPocketBaseFileInput> Files,
    bool bUseAuth = false,
    TMap<FString, FString> Query = {},
    FOpenPocketBaseUploadLimits UploadLimits = {},
    int64 MaxRequestBytes = 8 * 1024 * 1024,
    FOpenPocketBaseRequestOptions Options = {});

OPENPOCKETBASESDK_API FOpenPocketBaseCustomRouteRequest Text(
    EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    const FString& Body,
    FString ContentType = TEXT("text/plain; charset=utf-8"),
    bool bUseAuth = false,
    TMap<FString, FString> Query = {},
    FOpenPocketBaseRequestOptions Options = {});

OPENPOCKETBASESDK_API FOpenPocketBaseCustomRouteRequest Binary(
    EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    TArray<uint8> Body,
    FString ContentType = TEXT("application/octet-stream"),
    bool bUseAuth = false,
    TMap<FString, FString> Query = {},
    FOpenPocketBaseRequestOptions Options = {});
}
