#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseAdminTypes.h"

#include "OpenPocketBaseAdminQueryLibrary.generated.h"

namespace OpenPocketBase::AdminQuery
{
OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminCollectionFilter CollectionText(
    EOpenPocketBaseAdminCollectionTextField Field,
    EOpenPocketBaseStringComparison Comparison,
    const FString& Value);
OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminCollectionFilter CollectionType(
    EOpenPocketBaseStringComparison Comparison,
    EOpenPocketBaseCollectionType Value);
OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminCollectionFilter CollectionSystem(
    EOpenPocketBaseBooleanComparison Comparison,
    bool bValue);
OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminCollectionFilter CollectionDate(
    EOpenPocketBaseAdminCollectionDateField Field,
    EOpenPocketBaseDateComparison Comparison,
    const FDateTime& Value);
OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminCollectionFilter And(
    const FOpenPocketBaseAdminCollectionFilter& A,
    const FOpenPocketBaseAdminCollectionFilter& B);
OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminCollectionFilter Or(
    const FOpenPocketBaseAdminCollectionFilter& A,
    const FOpenPocketBaseAdminCollectionFilter& B);
OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminCollectionFilter DynamicCollectionFilter(
    FString Expression);
OPENPOCKETBASESDKADMIN_API FString CollectionProjection(
    EOpenPocketBaseAdminCollectionProjectionField Field);

OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminLogFilter LogText(
    EOpenPocketBaseAdminLogTextField Field,
    EOpenPocketBaseStringComparison Comparison,
    const FString& Value);
OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminLogFilter LogLevel(
    EOpenPocketBaseNumberComparison Comparison,
    double Value);
OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminLogFilter LogDate(
    EOpenPocketBaseAdminLogDateField Field,
    EOpenPocketBaseDateComparison Comparison,
    const FDateTime& Value);
OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminLogFilter And(
    const FOpenPocketBaseAdminLogFilter& A,
    const FOpenPocketBaseAdminLogFilter& B);
OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminLogFilter Or(
    const FOpenPocketBaseAdminLogFilter& A,
    const FOpenPocketBaseAdminLogFilter& B);
OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminLogFilter DynamicLogFilter(
    FString Expression);
OPENPOCKETBASESDKADMIN_API FString LogProjection(
    EOpenPocketBaseAdminLogProjectionField Field);
}

UCLASS()
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAdminQueryLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Collections", meta = (
        DisplayName = "Collection List Options", NativeMakeFunc))
    static FOpenPocketBaseAdminCollectionListOptions CollectionListOptions(
        int32 Page = 1,
        int32 PerPage = 30);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Collections", meta = (
        DisplayName = "Where Collections"))
    static FOpenPocketBaseAdminCollectionListOptions WhereCollections(
        FOpenPocketBaseAdminCollectionListOptions Options,
        FOpenPocketBaseAdminCollectionFilter Filter);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Collections", meta = (
        DisplayName = "Then Sort Collections By"))
    static FOpenPocketBaseAdminCollectionListOptions ThenSortCollectionsBy(
        FOpenPocketBaseAdminCollectionListOptions Options,
        EOpenPocketBaseAdminCollectionSortField Field,
        EOpenPocketBaseSortDirection Direction = EOpenPocketBaseSortDirection::Ascending);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Collections", meta = (
        DisplayName = "Select Collection Field"))
    static FOpenPocketBaseAdminCollectionListOptions SelectCollectionField(
        FOpenPocketBaseAdminCollectionListOptions Options,
        EOpenPocketBaseAdminCollectionProjectionField Field);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Collections|Filters", meta = (
        DisplayName = "Collection Text Filter"))
    static FOpenPocketBaseAdminCollectionFilter CollectionTextFilter(
        EOpenPocketBaseAdminCollectionTextField Field,
        EOpenPocketBaseStringComparison Comparison,
        const FString& Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Collections|Filters", meta = (
        DisplayName = "Collection Type Filter"))
    static FOpenPocketBaseAdminCollectionFilter CollectionTypeFilter(
        EOpenPocketBaseStringComparison Comparison,
        EOpenPocketBaseCollectionType Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Collections|Filters", meta = (
        DisplayName = "Collection System Filter"))
    static FOpenPocketBaseAdminCollectionFilter CollectionSystemFilter(
        EOpenPocketBaseBooleanComparison Comparison,
        bool bValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Collections|Filters", meta = (
        DisplayName = "Collection Date Filter"))
    static FOpenPocketBaseAdminCollectionFilter CollectionDateFilter(
        EOpenPocketBaseAdminCollectionDateField Field,
        EOpenPocketBaseDateComparison Comparison,
        FDateTime Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Collections|Filters", meta = (
        DisplayName = "And Collection Filters"))
    static FOpenPocketBaseAdminCollectionFilter AndCollectionFilters(
        FOpenPocketBaseAdminCollectionFilter A,
        FOpenPocketBaseAdminCollectionFilter B);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Collections|Filters", meta = (
        DisplayName = "Or Collection Filters"))
    static FOpenPocketBaseAdminCollectionFilter OrCollectionFilters(
        FOpenPocketBaseAdminCollectionFilter A,
        FOpenPocketBaseAdminCollectionFilter B);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Collections|Advanced", meta = (
        DisplayName = "Dynamic Collection Filter (Advanced)"))
    static FOpenPocketBaseAdminCollectionFilter DynamicCollectionFilter(
        const FString& Expression);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Logs", meta = (
        DisplayName = "Log List Options", NativeMakeFunc))
    static FOpenPocketBaseAdminLogListOptions LogListOptions(
        int32 Page = 1,
        int32 PerPage = 30);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Logs", meta = (
        DisplayName = "Where Logs"))
    static FOpenPocketBaseAdminLogListOptions WhereLogs(
        FOpenPocketBaseAdminLogListOptions Options,
        FOpenPocketBaseAdminLogFilter Filter);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Logs", meta = (
        DisplayName = "Then Sort Logs By"))
    static FOpenPocketBaseAdminLogListOptions ThenSortLogsBy(
        FOpenPocketBaseAdminLogListOptions Options,
        EOpenPocketBaseAdminLogSortField Field,
        EOpenPocketBaseSortDirection Direction = EOpenPocketBaseSortDirection::Ascending);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Logs|Advanced", meta = (
        DisplayName = "Then Sort Logs By Dynamic Data Field (Advanced)"))
    static FOpenPocketBaseAdminLogListOptions ThenSortLogsByDynamicDataField(
        FOpenPocketBaseAdminLogListOptions Options,
        const FString& DataField,
        EOpenPocketBaseSortDirection Direction = EOpenPocketBaseSortDirection::Ascending);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Logs", meta = (
        DisplayName = "Select Log Field"))
    static FOpenPocketBaseAdminLogListOptions SelectLogField(
        FOpenPocketBaseAdminLogListOptions Options,
        EOpenPocketBaseAdminLogProjectionField Field);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Logs|Filters", meta = (
        DisplayName = "Log Text Filter"))
    static FOpenPocketBaseAdminLogFilter LogTextFilter(
        EOpenPocketBaseAdminLogTextField Field,
        EOpenPocketBaseStringComparison Comparison,
        const FString& Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Logs|Filters", meta = (
        DisplayName = "Log Level Filter"))
    static FOpenPocketBaseAdminLogFilter LogLevelFilter(
        EOpenPocketBaseNumberComparison Comparison,
        double Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Logs|Filters", meta = (
        DisplayName = "Log Date Filter"))
    static FOpenPocketBaseAdminLogFilter LogDateFilter(
        EOpenPocketBaseAdminLogDateField Field,
        EOpenPocketBaseDateComparison Comparison,
        FDateTime Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Logs|Filters", meta = (
        DisplayName = "And Log Filters"))
    static FOpenPocketBaseAdminLogFilter AndLogFilters(
        FOpenPocketBaseAdminLogFilter A,
        FOpenPocketBaseAdminLogFilter B);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Logs|Filters", meta = (
        DisplayName = "Or Log Filters"))
    static FOpenPocketBaseAdminLogFilter OrLogFilters(
        FOpenPocketBaseAdminLogFilter A,
        FOpenPocketBaseAdminLogFilter B);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin|Logs|Advanced", meta = (
        DisplayName = "Dynamic Log Filter (Advanced)"))
    static FOpenPocketBaseAdminLogFilter DynamicLogFilter(const FString& Expression);
};
