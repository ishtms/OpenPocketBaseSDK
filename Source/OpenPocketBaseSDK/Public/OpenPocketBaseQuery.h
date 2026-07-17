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

namespace OpenPocketBase::Query
{
OPENPOCKETBASESDK_API FOpenPocketBaseSort Sort(
    const FOpenPocketBaseAnyFieldRef& Field,
    EOpenPocketBaseSortDirection Direction);
OPENPOCKETBASESDK_API FOpenPocketBaseExpand Expand(
    const FOpenPocketBaseRelationFieldRef& Relation);
OPENPOCKETBASESDK_API FOpenPocketBaseExpand ThenExpand(
    FOpenPocketBaseExpand Path,
    const FOpenPocketBaseRelationFieldRef& Relation);
}
