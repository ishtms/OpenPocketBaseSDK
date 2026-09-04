// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "EdGraphUtilities.h"

class FOpenPocketBaseSchemaGraphPinFactory final : public FGraphPanelPinFactory
{
public:
    virtual TSharedPtr<SGraphPin> CreatePin(UEdGraphPin* Pin) const override;
};
