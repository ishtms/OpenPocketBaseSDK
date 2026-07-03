#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseError.h"

#include "OpenPocketBaseClientConfig.generated.h"

UENUM(BlueprintType)
enum class EOpenPocketBaseSessionPersistence : uint8
{
    MemoryOnly,
    RequireSecureStorage
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseClientConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client")
    FString BaseUrl;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client")
    FString ProfileName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client")
    FString AcceptLanguage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client")
    TMap<FString, FString> DefaultHeaders;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Client")
    EOpenPocketBaseSessionPersistence SessionPersistence = EOpenPocketBaseSessionPersistence::MemoryOnly;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Authentication", AdvancedDisplay)
    bool bProactiveAuthRefresh = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Authentication", AdvancedDisplay, meta = (ClampMin = "0.0", ClampMax = "3600.0"))
    double AuthRefreshLeadTimeSeconds = 30.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open PocketBase|Authentication", AdvancedDisplay)
    bool bRetryEligibleReadsAfterAuthRefresh = true;

    bool TryGetNormalizedBaseUrl(FString& OutBaseUrl, FOpenPocketBaseError& OutError) const;
};
