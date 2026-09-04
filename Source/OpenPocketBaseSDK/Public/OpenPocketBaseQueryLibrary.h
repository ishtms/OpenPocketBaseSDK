// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseQuery.h"

#include "OpenPocketBaseQueryLibrary.generated.h"

UCLASS()
class OPENPOCKETBASESDK_API UOpenPocketBaseQueryLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Query",
        meta = (DisplayName = "Sort Ascending", NativeMakeFunc))
    static FOpenPocketBaseSort SortAscending(FOpenPocketBaseAnyFieldRef Field);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Query",
        meta = (DisplayName = "Sort Descending", NativeMakeFunc, ToolTip = "Sorts records from highest to lowest by the selected field.", Keywords = "pocketbase records query sort descending order"))
    static FOpenPocketBaseSort SortDescending(FOpenPocketBaseAnyFieldRef Field);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Query",
        meta = (DisplayName = "Expand Relation", NativeMakeFunc))
    static FOpenPocketBaseExpand ExpandRelation(FOpenPocketBaseRelationFieldRef Relation);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Query",
        meta = (DisplayName = "Then Expand Relation"))
    static FOpenPocketBaseExpand ThenExpandRelation(
        FOpenPocketBaseExpand Expand,
        UPARAM(meta = (OpenPocketBaseRelationTarget = "Expand")) FOpenPocketBaseRelationFieldRef Relation);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Query",
        meta = (DisplayName = "Select Field", NativeMakeFunc, ToolTip = "Requests only the selected field in each returned record.", Keywords = "pocketbase records query fields select projection"))
    static FOpenPocketBaseFieldSelection SelectField(FOpenPocketBaseAnyFieldRef Field);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Query",
        meta = (DisplayName = "Select Text Excerpt", NativeMakeFunc))
    static FOpenPocketBaseFieldSelection SelectTextExcerpt(
        FOpenPocketBaseStringFieldRef Field,
        int32 MaxLength,
        bool bWithEllipsis = false);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Query",
        meta = (DisplayName = "Select Expanded Field", NativeMakeFunc))
    static FOpenPocketBaseFieldSelection SelectExpandedField(
        FOpenPocketBaseExpand Expand,
        UPARAM(meta = (OpenPocketBaseRelationTarget = "Expand")) FOpenPocketBaseAnyFieldRef Field);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Query",
        meta = (DisplayName = "Select Expanded Text Excerpt", NativeMakeFunc))
    static FOpenPocketBaseFieldSelection SelectExpandedTextExcerpt(
        FOpenPocketBaseExpand Expand,
        UPARAM(meta = (OpenPocketBaseRelationTarget = "Expand")) FOpenPocketBaseStringFieldRef Field,
        int32 MaxLength,
        bool bWithEllipsis = false);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Query",
        meta = (DisplayName = "Select Expanded Record", NativeMakeFunc))
    static FOpenPocketBaseFieldSelection SelectExpandedRecord(FOpenPocketBaseExpand Expand);
};
