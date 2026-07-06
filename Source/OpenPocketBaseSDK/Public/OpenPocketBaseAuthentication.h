#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseRecord.h"

#include "OpenPocketBaseAuthentication.generated.h"

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseTimedAuthMethod
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    bool bEnabled = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    int32 DurationSeconds = 0;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBasePasswordAuthMethod
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    bool bEnabled = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    TArray<FString> IdentityFields;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseOAuthProvider
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FString State;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FString AuthUrl;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FString CodeVerifier;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FString CodeChallenge;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FString CodeChallengeMethod;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseOAuth2AuthMethod
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    bool bEnabled = false;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    TArray<FOpenPocketBaseOAuthProvider> Providers;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseAuthMethods
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FOpenPocketBaseTimedAuthMethod Mfa;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FOpenPocketBaseTimedAuthMethod Otp;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FOpenPocketBasePasswordAuthMethod Password;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FOpenPocketBaseOAuth2AuthMethod OAuth2;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseOtpRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FString OtpId;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseMfaContinuation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Open PocketBase|Authentication")
    FString Id;

    bool IsSet() const
    {
        return !Id.IsEmpty();
    }
};

UENUM(BlueprintType)
enum class EOpenPocketBaseAuthAttemptStatus : uint8
{
    Authenticated,
    MfaRequired
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseAuthAttempt
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    EOpenPocketBaseAuthAttemptStatus Status = EOpenPocketBaseAuthAttemptStatus::Authenticated;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FOpenPocketBaseAuthResult Authentication;

    UPROPERTY(BlueprintReadOnly, Category = "Open PocketBase|Authentication")
    FOpenPocketBaseMfaContinuation Mfa;
};
