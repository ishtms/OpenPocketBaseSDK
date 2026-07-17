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
        meta = (DisplayName = "Number Filter", Keywords = "where query compare"))
    static FOpenPocketBaseFilter NumberFilter(
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Read")) FOpenPocketBaseNumberFieldRef Field,
        EOpenPocketBaseNumberComparison Comparison,
        double Value);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Filters",
        meta = (DisplayName = "Boolean Filter", Keywords = "where query bool true false"))
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

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Filters",
        meta = (DisplayName = "And Filters", Keywords = "where query combine"))
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
        meta = (DisplayName = "Raw Filter (Advanced)", Keywords = "where query expression"))
    static FOpenPocketBaseFilter RawFilter(const FString& Expression);
};
