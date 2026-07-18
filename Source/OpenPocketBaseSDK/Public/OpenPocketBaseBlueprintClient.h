#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseSubscription.h"

#include "OpenPocketBaseBlueprintClient.generated.h"

class UOpenPocketBaseClient;

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseCollection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    TObjectPtr<UOpenPocketBaseClient> Client;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Records")
    FOpenPocketBaseCollectionRef Reference;

    bool IsValid() const;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseAuthCollection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    TObjectPtr<UOpenPocketBaseClient> Client;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FOpenPocketBaseAuthCollectionRef Reference;

    bool IsValid() const;
};

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

    virtual UWorld* GetWorld() const override;

    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> GetNativeClient() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Client",
        meta = (
            DisplayName = "Is PocketBase Client Ready",
            ReturnDisplayName = "Ready"))
    bool IsReady() const;

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Client")
    FString GetBaseUrl() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records",
        meta = (DisplayName = "Collection", Keywords = "records auth table"))
    FOpenPocketBaseCollection Collection(FOpenPocketBaseCollectionRef Reference);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Authentication",
        meta = (DisplayName = "Auth Collection", Keywords = "users login identity"))
    FOpenPocketBaseAuthCollection AuthCollection(FOpenPocketBaseAuthCollectionRef Reference);

    FOpenPocketBaseCollection DynamicCollection(FString Name);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Utilities")
    FOpenPocketBaseCapabilityInfo GetCapability(EOpenPocketBaseCapability Capability) const;

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Utilities")
    FOpenPocketBaseCapabilityReport GetCapabilityReport() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Authentication",
        meta = (ReturnDisplayName = "Authenticated"))
    bool IsAuthenticated() const;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Authentication|Internal",
        meta = (
            BlueprintInternalUseOnly = "true",
            DeprecatedFunction,
            DeprecationMessage = "Replace this node with Get Current Auth Record.",
            DisplayName = "Get Current Auth Record (Legacy)",
            ReturnDisplayName = "Found"))
    bool GetCurrentAuthRecord(FOpenPocketBaseRecord& OutRecord) const;

    UFUNCTION(
        BlueprintCallable,
        BlueprintPure = false,
        Category = "Open PocketBase|Authentication",
        meta = (
            DisplayName = "Get Current Auth Record",
            ExpandBoolAsExecs = "ReturnValue",
            ReturnDisplayName = "Found"))
    bool TryGetCurrentAuthRecord(FOpenPocketBaseRecord& OutRecord) const;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Session|Internal",
        meta = (
            BlueprintInternalUseOnly = "true",
            DeprecatedFunction,
            DeprecationMessage = "Replace this node with Get Current Session.",
            DisplayName = "Get Current Session (Legacy)",
            ReturnDisplayName = "Authenticated"))
    bool GetCurrentSession(FOpenPocketBaseSessionSnapshot& OutSession) const;

    UFUNCTION(
        BlueprintCallable,
        BlueprintPure = false,
        Category = "Open PocketBase|Session",
        meta = (
            DisplayName = "Get Current Session",
            ExpandBoolAsExecs = "ReturnValue",
            ReturnDisplayName = "Authenticated"))
    bool TryGetCurrentSession(FOpenPocketBaseSessionSnapshot& OutSession) const;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Session")
    void Logout();

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Client")
    void Shutdown();

    UOpenPocketBaseSubscription* SubscribeToRecords(
        FString Collection,
        const FOpenPocketBaseRealtimeOptions& Options,
        FOpenPocketBaseError& OutError);

    UOpenPocketBaseSubscription* SubscribeToRecord(
        FString Collection,
        FString RecordId,
        const FOpenPocketBaseRealtimeOptions& Options,
        FOpenPocketBaseError& OutError);

    UOpenPocketBaseSubscription* SubscribeToTopic(
        FString Topic,
        const FOpenPocketBaseRealtimeOptions& Options,
        FOpenPocketBaseError& OutError);

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
