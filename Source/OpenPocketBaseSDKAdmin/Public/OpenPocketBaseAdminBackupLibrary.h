#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseAdminTypes.h"
#include "OpenPocketBaseError.h"

#include "OpenPocketBaseAdminBackupLibrary.generated.h"

UCLASS()
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAdminBackupLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Admin|Backups",
        meta = (
            DisplayName = "Save Backup Download to File",
            ExpandBoolAsExecs = "ReturnValue",
            ReturnDisplayName = "Saved",
            DevelopmentOnly))
    static bool SaveBackupDownloadToFile(
        const FOpenPocketBaseAdminBackupDownload& Download,
        FString DestinationPath,
        bool bReplaceExisting,
        FString& SavedPath,
        FOpenPocketBaseError& Error);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Admin|Backups",
        meta = (
            DisplayName = "Delete Saved Backup File",
            ExpandBoolAsExecs = "ReturnValue",
            ReturnDisplayName = "Deleted",
            DevelopmentOnly))
    static bool DeleteSavedBackupFile(
        FString SavedPath,
        FOpenPocketBaseError& Error);
};
