// Copyright 2026 Ishtmeet Singh.

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
    UOpenPocketBaseClient* CreateClient(
        FName ClientName,
        const FOpenPocketBaseClientConfig& Config,
        FOpenPocketBaseError& OutError);

    UOpenPocketBaseClient* GetClient(FName ClientName) const;

    UOpenPocketBaseClient* GetDefaultClient() const;

    bool RemoveClient(FName ClientName);

    virtual void Deinitialize() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UOpenPocketBaseClient> DefaultClient;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UOpenPocketBaseClient>> NamedClients;
};
