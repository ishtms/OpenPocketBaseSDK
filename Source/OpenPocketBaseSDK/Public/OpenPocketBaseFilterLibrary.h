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
    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records|Filters")
    static bool AddStringParameter(
        UPARAM(ref) FOpenPocketBaseFilterParams& Params,
        const FString& Name,
        const FString& Value);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records|Filters")
    static bool AddNumberParameter(
        UPARAM(ref) FOpenPocketBaseFilterParams& Params,
        const FString& Name,
        double Value);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records|Filters")
    static bool AddBooleanParameter(
        UPARAM(ref) FOpenPocketBaseFilterParams& Params,
        const FString& Name,
        bool bValue);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records|Filters")
    static bool AddDateParameter(
        UPARAM(ref) FOpenPocketBaseFilterParams& Params,
        const FString& Name,
        FDateTime Value);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records|Filters")
    static bool AddNullParameter(
        UPARAM(ref) FOpenPocketBaseFilterParams& Params,
        const FString& Name);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records|Filters")
    static bool AddStringArrayParameter(
        UPARAM(ref) FOpenPocketBaseFilterParams& Params,
        const FString& Name,
        const TArray<FString>& Value);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records|Filters")
    static bool AddNumberArrayParameter(
        UPARAM(ref) FOpenPocketBaseFilterParams& Params,
        const FString& Name,
        const TArray<double>& Value);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records|Filters")
    static bool AddBooleanArrayParameter(
        UPARAM(ref) FOpenPocketBaseFilterParams& Params,
        const FString& Name,
        const TArray<bool>& Value);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records|Filters")
    static void ClearParameters(UPARAM(ref) FOpenPocketBaseFilterParams& Params);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Filters")
    static bool BindFilter(
        const FString& Expression,
        const FOpenPocketBaseFilterParams& Params,
        FString& OutFilter,
        FOpenPocketBaseError& OutError);
};
