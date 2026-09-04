// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"
#include "JsonObjectWrapper.h"
#include "OpenPocketBaseError.h"
#include "OpenPocketBaseRecord.h"
#include "Templates/Function.h"

#include "OpenPocketBaseRealtime.generated.h"

UENUM(BlueprintType)
enum class EOpenPocketBaseRealtimeConnectionState : uint8
{
    Created,
    Subscribing,
    Active,
    Reconnecting,
    Stopped
};

UENUM(BlueprintType)
enum class EOpenPocketBaseRealtimeAction : uint8
{
    Unknown,
    Create,
    Update,
    Delete
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseRealtimeEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Realtime")
    FString Topic;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Realtime")
    EOpenPocketBaseRealtimeAction Action = EOpenPocketBaseRealtimeAction::Unknown;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Realtime")
    FString ActionName;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Realtime")
    bool bHasRecord = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Realtime")
    FOpenPocketBaseRecord Record;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Realtime")
    FJsonObjectWrapper Data;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseRealtimeOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Realtime")
    FOpenPocketBaseFilter Filter;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Realtime")
    TArray<FOpenPocketBaseExpand> Expand;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Realtime")
    TArray<FOpenPocketBaseFieldSelection> Fields;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Realtime", AdvancedDisplay)
    TMap<FString, FString> QueryParameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Realtime", AdvancedDisplay)
    TMap<FString, FString> Headers;

    bool IsValid() const;
    bool BelongsTo(const FOpenPocketBaseCollectionRef& Collection) const;
};

struct OPENPOCKETBASESDK_API FOpenPocketBaseRealtimeCallbacks
{
    TFunction<void(const FOpenPocketBaseRealtimeEvent&)> OnEvent;
    TFunction<void(EOpenPocketBaseRealtimeConnectionState)> OnConnectionStateChanged;
    TFunction<void(const FOpenPocketBaseError&)> OnError;
    TFunction<void()> OnResyncRequired;
};

namespace OpenPocketBase::Realtime
{
class FConnectionManager;
}

class OPENPOCKETBASESDK_API FOpenPocketBaseSubscriptionHandle
{
public:
    FOpenPocketBaseSubscriptionHandle() = default;
    ~FOpenPocketBaseSubscriptionHandle();

    FOpenPocketBaseSubscriptionHandle(FOpenPocketBaseSubscriptionHandle&& Other) noexcept;
    FOpenPocketBaseSubscriptionHandle& operator=(FOpenPocketBaseSubscriptionHandle&& Other) noexcept;
    FOpenPocketBaseSubscriptionHandle(const FOpenPocketBaseSubscriptionHandle&) = delete;
    FOpenPocketBaseSubscriptionHandle& operator=(const FOpenPocketBaseSubscriptionHandle&) = delete;

    void Unsubscribe();
    bool IsActive() const;

private:
    FOpenPocketBaseSubscriptionHandle(
        TUniqueFunction<void()> InUnsubscribe,
        TUniqueFunction<bool()> InIsActive);

    TUniqueFunction<void()> UnsubscribeAction;
    TUniqueFunction<bool()> IsActiveQuery;

    friend class OpenPocketBase::Realtime::FConnectionManager;
};
