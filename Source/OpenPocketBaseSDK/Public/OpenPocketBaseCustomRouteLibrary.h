// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseCustomRoute.h"

#include "OpenPocketBaseCustomRouteLibrary.generated.h"

UCLASS()
class OPENPOCKETBASESDK_API UOpenPocketBaseCustomRouteLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Utilities|Advanced", meta = (
        DisplayName = "Dynamic Route without Body (Advanced)",
        AutoCreateRefTerm = "Query"))
    static FOpenPocketBaseCustomRouteRequest NoBodyRoute(
        EOpenPocketBaseCustomRouteMethod Method,
        FString Path,
        bool bUseAuth,
        TMap<FString, FString> Query,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Utilities|Advanced", meta = (
        DisplayName = "Dynamic JSON Route (Advanced)",
        AutoCreateRefTerm = "Query"))
    static FOpenPocketBaseCustomRouteRequest JsonRoute(
        EOpenPocketBaseCustomRouteMethod Method,
        FString Path,
        FJsonObjectWrapper Body,
        bool bUseAuth,
        TMap<FString, FString> Query,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Utilities|Advanced", meta = (
        DisplayName = "Dynamic Form Route (Advanced)",
        AutoCreateRefTerm = "Fields,Query"))
    static FOpenPocketBaseCustomRouteRequest FormRoute(
        EOpenPocketBaseCustomRouteMethod Method,
        FString Path,
        TMap<FString, FString> Fields,
        bool bUseAuth,
        TMap<FString, FString> Query,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Utilities|Advanced", meta = (
        DisplayName = "Dynamic Multipart Route (Advanced)",
        AutoCreateRefTerm = "Fields,Files,Query"))
    static FOpenPocketBaseCustomRouteRequest MultipartRoute(
        EOpenPocketBaseCustomRouteMethod Method,
        FString Path,
        TMap<FString, FString> Fields,
        TArray<FOpenPocketBaseFileInput> Files,
        bool bUseAuth,
        TMap<FString, FString> Query,
        FOpenPocketBaseUploadLimits UploadLimits,
        int64 MaxRequestBytes,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Utilities|Advanced", meta = (
        DisplayName = "Dynamic Text Route (Advanced)",
        AutoCreateRefTerm = "Query"))
    static FOpenPocketBaseCustomRouteRequest TextRoute(
        EOpenPocketBaseCustomRouteMethod Method,
        FString Path,
        FString Body,
        FString ContentType,
        bool bUseAuth,
        TMap<FString, FString> Query,
        FOpenPocketBaseRequestOptions Options);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Utilities|Advanced", meta = (
        DisplayName = "Dynamic Binary Route (Advanced)",
        AutoCreateRefTerm = "Body,Query"))
    static FOpenPocketBaseCustomRouteRequest BinaryRoute(
        EOpenPocketBaseCustomRouteMethod Method,
        FString Path,
        TArray<uint8> Body,
        FString ContentType,
        bool bUseAuth,
        TMap<FString, FString> Query,
        FOpenPocketBaseRequestOptions Options);
};
