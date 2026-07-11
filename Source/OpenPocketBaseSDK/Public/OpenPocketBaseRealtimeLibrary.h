#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseBlueprintClient.h"

#include "OpenPocketBaseRealtimeLibrary.generated.h"

UCLASS()
class OPENPOCKETBASESDK_API UOpenPocketBaseRealtimeLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Realtime",
        meta = (
            DisplayName = "Subscribe to Records",
            AutoCreateRefTerm = "Options",
            AdvancedDisplay = "Options",
            ExpandBoolAsExecs = "ReturnValue"))
    static bool SubscribeToRecords(
        FOpenPocketBaseCollection Collection,
        const FOpenPocketBaseRealtimeOptions& Options,
        UOpenPocketBaseSubscription*& Subscription,
        FOpenPocketBaseError& Error);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Realtime",
        meta = (
            DisplayName = "Subscribe to Record",
            AutoCreateRefTerm = "Options",
            AdvancedDisplay = "Options",
            ExpandBoolAsExecs = "ReturnValue"))
    static bool SubscribeToRecord(
        FOpenPocketBaseCollection Collection,
        const FString& RecordId,
        const FOpenPocketBaseRealtimeOptions& Options,
        UOpenPocketBaseSubscription*& Subscription,
        FOpenPocketBaseError& Error);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Realtime|Advanced",
        meta = (
            DisplayName = "Subscribe to Realtime Topic",
            AutoCreateRefTerm = "Options",
            AdvancedDisplay = "Options",
            ExpandBoolAsExecs = "ReturnValue"))
    static bool SubscribeToTopic(
        UOpenPocketBaseClient* PocketBaseClient,
        const FString& Topic,
        const FOpenPocketBaseRealtimeOptions& Options,
        UOpenPocketBaseSubscription*& Subscription,
        FOpenPocketBaseError& Error);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Realtime",
        meta = (DisplayName = "Unsubscribe All Realtime"))
    static void UnsubscribeAllRealtime(UOpenPocketBaseClient* PocketBaseClient);
};
