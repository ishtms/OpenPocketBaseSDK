// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseError.h"
#include "OpenPocketBaseQuery.h"
#include "OpenPocketBaseSchema.h"

#include "OpenPocketBaseFilter.generated.h"

UENUM(BlueprintType)
enum class EOpenPocketBaseStringComparison : uint8
{
    Equals,
    NotEquals UMETA(DisplayName = "Does Not Equal"),
    Contains,
    DoesNotContain UMETA(DisplayName = "Does Not Contain"),
    AnyEquals UMETA(DisplayName = "Any Equals"),
    AnyNotEquals UMETA(DisplayName = "Any Does Not Equal"),
    AnyContains UMETA(DisplayName = "Any Contains"),
    AnyDoesNotContain UMETA(DisplayName = "Any Does Not Contain")
};

UENUM(BlueprintType)
enum class EOpenPocketBaseNumberComparison : uint8
{
    Equals,
    NotEquals UMETA(DisplayName = "Does Not Equal"),
    GreaterThan,
    GreaterThanOrEqual UMETA(DisplayName = "Greater Than or Equal"),
    LessThan,
    LessThanOrEqual UMETA(DisplayName = "Less Than or Equal"),
    AnyEquals UMETA(DisplayName = "Any Equals"),
    AnyNotEquals UMETA(DisplayName = "Any Does Not Equal"),
    AnyGreaterThan UMETA(DisplayName = "Any Greater Than"),
    AnyGreaterThanOrEqual UMETA(DisplayName = "Any Greater Than or Equal"),
    AnyLessThan UMETA(DisplayName = "Any Less Than"),
    AnyLessThanOrEqual UMETA(DisplayName = "Any Less Than or Equal")
};

UENUM(BlueprintType)
enum class EOpenPocketBaseBooleanComparison : uint8
{
    Equals,
    NotEquals UMETA(DisplayName = "Does Not Equal"),
    AnyEquals UMETA(DisplayName = "Any Equals"),
    AnyNotEquals UMETA(DisplayName = "Any Does Not Equal")
};

UENUM(BlueprintType)
enum class EOpenPocketBaseDateComparison : uint8
{
    Equals,
    NotEquals UMETA(DisplayName = "Does Not Equal"),
    After UMETA(DisplayName = "Is After"),
    OnOrAfter UMETA(DisplayName = "Is On or After"),
    Before UMETA(DisplayName = "Is Before"),
    OnOrBefore UMETA(DisplayName = "Is On or Before"),
    AnyEquals UMETA(DisplayName = "Any Equals"),
    AnyNotEquals UMETA(DisplayName = "Any Does Not Equal"),
    AnyAfter UMETA(DisplayName = "Any Is After"),
    AnyOnOrAfter UMETA(DisplayName = "Any Is On or After"),
    AnyBefore UMETA(DisplayName = "Any Is Before"),
    AnyOnOrBefore UMETA(DisplayName = "Any Is On or Before")
};

UENUM(BlueprintType)
enum class EOpenPocketBaseNullComparison : uint8
{
    IsNull UMETA(DisplayName = "Is Null"),
    IsNotNull UMETA(DisplayName = "Is Not Null"),
    AnyIsNull UMETA(DisplayName = "Any Is Null"),
    AnyIsNotNull UMETA(DisplayName = "Any Is Not Null")
};

class OPENPOCKETBASESDK_API FOpenPocketBaseDynamicFilterParams
{
public:
    bool AddString(const FString& Name, const FString& Value);
    bool AddNumber(const FString& Name, double Value);
    bool AddBoolean(const FString& Name, bool bValue);
    bool AddDate(const FString& Name, const FDateTime& Value);
    bool AddNull(const FString& Name);
    bool AddStringArray(const FString& Name, const TArray<FString>& Value);
    bool AddNumberArray(const FString& Name, const TArray<double>& Value);
    bool AddBooleanArray(const FString& Name, const TArray<bool>& Value);
    int32 Num() const;
    void Reset();

private:
    bool AddEncoded(const FString& Name, FString EncodedValue);

    TMap<FString, FString> EncodedValues;

    friend struct FOpenPocketBaseFilter;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseFilter
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Filters")
    FString Expression;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Filters")
    bool bValid = true;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records|Filters")
    FString ErrorMessage;

    UPROPERTY(Transient)
    FGuid SchemaId;

    UPROPERTY(Transient)
    FString CollectionId;

    static FOpenPocketBaseFilter String(
        const FOpenPocketBaseStringFieldRef& Field,
        EOpenPocketBaseStringComparison Comparison,
        const FString& Value);
    static FOpenPocketBaseFilter StringArray(
        const FOpenPocketBaseStringArrayFieldRef& Field,
        EOpenPocketBaseStringComparison Comparison,
        const FString& Value);
    static FOpenPocketBaseFilter Number(
        const FOpenPocketBaseNumberFieldRef& Field,
        EOpenPocketBaseNumberComparison Comparison,
        double Value);
    static FOpenPocketBaseFilter Boolean(
        const FOpenPocketBaseBooleanFieldRef& Field,
        EOpenPocketBaseBooleanComparison Comparison,
        bool bValue);
    static FOpenPocketBaseFilter Date(
        const FOpenPocketBaseDateFieldRef& Field,
        EOpenPocketBaseDateComparison Comparison,
        const FDateTime& Value);
    static FOpenPocketBaseFilter Null(
        const FOpenPocketBaseFieldRef& Field,
        EOpenPocketBaseNullComparison Comparison = EOpenPocketBaseNullComparison::IsNull);
    static FOpenPocketBaseFilter RelatedString(
        const FOpenPocketBaseExpand& Relations,
        const FOpenPocketBaseStringFieldRef& Field,
        EOpenPocketBaseStringComparison Comparison,
        const FString& Value);
    static FOpenPocketBaseFilter RelatedNumber(
        const FOpenPocketBaseExpand& Relations,
        const FOpenPocketBaseNumberFieldRef& Field,
        EOpenPocketBaseNumberComparison Comparison,
        double Value);
    static FOpenPocketBaseFilter RelatedBoolean(
        const FOpenPocketBaseExpand& Relations,
        const FOpenPocketBaseBooleanFieldRef& Field,
        EOpenPocketBaseBooleanComparison Comparison,
        bool bValue);
    static FOpenPocketBaseFilter RelatedDate(
        const FOpenPocketBaseExpand& Relations,
        const FOpenPocketBaseDateFieldRef& Field,
        EOpenPocketBaseDateComparison Comparison,
        const FDateTime& Value);
    static FOpenPocketBaseFilter RelatedNull(
        const FOpenPocketBaseExpand& Relations,
        const FOpenPocketBaseFieldRef& Field,
        EOpenPocketBaseNullComparison Comparison = EOpenPocketBaseNullComparison::IsNull);
    static FOpenPocketBaseFilter DynamicString(
        FString Field,
        EOpenPocketBaseStringComparison Comparison,
        const FString& Value);
    static FOpenPocketBaseFilter DynamicNumber(
        FString Field,
        EOpenPocketBaseNumberComparison Comparison,
        double Value);
    static FOpenPocketBaseFilter DynamicBoolean(
        FString Field,
        EOpenPocketBaseBooleanComparison Comparison,
        bool bValue);
    static FOpenPocketBaseFilter DynamicDate(
        FString Field,
        EOpenPocketBaseDateComparison Comparison,
        const FDateTime& Value);
    static FOpenPocketBaseFilter DynamicNull(
        FString Field,
        EOpenPocketBaseNullComparison Comparison = EOpenPocketBaseNullComparison::IsNull);
    static FOpenPocketBaseFilter DynamicRaw(FString Expression);

    FOpenPocketBaseFilter And(const FOpenPocketBaseFilter& Other) const;
    FOpenPocketBaseFilter Or(const FOpenPocketBaseFilter& Other) const;
    bool IsEmpty() const;
    bool IsValid() const;
    bool BelongsTo(const FOpenPocketBaseCollectionRef& Collection) const;
    const FString& ToString() const;

    static bool TryBindDynamic(
        const FString& Expression,
        const FOpenPocketBaseDynamicFilterParams& Params,
        FOpenPocketBaseFilter& OutFilter,
        FOpenPocketBaseError& OutError);
};
