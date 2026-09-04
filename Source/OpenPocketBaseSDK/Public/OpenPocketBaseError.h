// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"

#include "OpenPocketBaseError.generated.h"

UENUM(BlueprintType)
enum class EOpenPocketBaseErrorKind : uint8
{
    None,
    Cancelled,
    InvalidArgument,
    Transport,
    Timeout,
    Http,
    PocketBase,
    Serialization,
    Authentication,
    SecureStorage,
    OfflineQueue,
    Unsupported,
    Internal
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseFieldError
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase")
    FString Code;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase")
    FString Message;
};

USTRUCT(
    BlueprintType,
    meta = (HasNativeBreak = "/Script/OpenPocketBaseSDK.OpenPocketBaseClientLibrary.BreakError"))
struct OPENPOCKETBASESDK_API FOpenPocketBaseError
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase")
    EOpenPocketBaseErrorKind Kind = EOpenPocketBaseErrorKind::None;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase")
    int32 HttpStatus = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase")
    FString Code;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase")
    FString Message;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase")
    TMap<FString, FOpenPocketBaseFieldError> FieldErrors;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase")
    bool bMayRetry = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase")
    FString RequestId;

    bool IsSet() const
    {
        return Kind != EOpenPocketBaseErrorKind::None;
    }
};
