// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseAdminClient.h"

#include "OpenPocketBaseAdminBlueprintClient.generated.h"

UCLASS(BlueprintType)
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAdminClient final : public UObject
{
    GENERATED_BODY()

public:
    static UOpenPocketBaseAdminClient* Create(
        UObject* Outer,
        const FOpenPocketBaseClientConfig& CoreConfig,
        const FOpenPocketBaseAdminPolicy& Policy,
        FOpenPocketBaseError& OutError);

    static UOpenPocketBaseAdminClient* Create(
        UObject* Outer,
        const FOpenPocketBaseClientConfig& CoreConfig,
        const FOpenPocketBaseAdminPolicy& Policy,
        TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
        FOpenPocketBaseError& OutError);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Admin",
        meta = (
            WorldContext = "WorldContextObject",
            DefaultToSelf = "WorldContextObject",
            HidePin = "WorldContextObject",
            DisplayName = "Initialize Privileged PocketBase",
            ExpandBoolAsExecs = "ReturnValue",
            ReturnDisplayName = "Succeeded",
            DevelopmentOnly))
    static bool InitializeAdminClient(
        const UObject* WorldContextObject,
        FOpenPocketBaseClientConfig CoreConfig,
        FOpenPocketBaseAdminPolicy Policy,
        UOpenPocketBaseAdminClient*& Client,
        FOpenPocketBaseError& Error);

    virtual UWorld* GetWorld() const override;

    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> GetNativeClient() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Admin",
        meta = (
            DevelopmentOnly,
            ReturnDisplayName = "Ready"))
    bool IsReady() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Admin",
        meta = (
            DevelopmentOnly,
            ReturnDisplayName = "Authenticated"))
    bool IsAuthenticated() const;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (DevelopmentOnly))
    void Logout();

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (DevelopmentOnly))
    void Shutdown();

    virtual void BeginDestroy() override;

private:
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> NativeClient;
};
