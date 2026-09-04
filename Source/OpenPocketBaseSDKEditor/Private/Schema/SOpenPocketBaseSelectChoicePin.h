// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "SGraphPin.h"

struct FOpenPocketBaseFieldRef;
class SComboButton;

class SOpenPocketBaseSelectChoicePin final : public SGraphPin
{
public:
    SLATE_BEGIN_ARGS(SOpenPocketBaseSelectChoicePin)
    {
    }
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
    virtual TSharedRef<SWidget> GetDefaultValueWidget() override;

private:
    TSharedRef<SWidget> BuildMenu();
    FReply SetChoice(FString Choice);
    FText GetCurrentLabel() const;
    bool ResolveField(FOpenPocketBaseFieldRef& OutField) const;

    TSharedPtr<SComboButton> ComboButton;
};
