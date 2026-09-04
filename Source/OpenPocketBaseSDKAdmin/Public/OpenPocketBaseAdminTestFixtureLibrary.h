// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseError.h"

#include "OpenPocketBaseAdminTestFixtureLibrary.generated.h"

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDKADMIN_API FOpenPocketBaseAdminTestCredentials
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Open PocketBase|Admin|Testing")
    FString BaseUrl;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Open PocketBase|Admin|Testing")
    FString Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Open PocketBase|Admin|Testing", meta = (PasswordField = "true"))
    FString Password;
};

UCLASS()
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAdminTestFixtureLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Admin|Testing",
        meta = (
            DisplayName = "Load Admin Test Credentials",
            ExpandBoolAsExecs = "ReturnValue",
            ReturnDisplayName = "Loaded",
            CPP_Default_ProjectRelativePath = ".runtime/admin-credentials.json",
            DevelopmentOnly))
    static bool LoadAdminTestCredentials(
        FString ProjectRelativePath,
        FOpenPocketBaseAdminTestCredentials& Credentials,
        FOpenPocketBaseError& Error);

};
