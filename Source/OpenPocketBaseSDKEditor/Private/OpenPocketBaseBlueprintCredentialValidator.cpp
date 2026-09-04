// Copyright 2026 Ishtmeet Singh.

#include "OpenPocketBaseBlueprintCredentialValidator.h"

#include "Engine/Blueprint.h"
#include "OpenPocketBaseEditorValidation.h"

bool UOpenPocketBaseBlueprintCredentialValidator::CanValidateAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InObject,
    FDataValidationContext& InContext) const
{
    return InObject != nullptr && InObject->IsA<UBlueprint>();
}

EDataValidationResult UOpenPocketBaseBlueprintCredentialValidator::ValidateLoadedAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InAsset,
    FDataValidationContext& Context)
{
    const UBlueprint* Blueprint = Cast<UBlueprint>(InAsset);
    if (Blueprint == nullptr)
    {
        return EDataValidationResult::NotValidated;
    }

    FOpenPocketBaseEditorValidationReport Report;
    FOpenPocketBaseEditorValidator::ScanBlueprint(*Blueprint, Report);
    if (Report.HasErrors())
    {
        AssetFails(
            InAsset,
            FText::FromString(
                TEXT("A privileged PocketBase credential is embedded in this asset. "
                     "Clear the credential default and supply it from a process-local prompt or secret provider.")));
        return EDataValidationResult::Invalid;
    }

    AssetPasses(InAsset);
    return EDataValidationResult::Valid;
}
