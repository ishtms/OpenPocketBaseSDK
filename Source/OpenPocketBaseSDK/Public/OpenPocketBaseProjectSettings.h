#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenPocketBaseClientConfig.h"

#include "OpenPocketBaseProjectSettings.generated.h"

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseProjectProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open PocketBase")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open PocketBase")
    FString BaseUrl;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open PocketBase")
    FString AcceptLanguage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open PocketBase")
    EOpenPocketBaseSessionPersistence SessionPersistence =
        EOpenPocketBaseSessionPersistence::MemoryOnly;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Authentication", AdvancedDisplay)
    bool bProactiveAuthRefresh = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Authentication", AdvancedDisplay,
        meta = (ClampMin = "0.0", ClampMax = "3600.0"))
    double AuthRefreshLeadTimeSeconds = 30.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Authentication", AdvancedDisplay)
    bool bRetryEligibleReadsAfterAuthRefresh = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Authentication", AdvancedDisplay)
    bool bEnableAssistedOAuth = false;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Open PocketBase SDK"))
class OPENPOCKETBASESDK_API UOpenPocketBaseProjectSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category = "Profiles")
    FName DefaultProfile;

    UPROPERTY(Config, EditAnywhere, Category = "Profiles",
        meta = (TitleProperty = "Name"))
    TArray<FOpenPocketBaseProjectProfile> Profiles;

    UPROPERTY(Config, EditAnywhere, Category = "Development",
        meta = (DisplayName = "Allow OPENPOCKETBASE_DEVELOPMENT_BASE_URL override"))
    bool bAllowDevelopmentEnvironmentOverride = false;

    UPROPERTY(Config, EditAnywhere, Category = "Validation")
    bool bRequireRealtimeStreaming = false;

    UPROPERTY(Config, EditAnywhere, Category = "Validation")
    bool bRequireOfflineModule = false;

    UPROPERTY(Config, EditAnywhere, Category = "Validation")
    bool bRequirePrivilegedModule = false;

    virtual FName GetCategoryName() const override;

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Settings",
        meta = (ReturnDisplayName = "Resolved"))
    bool TryResolveProfile(
        FName Profile,
        FOpenPocketBaseClientConfig& OutConfig,
        FOpenPocketBaseError& OutError) const;

    bool TryResolveProfileWithDevelopmentOverride(
        FName Profile,
        const FString& OverrideBaseUrl,
        FOpenPocketBaseClientConfig& OutConfig,
        FOpenPocketBaseError& OutError) const;

    static const TCHAR* GetDevelopmentOverrideEnvironmentVariableName();

private:
    bool TryResolveProfileInternal(
        FName Profile,
        const FString* OverrideBaseUrl,
        FOpenPocketBaseClientConfig& OutConfig,
        FOpenPocketBaseError& OutError) const;
};
