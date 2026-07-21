#include "Schema/SOpenPocketBaseMultiSelectChoicePin.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Misc/OutputDeviceNull.h"
#include "OpenPocketBaseRecord.h"
#include "OpenPocketBaseSchemaPicker.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "OpenPocketBaseMultiSelectChoicePin"

void SOpenPocketBaseMultiSelectChoicePin::Construct(
    const FArguments& InArgs,
    UEdGraphPin* InGraphPinObj)
{
    SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget> SOpenPocketBaseMultiSelectChoicePin::GetDefaultValueWidget()
{
    return SAssignNew(ComboButton, SComboButton)
        .IsEnabled(this, &SGraphPin::GetDefaultValueIsEditable)
        .Visibility(this, &SGraphPin::GetDefaultValueVisibility)
        .OnGetMenuContent(this, &SOpenPocketBaseMultiSelectChoicePin::BuildMenu)
        .ContentPadding(FMargin(8.0f, 3.0f))
        .ButtonContent()
        [
            SNew(SBox)
            .MinDesiredWidth(140.0f)
            [
                SNew(STextBlock)
                .Text(this, &SOpenPocketBaseMultiSelectChoicePin::GetCurrentLabel)
            ]
        ];
}

TSharedRef<SWidget> SOpenPocketBaseMultiSelectChoicePin::BuildMenu()
{
    FOpenPocketBaseFieldRef Field;
    TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
    if (ResolveField(Field) && !Field.Choices.IsEmpty())
    {
        Content->AddSlot()
            .AutoHeight()
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "Menu.Button")
                .Text(LOCTEXT("ClearSelectChoices", "Clear selection"))
                .OnClicked(this, &SOpenPocketBaseMultiSelectChoicePin::ClearChoices)
            ];
        Content->AddSlot()
            .AutoHeight()
            [
                SNew(SSeparator)
            ];
        for (const FString& Choice : Field.Choices)
        {
            Content->AddSlot()
                .AutoHeight()
                .Padding(8.0f, 4.0f)
                [
                    SNew(SCheckBox)
                    .IsChecked_Lambda([this, Choice]() { return IsChoiceChecked(Choice); })
                    .OnCheckStateChanged_Lambda(
                        [this, Choice](const ECheckBoxState State)
                        {
                            SetChoiceChecked(State, Choice);
                        })
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(Choice))
                    ]
                ];
        }
    }
    else
    {
        Content->AddSlot()
            .AutoHeight()
            .Padding(10.0f, 8.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ChooseMultiSelectFieldFirst", "Choose a multiple-select field first."))
            ];
    }
    return SNew(SBox)
        .MinDesiredWidth(200.0f)
        [
            Content
        ];
}

void SOpenPocketBaseMultiSelectChoicePin::SetChoiceChecked(
    const ECheckBoxState State,
    FString Choice)
{
    FOpenPocketBaseSelectValues Current;
    GetCurrentValues(Current);
    if (State == ECheckBoxState::Checked)
    {
        Current.Values.AddUnique(MoveTemp(Choice));
    }
    else
    {
        Current.Values.Remove(Choice);
    }
    SetCurrentValues(Current);
}

FReply SOpenPocketBaseMultiSelectChoicePin::ClearChoices()
{
    SetCurrentValues({});
    return FReply::Handled();
}

ECheckBoxState SOpenPocketBaseMultiSelectChoicePin::IsChoiceChecked(FString Choice) const
{
    FOpenPocketBaseSelectValues Current;
    return GetCurrentValues(Current) && Current.Values.Contains(Choice)
        ? ECheckBoxState::Checked
        : ECheckBoxState::Unchecked;
}

FText SOpenPocketBaseMultiSelectChoicePin::GetCurrentLabel() const
{
    FOpenPocketBaseSelectValues Current;
    if (!GetCurrentValues(Current) || Current.Values.IsEmpty())
    {
        return LOCTEXT("ChooseSelectChoices", "Choose options");
    }
    return FText::Format(
        LOCTEXT("SelectedOptionCount", "{0} selected"),
        FText::AsNumber(Current.Values.Num()));
}

bool SOpenPocketBaseMultiSelectChoicePin::GetCurrentValues(
    FOpenPocketBaseSelectValues& OutValues) const
{
    OutValues = {};
    if (GraphPinObj == nullptr || GraphPinObj->GetDefaultAsString().IsEmpty())
    {
        return true;
    }
    FOutputDeviceNull Output;
    return FOpenPocketBaseSelectValues::StaticStruct()->ImportText(
        *GraphPinObj->GetDefaultAsString(),
        &OutValues,
        nullptr,
        PPF_SerializedAsImportText,
        &Output,
        TEXT("SelectValues")) != nullptr;
}

void SOpenPocketBaseMultiSelectChoicePin::SetCurrentValues(
    const FOpenPocketBaseSelectValues& Values)
{
    if (GraphPinObj == nullptr || GraphPinObj->GetSchema() == nullptr)
    {
        return;
    }

    FString DefaultValue;
    FOpenPocketBaseSelectValues::StaticStruct()->ExportText(
        DefaultValue,
        &Values,
        &Values,
        nullptr,
        PPF_SerializedAsImportText,
        nullptr);
    const FScopedTransaction Transaction(
        LOCTEXT("SetMultiSelectChoices", "Choose PocketBase select values"));
    GraphPinObj->Modify();
    GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, DefaultValue);
}

bool SOpenPocketBaseMultiSelectChoicePin::ResolveField(
    FOpenPocketBaseFieldRef& OutField) const
{
    OutField = {};
    UEdGraphNode* Node = GraphPinObj != nullptr ? GraphPinObj->GetOwningNode() : nullptr;
    if (Node == nullptr)
    {
        return false;
    }
    const FString FieldPinName = Node->GetPinMetaData(
        GraphPinObj->PinName,
        TEXT("OpenPocketBaseSelectField"));
    return FOpenPocketBaseSchemaPickerModel::ResolveFieldFromPinContext(
        *GraphPinObj,
        FieldPinName,
        OutField);
}

#undef LOCTEXT_NAMESPACE
