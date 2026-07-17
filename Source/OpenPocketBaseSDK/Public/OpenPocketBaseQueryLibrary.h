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
        meta = (DisplayName = "Sort Descending", NativeMakeFunc))
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
        FOpenPocketBaseRelationFieldRef Relation);
};
