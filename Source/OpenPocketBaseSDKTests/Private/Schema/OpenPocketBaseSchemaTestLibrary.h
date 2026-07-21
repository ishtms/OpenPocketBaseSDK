#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseSchema.h"

#include "OpenPocketBaseSchemaTestLibrary.generated.h"

UCLASS()
class UOpenPocketBaseSchemaTestLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Tests")
    static void UseSchemaReferences(
        FOpenPocketBaseCollectionRef Collection,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseBooleanFieldRef Field);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Tests")
    static FOpenPocketBaseSingleSelectFieldRef PassThroughSingleSelectField(
        FOpenPocketBaseSingleSelectFieldRef Field);
};
