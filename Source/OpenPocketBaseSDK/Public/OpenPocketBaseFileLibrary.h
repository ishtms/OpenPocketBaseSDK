#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseBlueprintClient.h"
#include "OpenPocketBaseFile.h"

#include "OpenPocketBaseFileLibrary.generated.h"

UCLASS()
class OPENPOCKETBASESDK_API UOpenPocketBaseFileLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Files",
        meta = (DisplayName = "File From Path", ToolTip = "Prepares a local file for upload to the selected PocketBase file field. File name and content type are inferred when omitted.", Keywords = "pocketbase upload file path attachment"))
    static FOpenPocketBaseFileInput FileFromPath(
        FOpenPocketBaseFileFieldRef Field,
        FString FilePath,
        FString FileName = "",
        FString ContentType = "",
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Files",
        meta = (DisplayName = "File From Bytes"))
    static FOpenPocketBaseFileInput FileFromBytes(
        FOpenPocketBaseFileFieldRef Field,
        TArray<uint8> Bytes,
        FString FileName,
        FString ContentType = "",
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Files|Advanced",
        meta = (DisplayName = "Dynamic File From Path (Advanced)"))
    static FOpenPocketBaseFileInput DynamicFileFromPath(
        FString FieldName,
        FString FilePath,
        FString FileName = "",
        FString ContentType = "",
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Files|Advanced",
        meta = (DisplayName = "Dynamic File From Bytes (Advanced)"))
    static FOpenPocketBaseFileInput DynamicFileFromBytes(
        FString FieldName,
        TArray<uint8> Bytes,
        FString FileName,
        FString ContentType = "",
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(
        BlueprintCallable,
        Category = "Open PocketBase|Files",
        meta = (
            DisplayName = "Try Build File URL",
            AutoCreateRefTerm = "Options",
            ExpandBoolAsExecs = "ReturnValue",
            ReturnDisplayName = "Built"))
    static bool TryBuildFileUrl(
        FOpenPocketBaseCollection Collection,
        FString RecordId,
        FString FileName,
        FOpenPocketBaseFileUrlOptions Options,
        FString& Url,
        FOpenPocketBaseError& Error);
};
