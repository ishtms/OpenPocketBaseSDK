#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseBlueprintClient.h"
#include "OpenPocketBaseFile.h"

#include "OpenPocketBaseFileLibrary.generated.h"

UCLASS()
class OPENPOCKETBASESDK_API UOpenPocketBaseFileLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Files",
        meta = (
            DisplayName = "Try Build File URL",
            AutoCreateRefTerm = "Options",
            ExpandBoolAsExecs = "ReturnValue",
            ReturnDisplayName = "Built"))
    static bool TryBuildFileUrl(
        FOpenPocketBaseCollection Collection,
        FString RecordId,
        FString FileName,
        FOpenPocketBaseFileUrlOptions Options,
        FString& Url,
        FOpenPocketBaseError& Error);
};
