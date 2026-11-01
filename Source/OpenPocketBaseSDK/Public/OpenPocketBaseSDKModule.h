// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "Modules/ModuleInterface.h"

class FOpenPocketBaseSDKModule final : public IModuleInterface
{
public:
    virtual void ShutdownModule() override;
    virtual bool SupportsDynamicReloading() override { return false; }
};
