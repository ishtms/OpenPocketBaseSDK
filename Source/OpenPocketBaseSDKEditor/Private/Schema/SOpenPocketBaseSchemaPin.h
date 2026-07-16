#pragma once

#include "OpenPocketBaseSchemaPicker.h"
#include "SGraphPin.h"

class SCheckBox;
class SComboButton;
class SSearchBox;

class SOpenPocketBaseSchemaPin final : public SGraphPin
{
public:
    SLATE_BEGIN_ARGS(SOpenPocketBaseSchemaPin)
    {
    }
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
    virtual TSharedRef<SWidget> GetDefaultValueWidget() override;

private:
    using FChoicePtr = TSharedPtr<FOpenPocketBaseSchemaPickerChoice>;

    TSharedRef<SWidget> BuildMenu();
    TSharedRef<ITableRow> BuildChoiceRow(
        FChoicePtr Choice,
        const TSharedRef<STableViewBase>& OwnerTable) const;
    void RefreshChoices();
    void ApplySearch(const FText& SearchText);
    void SelectChoice(FChoicePtr Choice, ESelectInfo::Type SelectInfo);
    void SetShowSystemCollections(ECheckBoxState State);
    void SetShowHiddenFields(ECheckBoxState State);
    FText GetCurrentLabel() const;
    FText GetCurrentToolTip() const;
    FSlateColor GetCurrentColor() const;
    EVisibility GetEmptyMessageVisibility() const;
    FText GetEmptyMessage() const;
    const UScriptStruct* GetReferenceStruct() const;
    bool IsCollectionPin() const;
    bool FindSiblingCollection(FOpenPocketBaseCollectionRef& OutCollection) const;
    void LoadSchemas(TArray<UOpenPocketBaseSchema*>& OutSchemas) const;

    TArray<FChoicePtr> AllChoices;
    TArray<FChoicePtr> FilteredChoices;
    TSharedPtr<SComboButton> ComboButton;
    TSharedPtr<SSearchBox> SearchBox;
    TSharedPtr<SListView<FChoicePtr>> ChoiceList;
    FText CurrentSearch;
    bool bShowSystemCollections = false;
    bool bShowHiddenFields = false;
};
