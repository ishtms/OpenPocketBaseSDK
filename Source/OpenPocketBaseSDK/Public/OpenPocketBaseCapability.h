// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"

#include "OpenPocketBaseCapability.generated.h"

UENUM(BlueprintType)
enum class EOpenPocketBaseCapability : uint8
{
    HttpStreaming,
    SecurePersistence,
    OAuthCallback,
    OfflineModule,
    EditorMock,
    PrivilegedModule
};

UENUM(BlueprintType)
enum class EOpenPocketBaseCapabilityStatus : uint8
{
    Supported,
    Unavailable,
    DisabledByPolicy,
    RequiresConfiguration,
    Restricted,
    TemporarilyUnavailable,
    Unsupported
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseCapabilityInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    EOpenPocketBaseCapability Capability = EOpenPocketBaseCapability::HttpStreaming;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    EOpenPocketBaseCapabilityStatus Status = EOpenPocketBaseCapabilityStatus::Unsupported;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    FString Platform;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    FString BuildConfiguration;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    FString Reason;

    bool IsSupported() const
    {
        return Status == EOpenPocketBaseCapabilityStatus::Supported;
    }
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseCapabilityReport
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Utilities")
    TArray<FOpenPocketBaseCapabilityInfo> Entries;

    bool TryGet(
        const EOpenPocketBaseCapability Capability,
        FOpenPocketBaseCapabilityInfo& OutInfo) const
    {
        for (const FOpenPocketBaseCapabilityInfo& Entry : Entries)
        {
            if (Entry.Capability == Capability)
            {
                OutInfo = Entry;
                return true;
            }
        }
        OutInfo = FOpenPocketBaseCapabilityInfo();
        return false;
    }
};
