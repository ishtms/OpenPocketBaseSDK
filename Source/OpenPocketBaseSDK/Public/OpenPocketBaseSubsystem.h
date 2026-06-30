#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseBlueprintClient.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "OpenPocketBaseSubsystem.generated.h"

UCLASS()
class OPENPOCKETBASESDK_API UOpenPocketBaseSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Client", meta = (DisplayName = "Create PocketBase Client"))
    UOpenPocketBaseClient* CreateClient(
        FName ClientName,
        const FOpenPocketBaseClientConfig& Config,
        FOpenPocketBaseError& OutError);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Client", meta = (DisplayName = "Get PocketBase Client"))
    UOpenPocketBaseClient* GetClient(FName ClientName) const;

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Client")
    UOpenPocketBaseClient* GetDefaultClient() const;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Client")
    bool RemoveClient(FName ClientName);

    virtual void Deinitialize() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UOpenPocketBaseClient> DefaultClient;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UOpenPocketBaseClient>> NamedClients;
};
