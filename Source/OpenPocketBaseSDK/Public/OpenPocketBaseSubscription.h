#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseRealtime.h"

#include "OpenPocketBaseSubscription.generated.h"

class UOpenPocketBaseClient;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseRealtimeEventDelegate,
    FOpenPocketBaseRealtimeEvent,
    Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseConnectionStateDelegate,
    EOpenPocketBaseRealtimeConnectionState,
    State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseRealtimeErrorDelegate,
    FOpenPocketBaseError,
    Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOpenPocketBaseRealtimeResyncDelegate);

UCLASS(BlueprintType)
class OPENPOCKETBASESDK_API UOpenPocketBaseSubscription final : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Open PocketBase|Realtime")
    FOpenPocketBaseRealtimeEventDelegate OnEvent;

    UPROPERTY(BlueprintAssignable, Category = "Open PocketBase|Realtime")
    FOpenPocketBaseConnectionStateDelegate OnConnectionStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Open PocketBase|Realtime")
    FOpenPocketBaseRealtimeErrorDelegate OnError;

    UPROPERTY(BlueprintAssignable, Category = "Open PocketBase|Realtime")
    FOpenPocketBaseRealtimeResyncDelegate OnRealtimeResyncRequired;

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Realtime")
    void Unsubscribe();

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Realtime",
        meta = (ReturnDisplayName = "Active"))
    bool IsActive() const;

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Realtime")
    EOpenPocketBaseRealtimeConnectionState GetConnectionState() const;

    virtual void BeginDestroy() override;

private:
    void Initialize(
        UOpenPocketBaseClient* InOwner,
        FOpenPocketBaseSubscriptionHandle InHandle);
    void HandleNativeEvent(const FOpenPocketBaseRealtimeEvent& Event);
    void HandleNativeState(EOpenPocketBaseRealtimeConnectionState State);
    void HandleNativeError(const FOpenPocketBaseError& Error);
    void HandleNativeResyncRequired();

    TWeakObjectPtr<UOpenPocketBaseClient> OwnerClient;
    TUniquePtr<FOpenPocketBaseSubscriptionHandle> NativeHandle;
    EOpenPocketBaseRealtimeConnectionState ConnectionState =
        EOpenPocketBaseRealtimeConnectionState::Created;

    friend class UOpenPocketBaseClient;
};
