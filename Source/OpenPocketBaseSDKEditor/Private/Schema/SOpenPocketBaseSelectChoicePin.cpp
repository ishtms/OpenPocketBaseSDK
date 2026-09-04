// Copyright 2026 Ishtmeet Singh.

#include "Schema/SOpenPocketBaseSelectChoicePin.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "OpenPocketBaseSchemaPicker.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "OpenPocketBaseSelectChoicePin"

void SOpenPocketBaseSelectChoicePin::Construct(
    const FArguments& InArgs,
    UEdGraphPin* InGraphPinObj)
{
    SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget> SOpenPocketBaseSelectChoicePin::GetDefaultValueWidget()
{
    return SAssignNew(ComboButton, SComboButton)
        .IsEnabled(this, &SGraphPin::GetDefaultValueIsEditable)
        .Visibility(this, &SGraphPin::GetDefaultValueVisibility)
        .OnGetMenuContent(this, &SOpenPocketBaseSelectChoicePin::BuildMenu)
        .ContentPadding(FMargin(8.0f, 3.0f))
        .ButtonContent()
        [
            SNew(SBox)
            .MinDesiredWidth(140.0f)
            [
                SNew(STextBlock)
                .Text(this, &SOpenPocketBaseSelectChoicePin::GetCurrentLabel)
            ]
        ];
}

TSharedRef<SWidget> SOpenPocketBaseSelectChoicePin::BuildMenu()
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
                .Text(LOCTEXT("ClearSelectChoice", "Clear selection"))
                .OnClicked_Lambda([this]() { return SetChoice(FString()); })
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
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "Menu.Button")
                    .Text(FText::FromString(Choice))
                    .OnClicked_Lambda([this, Choice]() { return SetChoice(Choice); })
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
                .Text(LOCTEXT("ChooseSelectFieldFirst", "Choose a select field first."))
            ];
    }
    return SNew(SBox)
        .MinDesiredWidth(180.0f)
        [
            Content
        ];
}

FReply SOpenPocketBaseSelectChoicePin::SetChoice(FString Choice)
{
    if (GraphPinObj != nullptr && GraphPinObj->GetSchema() != nullptr)
    {
        const FScopedTransaction Transaction(LOCTEXT("SetSelectChoice", "Choose PocketBase select value"));
        GraphPinObj->Modify();
        GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, Choice);
    }
    if (ComboButton.IsValid())
    {
        ComboButton->SetIsOpen(false);
    }
    return FReply::Handled();
}

FText SOpenPocketBaseSelectChoicePin::GetCurrentLabel() const
{
    if (GraphPinObj == nullptr || GraphPinObj->GetDefaultAsString().IsEmpty())
    {
        return LOCTEXT("ChooseSelectChoice", "Choose option");
    }
    return FText::FromString(GraphPinObj->GetDefaultAsString());
}

bool SOpenPocketBaseSelectChoicePin::ResolveField(FOpenPocketBaseFieldRef& OutField) const
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
