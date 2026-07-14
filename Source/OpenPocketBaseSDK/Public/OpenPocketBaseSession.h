#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseRecord.h"

#include "OpenPocketBaseSession.generated.h"

UENUM(BlueprintType)
enum class EOpenPocketBaseSessionChangeReason : uint8
{
    LoggedIn,
    LoggedOut,
    Refreshed,
    Restored,
    UserSwitched,
    RecordUpdated
};

UENUM(BlueprintType)
enum class EOpenPocketBaseSessionPersistenceState : uint8
{
    MemoryOnly,
    Persisted,
    Unavailable,
    Failed
};

UENUM(BlueprintType)
enum class EOpenPocketBaseSessionRestoreStatus : uint8
{
    Restored,
    Verified,
    NotFound,
    Expired,
    Corrupt,
    Unavailable,
    PolicyRejected
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseSessionSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Session")
    bool bAuthenticated = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Session")
    FString AuthCollection;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Session")
    int64 AuthGeneration = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Session")
    EOpenPocketBaseSessionPersistenceState PersistenceState =
        EOpenPocketBaseSessionPersistenceState::MemoryOnly;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Session")
    EOpenPocketBaseSessionChangeReason Reason = EOpenPocketBaseSessionChangeReason::LoggedOut;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Session")
    FOpenPocketBaseRecord AuthRecord;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseSessionRestoreResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Session")
    EOpenPocketBaseSessionRestoreStatus Status = EOpenPocketBaseSessionRestoreStatus::NotFound;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Session")
    FOpenPocketBaseSessionSnapshot Session;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
    FOpenPocketBaseSessionChanged,
    const FOpenPocketBaseSessionSnapshot&);
