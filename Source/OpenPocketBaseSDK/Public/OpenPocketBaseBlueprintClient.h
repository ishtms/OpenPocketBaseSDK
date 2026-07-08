#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseSubscription.h"

#include "OpenPocketBaseBlueprintClient.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseBlueprintSessionChanged,
    FOpenPocketBaseSessionSnapshot,
    Session);

UCLASS(BlueprintType)
class OPENPOCKETBASESDK_API UOpenPocketBaseClient final : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Open PocketBase|Session")
    FOpenPocketBaseBlueprintSessionChanged SessionChanged;

    static UOpenPocketBaseClient* Create(
        UObject* Outer,
        const FOpenPocketBaseClientConfig& Config,
        FOpenPocketBaseError& OutError);

    static UOpenPocketBaseClient* Create(
        UObject* Outer,
        const FOpenPocketBaseClientConfig& Config,
        TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
        FOpenPocketBaseError& OutError);

    static UOpenPocketBaseClient* Wrap(
        UObject* Outer,
        TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client);

    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> GetNativeClient() const;

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Client", meta = (DisplayName = "Is PocketBase Client Ready"))
    bool IsReady() const;

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Client")
    FString GetBaseUrl() const;

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Utilities")
    FOpenPocketBaseCapabilityInfo GetCapability(EOpenPocketBaseCapability Capability) const;

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Utilities")
    FOpenPocketBaseCapabilityReport GetCapabilityReport() const;

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Authentication")
    bool IsAuthenticated() const;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Authentication")
    bool GetCurrentAuthRecord(FOpenPocketBaseRecord& OutRecord) const;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Session")
    bool GetCurrentSession(FOpenPocketBaseSessionSnapshot& OutSession) const;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Session")
    void Logout();

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Client")
    void Shutdown();

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Realtime",
        meta = (DisplayName = "Subscribe to Records", AutoCreateRefTerm = "Options"))
    UOpenPocketBaseSubscription* SubscribeToRecords(
        FString Collection,
        const FOpenPocketBaseRealtimeOptions& Options,
        FOpenPocketBaseError& OutError);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Realtime",
        meta = (DisplayName = "Subscribe to Record", AutoCreateRefTerm = "Options"))
    UOpenPocketBaseSubscription* SubscribeToRecord(
        FString Collection,
        FString RecordId,
        const FOpenPocketBaseRealtimeOptions& Options,
        FOpenPocketBaseError& OutError);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Realtime",
        meta = (DisplayName = "Subscribe to Realtime Topic", AutoCreateRefTerm = "Options"))
    UOpenPocketBaseSubscription* SubscribeToTopic(
        FString Topic,
        const FOpenPocketBaseRealtimeOptions& Options,
        FOpenPocketBaseError& OutError);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Realtime")
    void UnsubscribeAllRealtime();

    virtual void BeginDestroy() override;

private:
    void BindNativeSessionEvents();
    void HandleNativeSessionChanged(const FOpenPocketBaseSessionSnapshot& Session);
    FOpenPocketBaseRealtimeCallbacks MakeRealtimeCallbacks(
        UOpenPocketBaseSubscription* Subscription) const;
    void RetainSubscription(UOpenPocketBaseSubscription* Subscription);
    void ReleaseSubscription(UOpenPocketBaseSubscription* Subscription);

    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient;
    FDelegateHandle SessionChangedHandle;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UOpenPocketBaseSubscription>> ActiveSubscriptions;

    friend class UOpenPocketBaseSubscription;
};
