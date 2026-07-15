#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseBlueprintClient.h"

#include "OpenPocketBaseClientLibrary.generated.h"

UCLASS()
class OPENPOCKETBASESDK_API UOpenPocketBaseClientLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Client",
        meta = (
            DisplayName = "Initialize PocketBase",
            WorldContext = "WorldContextObject",
            DefaultToSelf = "WorldContextObject",
            HidePin = "WorldContextObject",
            ExpandBoolAsExecs = "ReturnValue",
            ReturnDisplayName = "Succeeded"))
    static bool InitializePocketBase(
        const UObject* WorldContextObject,
        const FString& BaseUrl,
        UOpenPocketBaseClient*& Client,
        FOpenPocketBaseError& Error);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Client|Advanced",
        meta = (
            DisplayName = "Initialize PocketBase with Config",
            WorldContext = "WorldContextObject",
            DefaultToSelf = "WorldContextObject",
            HidePin = "WorldContextObject",
            ExpandBoolAsExecs = "ReturnValue",
            ReturnDisplayName = "Succeeded"))
    static bool InitializePocketBaseWithConfig(
        const UObject* WorldContextObject,
        const FOpenPocketBaseClientConfig& Config,
        UOpenPocketBaseClient*& Client,
        FOpenPocketBaseError& Error);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Client",
        meta = (
            DisplayName = "Get PocketBase Client",
            WorldContext = "WorldContextObject",
            DefaultToSelf = "WorldContextObject",
            HidePin = "WorldContextObject"))
    static UOpenPocketBaseClient* GetPocketBaseClient(const UObject* WorldContextObject);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Client|Advanced",
        meta = (
            DisplayName = "Create Named PocketBase Client",
            WorldContext = "WorldContextObject",
            DefaultToSelf = "WorldContextObject",
            HidePin = "WorldContextObject",
            ExpandBoolAsExecs = "ReturnValue",
            ReturnDisplayName = "Succeeded"))
    static bool CreateNamedPocketBaseClient(
        const UObject* WorldContextObject,
        FName ClientName,
        const FOpenPocketBaseClientConfig& Config,
        UOpenPocketBaseClient*& Client,
        FOpenPocketBaseError& Error);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Client|Advanced",
        meta = (
            DisplayName = "Get Named PocketBase Client",
            WorldContext = "WorldContextObject",
            DefaultToSelf = "WorldContextObject",
            HidePin = "WorldContextObject"))
    static UOpenPocketBaseClient* GetNamedPocketBaseClient(
        const UObject* WorldContextObject,
        FName ClientName);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Client",
        meta = (
            DisplayName = "Shutdown PocketBase",
            WorldContext = "WorldContextObject",
            DefaultToSelf = "WorldContextObject",
            HidePin = "WorldContextObject",
            ReturnDisplayName = "Shut Down"))
    static bool ShutdownPocketBase(const UObject* WorldContextObject);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Client|Advanced",
        meta = (
            DisplayName = "Remove Named PocketBase Client",
            WorldContext = "WorldContextObject",
            DefaultToSelf = "WorldContextObject",
            HidePin = "WorldContextObject",
            ReturnDisplayName = "Removed"))
    static bool RemoveNamedPocketBaseClient(
        const UObject* WorldContextObject,
        FName ClientName);
};
