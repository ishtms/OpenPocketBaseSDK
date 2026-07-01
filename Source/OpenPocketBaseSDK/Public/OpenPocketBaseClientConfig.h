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

    bool TryGetNormalizedBaseUrl(FString& OutBaseUrl, FOpenPocketBaseError& OutError) const;
};
