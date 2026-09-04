// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseAdminTestFixtureLibrary.h"
#include "OpenPocketBaseAdminTypes.h"

#include "OpenPocketBaseAdminStringLibrary.generated.h"

UCLASS()
class OPENPOCKETBASESDKADMIN_API UOpenPocketBaseAdminStringLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Test Credentials)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminTestCredentialsToString(const FOpenPocketBaseAdminTestCredentials& Value);

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

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Collection Filter)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminCollectionFilterToString(const FOpenPocketBaseAdminCollectionFilter& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Collection Sort)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminCollectionSortToString(const FOpenPocketBaseAdminCollectionSort& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Collection List Options)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminCollectionListOptionsToString(const FOpenPocketBaseAdminCollectionListOptions& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Log Filter)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminLogFilterToString(const FOpenPocketBaseAdminLogFilter& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Log Sort)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminLogSortToString(const FOpenPocketBaseAdminLogSort& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Log List Options)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminLogListOptionsToString(const FOpenPocketBaseAdminLogListOptions& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Dynamic Admin List Options)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseDynamicAdminListOptionsToString(const FOpenPocketBaseDynamicAdminListOptions& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Page)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminPageToString(const FOpenPocketBaseAdminPage& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Policy)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminPolicyToString(const FOpenPocketBaseAdminPolicy& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin SQL Column)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminSqlColumnToString(const FOpenPocketBaseAdminSqlColumn& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin SQL Result)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminSqlResultToString(const FOpenPocketBaseAdminSqlResult& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Collection Text Field)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminCollectionTextFieldToString(EOpenPocketBaseAdminCollectionTextField Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Collection Date Field)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminCollectionDateFieldToString(EOpenPocketBaseAdminCollectionDateField Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Collection Sort Field)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminCollectionSortFieldToString(EOpenPocketBaseAdminCollectionSortField Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Collection Projection Field)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminCollectionProjectionFieldToString(EOpenPocketBaseAdminCollectionProjectionField Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Log Text Field)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminLogTextFieldToString(EOpenPocketBaseAdminLogTextField Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Log Date Field)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminLogDateFieldToString(EOpenPocketBaseAdminLogDateField Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Log Sort Field)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminLogSortFieldToString(EOpenPocketBaseAdminLogSortField Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Admin Log Projection Field)", CompactNodeTitle = "->", BlueprintAutocast, DevelopmentOnly), Category = "Open PocketBase|Admin|Utilities|String")
    static FString Conv_OpenPocketBaseAdminLogProjectionFieldToString(EOpenPocketBaseAdminLogProjectionField Value);
};
