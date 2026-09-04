// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseSchema.h"

#include "OpenPocketBaseQuery.generated.h"

UENUM(BlueprintType)
enum class EOpenPocketBaseSortDirection : uint8
{
    Ascending,
    Descending
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseSort
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Query")
    FOpenPocketBaseAnyFieldRef Field;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Query")
    EOpenPocketBaseSortDirection Direction = EOpenPocketBaseSortDirection::Ascending;

    bool IsSet() const;
    bool BelongsTo(const FOpenPocketBaseCollectionRef& Collection) const;
    FString ToQueryValue() const;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseExpand
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Query")
    TArray<FOpenPocketBaseRelationFieldRef> Path;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Query")
    bool bValid = true;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Query")
    FString ErrorMessage;

    bool IsSet() const;
    bool BelongsTo(const FOpenPocketBaseCollectionRef& Collection) const;
    FString ToQueryValue() const;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseFieldSelection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Query")
    FOpenPocketBaseFieldRef Field;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Query")
    FOpenPocketBaseExpand Expand;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Query")
    bool bAllExpandedFields = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Query")
    bool bExcerpt = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Query")
    int32 ExcerptMaxLength = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Query")
    bool bExcerptWithEllipsis = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Query")
    bool bValid = true;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Query")
    FString ErrorMessage;

    bool IsSet() const;
    bool BelongsTo(const FOpenPocketBaseCollectionRef& Collection) const;
    FString ToQueryValue() const;
};

namespace OpenPocketBase::Query
{
OPENPOCKETBASESDK_API FOpenPocketBaseSort Sort(
    const FOpenPocketBaseFieldRef& Field,
    EOpenPocketBaseSortDirection Direction);
OPENPOCKETBASESDK_API FOpenPocketBaseExpand Expand(
    const FOpenPocketBaseRelationFieldRef& Relation);
OPENPOCKETBASESDK_API FOpenPocketBaseExpand ThenExpand(
    FOpenPocketBaseExpand Path,
    const FOpenPocketBaseRelationFieldRef& Relation);
OPENPOCKETBASESDK_API FOpenPocketBaseFieldSelection Select(
    const FOpenPocketBaseFieldRef& Field);
OPENPOCKETBASESDK_API FOpenPocketBaseFieldSelection SelectExcerpt(
    const FOpenPocketBaseStringFieldRef& Field,
    int32 MaxLength,
    bool bWithEllipsis = false);
OPENPOCKETBASESDK_API FOpenPocketBaseFieldSelection SelectExpanded(
    FOpenPocketBaseExpand Path,
    const FOpenPocketBaseFieldRef& Field);
OPENPOCKETBASESDK_API FOpenPocketBaseFieldSelection SelectExpandedExcerpt(
    FOpenPocketBaseExpand Path,
    const FOpenPocketBaseStringFieldRef& Field,
    int32 MaxLength,
    bool bWithEllipsis = false);
OPENPOCKETBASESDK_API FOpenPocketBaseFieldSelection SelectExpandedRecord(
    FOpenPocketBaseExpand Path);
}
