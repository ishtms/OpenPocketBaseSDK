#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseClient.h"

#include "OpenPocketBaseBlueprintClient.generated.h"

UCLASS(BlueprintType)
class OPENPOCKETBASESDK_API UOpenPocketBaseClient final : public UObject
{
    GENERATED_BODY()

public:
    static UOpenPocketBaseClient* Create(
        UObject* Outer,
        const FOpenPocketBaseClientConfig& Config,
        FOpenPocketBaseError& OutError);

    static UOpenPocketBaseClient* Create(
        UObject* Outer,
        const FOpenPocketBaseClientConfig& Config,
        TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
        FOpenPocketBaseError& OutError);

    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> GetNativeClient() const;

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Client", meta = (DisplayName = "Is PocketBase Client Ready"))
    bool IsReady() const;

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Client")
    FString GetBaseUrl() const;

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Authentication")
    bool IsAuthenticated() const;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Authentication")
    bool GetCurrentAuthRecord(FOpenPocketBaseRecord& OutRecord) const;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Client")
    void Shutdown();

    virtual void BeginDestroy() override;

private:
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient;
};
