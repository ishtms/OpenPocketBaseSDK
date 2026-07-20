#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseFilter.h"

#include "OpenPocketBaseFilterLibrary.generated.h"

UCLASS()
class OPENPOCKETBASESDK_API UOpenPocketBaseFilterLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Filters",
        meta = (DisplayName = "String Filter", Keywords = "where query equals contains"))
    static FOpenPocketBaseFilter StringFilter(
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Read")) FOpenPocketBaseStringFieldRef Field,
        EOpenPocketBaseStringComparison Comparison,
        const FString& Value);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Filters",
        meta = (DisplayName = "String Array Filter", Keywords = "where query list select relation contains"))
    static FOpenPocketBaseFilter StringArrayFilter(
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Read")) FOpenPocketBaseStringArrayFieldRef Field,
        EOpenPocketBaseStringComparison Comparison,
        const FString& Value);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Filters",
        meta = (DisplayName = "Number Filter", Keywords = "where query compare"))
    static FOpenPocketBaseFilter NumberFilter(
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Read")) FOpenPocketBaseNumberFieldRef Field,
        EOpenPocketBaseNumberComparison Comparison,
        double Value);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Filters",
        meta = (DisplayName = "Boolean Filter", ToolTip = "Creates a type-safe PocketBase filter for a Boolean field.", Keywords = "pocketbase where query bool true false filter"))
    static FOpenPocketBaseFilter BooleanFilter(
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Read")) FOpenPocketBaseBooleanFieldRef Field,
        EOpenPocketBaseBooleanComparison Comparison,
        bool bValue);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Filters",
        meta = (DisplayName = "Date Filter", Keywords = "where query time compare"))
    static FOpenPocketBaseFilter DateFilter(
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Read")) FOpenPocketBaseDateFieldRef Field,
        EOpenPocketBaseDateComparison Comparison,
        FDateTime Value);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Filters",
        meta = (DisplayName = "Null Filter", Keywords = "where query empty null"))
    static FOpenPocketBaseFilter NullFilter(
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Read")) FOpenPocketBaseAnyFieldRef Field,
        EOpenPocketBaseNullComparison Comparison = EOpenPocketBaseNullComparison::IsNull);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Filters|Relations", meta = (DisplayName = "Related String Filter"))
    static FOpenPocketBaseFilter RelatedStringFilter(
        FOpenPocketBaseExpand Relations,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Read", OpenPocketBaseRelationTarget = "Relations")) FOpenPocketBaseStringFieldRef Field,
        EOpenPocketBaseStringComparison Comparison,
        const FString& Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Filters|Relations", meta = (DisplayName = "Related Number Filter"))
    static FOpenPocketBaseFilter RelatedNumberFilter(
        FOpenPocketBaseExpand Relations,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Read", OpenPocketBaseRelationTarget = "Relations")) FOpenPocketBaseNumberFieldRef Field,
        EOpenPocketBaseNumberComparison Comparison,
        double Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Filters|Relations", meta = (DisplayName = "Related Boolean Filter"))
    static FOpenPocketBaseFilter RelatedBooleanFilter(
        FOpenPocketBaseExpand Relations,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Read", OpenPocketBaseRelationTarget = "Relations")) FOpenPocketBaseBooleanFieldRef Field,
        EOpenPocketBaseBooleanComparison Comparison,
        bool bValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Filters|Relations", meta = (DisplayName = "Related Date Filter"))
    static FOpenPocketBaseFilter RelatedDateFilter(
        FOpenPocketBaseExpand Relations,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Read", OpenPocketBaseRelationTarget = "Relations")) FOpenPocketBaseDateFieldRef Field,
        EOpenPocketBaseDateComparison Comparison,
        FDateTime Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Filters|Relations", meta = (DisplayName = "Related Null Filter"))
    static FOpenPocketBaseFilter RelatedNullFilter(
        FOpenPocketBaseExpand Relations,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Read", OpenPocketBaseRelationTarget = "Relations")) FOpenPocketBaseAnyFieldRef Field,
        EOpenPocketBaseNullComparison Comparison = EOpenPocketBaseNullComparison::IsNull);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Filters",
        meta = (DisplayName = "And Filters", ToolTip = "Requires both PocketBase filters to match.", Keywords = "pocketbase where query combine and filter"))
    static FOpenPocketBaseFilter AndFilters(
        const FOpenPocketBaseFilter& A,
        const FOpenPocketBaseFilter& B);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Filters",
        meta = (DisplayName = "Or Filters", Keywords = "where query combine"))
    static FOpenPocketBaseFilter OrFilters(
        const FOpenPocketBaseFilter& A,
        const FOpenPocketBaseFilter& B);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Filters|Advanced",
        meta = (DisplayName = "Dynamic Filter (Advanced)", Keywords = "raw where query expression"))
    static FOpenPocketBaseFilter DynamicFilter(const FString& Expression);
};
