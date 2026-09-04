// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "SGraphPin.h"

struct FOpenPocketBaseFieldRef;
struct FOpenPocketBaseSelectValues;
class SComboButton;

class SOpenPocketBaseMultiSelectChoicePin final : public SGraphPin
{
public:
    SLATE_BEGIN_ARGS(SOpenPocketBaseMultiSelectChoicePin)
    {
    }
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
    virtual TSharedRef<SWidget> GetDefaultValueWidget() override;

private:
    TSharedRef<SWidget> BuildMenu();
    void SetChoiceChecked(ECheckBoxState State, FString Choice);
    FReply ClearChoices();
    ECheckBoxState IsChoiceChecked(FString Choice) const;
    FText GetCurrentLabel() const;
    bool GetCurrentValues(FOpenPocketBaseSelectValues& OutValues) const;
    void SetCurrentValues(const FOpenPocketBaseSelectValues& Values);
    bool ResolveField(FOpenPocketBaseFieldRef& OutField) const;

    TSharedPtr<SComboButton> ComboButton;
};
