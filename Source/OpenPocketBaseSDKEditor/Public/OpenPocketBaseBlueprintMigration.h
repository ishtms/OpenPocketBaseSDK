// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"

class UBlueprint;

class OPENPOCKETBASESDKEDITOR_API FOpenPocketBaseBlueprintMigration final
{
public:
    static int32 UpgradeNativeMakeNodes(UBlueprint& Blueprint);
};
