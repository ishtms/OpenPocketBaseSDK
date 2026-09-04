// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

#include "OpenPocketBaseJsonValue.generated.h"

UENUM(BlueprintType)
enum class EOpenPocketBaseJsonValueType : uint8
{
    Invalid,
    Null,
    Object,
    Array,
    String,
    Number,
    Boolean
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseJsonValue
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|JSON")
    EOpenPocketBaseJsonValueType Type = EOpenPocketBaseJsonValueType::Invalid;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|JSON")
    FString Json;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|JSON")
    FString ErrorMessage;

    bool IsValid() const;
    TSharedPtr<FJsonValue> ToJsonValue() const;

    static FOpenPocketBaseJsonValue FromJsonValue(const TSharedPtr<FJsonValue>& Value);
    static FOpenPocketBaseJsonValue Invalid(FString Message);
};
