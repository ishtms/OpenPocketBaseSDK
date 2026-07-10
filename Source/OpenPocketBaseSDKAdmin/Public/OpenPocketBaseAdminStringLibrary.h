#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseAdminTypes.h"

#include "OpenPocketBaseAdminStringLibrary.generated.h"

UCLASS()
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAdminStringLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Backup)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminBackupToString(const FOpenPocketBaseAdminBackup& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Backup Download)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminBackupDownloadToString(const FOpenPocketBaseAdminBackupDownload& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Backup List)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminBackupListToString(const FOpenPocketBaseAdminBackupList& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Document)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminDocumentToString(const FOpenPocketBaseAdminDocument& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Document List)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminDocumentListToString(const FOpenPocketBaseAdminDocumentList& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Identity)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminIdentityToString(const FOpenPocketBaseAdminIdentity& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin List Options)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminListOptionsToString(const FOpenPocketBaseAdminListOptions& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Page)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminPageToString(const FOpenPocketBaseAdminPage& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Policy)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminPolicyToString(const FOpenPocketBaseAdminPolicy& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin SQL Column)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminSqlColumnToString(const FOpenPocketBaseAdminSqlColumn& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin SQL Result)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminSqlResultToString(const FOpenPocketBaseAdminSqlResult& Value);
};
