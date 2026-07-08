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
            DisplayName = "Create Privileged PocketBase Client",
            DevelopmentOnly))
    static UOpenPocketBaseAdminClient* CreateAdminClient(
        const UObject* WorldContextObject,
        FOpenPocketBaseClientConfig CoreConfig,
        FOpenPocketBaseAdminPolicy Policy,
        FOpenPocketBaseError& OutError);

    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> GetNativeClient() const;

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin", meta = (DevelopmentOnly))
    bool IsReady() const;

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Admin", meta = (DevelopmentOnly))
    bool IsAuthenticated() const;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (DevelopmentOnly))
    void Logout();

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Admin", meta = (DevelopmentOnly))
    void Shutdown();

    virtual void BeginDestroy() override;

private:
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> NativeClient;
};
