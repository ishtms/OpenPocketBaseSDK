// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "EditorValidatorBase.h"

#include "OpenPocketBaseBlueprintCredentialValidator.generated.h"

UCLASS()
class UOpenPocketBaseBlueprintCredentialValidator final : public UEditorValidatorBase
{
    GENERATED_BODY()

protected:
    virtual bool CanValidateAsset_Implementation(
        const FAssetData& InAssetData,
        UObject* InObject,
        FDataValidationContext& InContext) const override;

    virtual EDataValidationResult ValidateLoadedAsset_Implementation(
        const FAssetData& InAssetData,
        UObject* InAsset,
        FDataValidationContext& Context) override;
};
