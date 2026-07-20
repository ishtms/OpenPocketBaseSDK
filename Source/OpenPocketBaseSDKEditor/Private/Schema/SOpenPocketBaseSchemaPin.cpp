#include "Schema/SOpenPocketBaseSchemaPin.h"
#include "Schema/OpenPocketBaseSchemaGraphContext.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Modules/ModuleManager.h"
#include "OpenPocketBaseProjectSettings.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "OpenPocketBaseSchemaPin"

void SOpenPocketBaseSchemaPin::Construct(
    const FArguments& InArgs,
    UEdGraphPin* InGraphPinObj)
{
    SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget> SOpenPocketBaseSchemaPin::GetDefaultValueWidget()
{
    return SAssignNew(ComboButton, SComboButton)
        .IsEnabled(this, &SGraphPin::GetDefaultValueIsEditable)
        .Visibility(this, &SGraphPin::GetDefaultValueVisibility)
        .OnGetMenuContent(this, &SOpenPocketBaseSchemaPin::BuildMenu)
        .ContentPadding(FMargin(8.0f, 3.0f))
        .ToolTipText(this, &SOpenPocketBaseSchemaPin::GetCurrentToolTip)
        .ButtonContent()
        [
            SNew(SBox)
            .MinDesiredWidth(150.0f)
            [
                SNew(STextBlock)
                .Text(this, &SOpenPocketBaseSchemaPin::GetCurrentLabel)
                .ColorAndOpacity(this, &SOpenPocketBaseSchemaPin::GetCurrentColor)
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ];
}

TSharedRef<SWidget> SOpenPocketBaseSchemaPin::BuildMenu()
{
    RefreshChoices();
    const FText Hint = IsCollectionPin()
        ? LOCTEXT("SearchCollections", "Search collections")
        : LOCTEXT("SearchFields", "Search fields");

    TSharedRef<SVerticalBox> Content = SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.0f, 8.0f, 8.0f, 5.0f)
        [
            SAssignNew(SearchBox, SSearchBox)
            .HintText(Hint)
            .OnTextChanged(this, &SOpenPocketBaseSchemaPin::ApplySearch)
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(4.0f, 0.0f)
        [
            SNew(SBox)
            .MinDesiredHeight(160.0f)
            .MaxDesiredHeight(360.0f)
            [
                SAssignNew(ChoiceList, SListView<FChoicePtr>)
                .ListItemsSource(&FilteredChoices)
                .SelectionMode(ESelectionMode::Single)
                .OnGenerateRow(this, &SOpenPocketBaseSchemaPin::BuildChoiceRow)
                .OnSelectionChanged(this, &SOpenPocketBaseSchemaPin::SelectChoice)
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(10.0f, 8.0f)
        [
            SNew(STextBlock)
            .Text(this, &SOpenPocketBaseSchemaPin::GetEmptyMessage)
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            .AutoWrapText(true)
            .Visibility(this, &SOpenPocketBaseSchemaPin::GetEmptyMessageVisibility)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SSeparator)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.0f, 5.0f, 8.0f, 0.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("ClearSchemaChoice", "Clear selection"))
            .OnClicked(this, &SOpenPocketBaseSchemaPin::ClearChoice)
        ];

    if (IsCollectionPin())
    {
        Content->AddSlot()
            .AutoHeight()
            .Padding(8.0f, 5.0f, 8.0f, 7.0f)
            [
                SNew(SCheckBox)
                .IsChecked(bShowSystemCollections ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                .OnCheckStateChanged(this, &SOpenPocketBaseSchemaPin::SetShowSystemCollections)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ShowSystemCollections", "Show PocketBase system collections"))
                ]
            ];
    }
    else
    {
        Content->AddSlot()
            .AutoHeight()
            .Padding(8.0f, 5.0f, 8.0f, 7.0f)
            [
                SNew(SCheckBox)
                .IsChecked(bShowHiddenFields ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                .OnCheckStateChanged(this, &SOpenPocketBaseSchemaPin::SetShowHiddenFields)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ShowHiddenFields", "Show hidden fields"))
                ]
            ];
    }

    return SNew(SBox)
        .WidthOverride(390.0f)
        [
            Content
        ];
}

TSharedRef<ITableRow> SOpenPocketBaseSchemaPin::BuildChoiceRow(
    FChoicePtr Choice,
    const TSharedRef<STableViewBase>& OwnerTable) const
{
    return SNew(STableRow<FChoicePtr>, OwnerTable)
        .Padding(FMargin(8.0f, 5.0f))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(Choice->Label)
                .HighlightText(CurrentSearch)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 2.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(Choice->Detail)
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
            ]
        ];
}

void SOpenPocketBaseSchemaPin::RefreshChoices()
{
    TArray<UOpenPocketBaseSchema*> Schemas;
    LoadSchemas(Schemas);
    TArray<FOpenPocketBaseSchemaPickerChoice> Choices;
    if (IsCollectionPin())
    {
        FOpenPocketBaseSchemaPickerModel::BuildCollectionChoices(
            Schemas,
            GetReferenceStruct(),
            bShowSystemCollections,
            GraphPinObj != nullptr
                ? OpenPocketBase::Editor::FindCollectionRequirement(*GraphPinObj)
                : EOpenPocketBaseCollectionRequirement::Any,
            Choices);
    }
    else
    {
        FOpenPocketBaseCollectionRef Collection;
        FOpenPocketBaseFieldPickerFilter Filter;
        Filter.Collection = FindSiblingCollection(Collection) ? &Collection : nullptr;
        Filter.ReferenceStruct = GetReferenceStruct();
        Filter.bWritableOnly = GraphPinObj != nullptr &&
            GraphPinObj->GetOwningNode()->GetPinMetaData(
                GraphPinObj->PinName,
                TEXT("OpenPocketBaseFieldAccess")) == TEXT("Write");
        Filter.bIncludeHidden = bShowHiddenFields;
        FOpenPocketBaseSchemaPickerModel::BuildFieldChoices(Schemas, Filter, Choices);
    }

    AllChoices.Reset(Choices.Num());
    for (FOpenPocketBaseSchemaPickerChoice& Choice : Choices)
    {
        AllChoices.Add(MakeShared<FOpenPocketBaseSchemaPickerChoice>(MoveTemp(Choice)));
    }
    ApplySearch(CurrentSearch);
}

void SOpenPocketBaseSchemaPin::ApplySearch(const FText& SearchText)
{
    CurrentSearch = SearchText;
    const FString Query = SearchText.ToString().TrimStartAndEnd();
    FilteredChoices.Reset();
    for (const FChoicePtr& Choice : AllChoices)
    {
        if (Query.IsEmpty() || Choice->SearchText.Contains(Query, ESearchCase::IgnoreCase))
        {
            FilteredChoices.Add(Choice);
        }
    }
    if (ChoiceList.IsValid())
    {
        ChoiceList->RequestListRefresh();
    }
}

void SOpenPocketBaseSchemaPin::SelectChoice(
    FChoicePtr Choice,
    const ESelectInfo::Type SelectInfo)
{
    if (!Choice.IsValid() || GraphPinObj == nullptr)
    {
        return;
    }

    const FString Value = IsCollectionPin()
        ? FOpenPocketBaseSchemaPickerModel::ExportCollectionDefault(Choice->Collection)
        : FOpenPocketBaseSchemaPickerModel::ExportFieldDefault(GetReferenceStruct(), Choice->Field);
    if (Value.IsEmpty() || Value == GraphPinObj->GetDefaultAsString())
    {
        return;
    }

    const FScopedTransaction Transaction(
        LOCTEXT("ChooseSchemaReferenceTransaction", "Choose PocketBase schema value"));
    GraphPinObj->Modify();
    GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, Value);
    if (ComboButton.IsValid())
    {
        ComboButton->SetIsOpen(false);
    }
}

FReply SOpenPocketBaseSchemaPin::ClearChoice()
{
    if (GraphPinObj != nullptr && GraphPinObj->GetSchema() != nullptr)
    {
        const FScopedTransaction Transaction(LOCTEXT("ClearSchemaReference", "Clear PocketBase schema value"));
        GraphPinObj->Modify();
        GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, FString());
    }
    if (ComboButton.IsValid())
    {
        ComboButton->SetIsOpen(false);
    }
    return FReply::Handled();
}

void SOpenPocketBaseSchemaPin::SetShowSystemCollections(const ECheckBoxState State)
{
    bShowSystemCollections = State == ECheckBoxState::Checked;
    RefreshChoices();
}

void SOpenPocketBaseSchemaPin::SetShowHiddenFields(const ECheckBoxState State)
{
    bShowHiddenFields = State == ECheckBoxState::Checked;
    RefreshChoices();
}

FText SOpenPocketBaseSchemaPin::GetCurrentLabel() const
{
    if (GraphPinObj == nullptr)
    {
        return FText::GetEmpty();
    }

    if (IsCollectionPin())
    {
        FOpenPocketBaseCollectionRef Ref;
        if (!FOpenPocketBaseSchemaPickerModel::ParseCollectionDefault(
                GraphPinObj->GetDefaultAsString(),
                Ref) ||
            !Ref.IsSet())
        {
            return LOCTEXT("ChooseCollectionLabel", "Choose collection");
        }
        FOpenPocketBaseCollectionRef Current;
        return FText::FromString(Ref.ResolveCurrent(Current) ? Current.Name : Ref.Name);
    }

    FOpenPocketBaseFieldRef Ref;
    if (!FOpenPocketBaseSchemaPickerModel::ParseFieldDefault(
            GetReferenceStruct(),
            GraphPinObj->GetDefaultAsString(),
            Ref) ||
        !Ref.IsSet())
    {
        return LOCTEXT("ChooseFieldLabel", "Choose field");
    }

    FOpenPocketBaseFieldRef Current;
    const FOpenPocketBaseFieldRef& DisplayRef = Ref.ResolveCurrent(Current) ? Current : Ref;
    UOpenPocketBaseSchema* Schema = DisplayRef.Schema.LoadSynchronous();
    const FOpenPocketBaseSchemaCollection* Collection =
        Schema != nullptr ? Schema->FindCollection(DisplayRef.CollectionId) : nullptr;
    return FText::FromString(FString::Printf(
        TEXT("%s.%s"),
        Collection != nullptr ? *Collection->Name : TEXT("?"),
        *DisplayRef.Name));
}

FText SOpenPocketBaseSchemaPin::GetCurrentToolTip() const
{
    if (GraphPinObj == nullptr)
    {
        return FText::GetEmpty();
    }

    FText Message;
    if (IsCollectionPin())
    {
        FOpenPocketBaseCollectionRef Ref;
        FOpenPocketBaseSchemaPickerModel::ParseCollectionDefault(
            GraphPinObj->GetDefaultAsString(),
            Ref);
        FOpenPocketBaseSchemaPickerModel::ValidateCollection(
            GetReferenceStruct(),
            Ref,
            OpenPocketBase::Editor::FindCollectionRequirement(*GraphPinObj),
            Message);
    }
    else
    {
        FOpenPocketBaseFieldRef Ref;
        FOpenPocketBaseSchemaPickerModel::ParseFieldDefault(
            GetReferenceStruct(),
            GraphPinObj->GetDefaultAsString(),
            Ref);
        FOpenPocketBaseSchemaPickerModel::ValidateField(
            GetReferenceStruct(),
            Ref,
            GraphPinObj->GetOwningNode()->GetPinMetaData(
                GraphPinObj->PinName,
                TEXT("OpenPocketBaseFieldAccess")) == TEXT("Write"),
            Message);
    }
    return Message.IsEmpty() ? LOCTEXT("SchemaReferenceValid", "PocketBase schema reference") : Message;
}

FSlateColor SOpenPocketBaseSchemaPin::GetCurrentColor() const
{
    if (GraphPinObj == nullptr)
    {
        return FStyleColors::Foreground;
    }

    FText Message;
    EOpenPocketBaseSchemaReferenceStatus Status;
    if (IsCollectionPin())
    {
        FOpenPocketBaseCollectionRef Ref;
        FOpenPocketBaseSchemaPickerModel::ParseCollectionDefault(
            GraphPinObj->GetDefaultAsString(),
            Ref);
        Status = FOpenPocketBaseSchemaPickerModel::ValidateCollection(
            GetReferenceStruct(),
            Ref,
            OpenPocketBase::Editor::FindCollectionRequirement(*GraphPinObj),
            Message);
    }
    else
    {
        FOpenPocketBaseFieldRef Ref;
        FOpenPocketBaseSchemaPickerModel::ParseFieldDefault(
            GetReferenceStruct(),
            GraphPinObj->GetDefaultAsString(),
            Ref);
        Status = FOpenPocketBaseSchemaPickerModel::ValidateField(
            GetReferenceStruct(),
            Ref,
            GraphPinObj->GetOwningNode()->GetPinMetaData(
                GraphPinObj->PinName,
                TEXT("OpenPocketBaseFieldAccess")) == TEXT("Write"),
            Message);
    }

    if (Status == EOpenPocketBaseSchemaReferenceStatus::Valid)
    {
        return FStyleColors::Foreground;
    }
    if (Status == EOpenPocketBaseSchemaReferenceStatus::Empty)
    {
        return FSlateColor::UseSubduedForeground();
    }
    return FStyleColors::Error;
}

EVisibility SOpenPocketBaseSchemaPin::GetEmptyMessageVisibility() const
{
    return FilteredChoices.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SOpenPocketBaseSchemaPin::GetEmptyMessage() const
{
    if (!CurrentSearch.IsEmpty())
    {
        return LOCTEXT("NoMatchingSchemaValues", "No matching schema values.");
    }
    return LOCTEXT(
        "NoImportedSchemas",
        "No choices are available. Import a PocketBase schema JSON file into the Content Browser.");
}

const UScriptStruct* SOpenPocketBaseSchemaPin::GetReferenceStruct() const
{
    return GraphPinObj != nullptr
        ? Cast<UScriptStruct>(GraphPinObj->PinType.PinSubCategoryObject.Get())
        : nullptr;
}

bool SOpenPocketBaseSchemaPin::IsCollectionPin() const
{
    return FOpenPocketBaseSchemaPickerModel::SupportsCollectionStruct(GetReferenceStruct());
}

bool SOpenPocketBaseSchemaPin::FindSiblingCollection(
    FOpenPocketBaseCollectionRef& OutCollection) const
{
    return GraphPinObj != nullptr &&
        OpenPocketBase::Editor::FindCollectionContext(*GraphPinObj, OutCollection);
}

void SOpenPocketBaseSchemaPin::LoadSchemas(
    TArray<UOpenPocketBaseSchema*>& OutSchemas) const
{
    OutSchemas.Reset();
    auto AddSchema = [&OutSchemas](UOpenPocketBaseSchema* Schema)
    {
        if (Schema != nullptr && !OutSchemas.Contains(Schema))
        {
            OutSchemas.Add(Schema);
        }
    };

    if (GraphPinObj != nullptr)
    {
        if (IsCollectionPin())
        {
            FOpenPocketBaseCollectionRef Ref;
            if (FOpenPocketBaseSchemaPickerModel::ParseCollectionDefault(
                    GraphPinObj->GetDefaultAsString(),
                    Ref))
            {
                AddSchema(Ref.Schema.LoadSynchronous());
            }
        }
        else
        {
            FOpenPocketBaseFieldRef Ref;
            if (FOpenPocketBaseSchemaPickerModel::ParseFieldDefault(
                    GetReferenceStruct(),
                    GraphPinObj->GetDefaultAsString(),
                    Ref))
            {
                AddSchema(Ref.Schema.LoadSynchronous());
            }

            FOpenPocketBaseCollectionRef Collection;
            if (FindSiblingCollection(Collection))
            {
                AddSchema(Collection.Schema.LoadSynchronous());
            }
        }
    }

    if (!OutSchemas.IsEmpty())
    {
        return;
    }

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> Assets;
    AssetRegistryModule.Get().GetAssetsByClass(
        UOpenPocketBaseSchema::StaticClass()->GetClassPathName(),
        Assets,
        true);
    TArray<UOpenPocketBaseSchema*> AvailableSchemas;
    for (const FAssetData& Asset : Assets)
    {
        UOpenPocketBaseSchema* Schema = Cast<UOpenPocketBaseSchema>(Asset.GetAsset());
        if (Schema != nullptr)
        {
            AvailableSchemas.AddUnique(Schema);
        }
    }
    FOpenPocketBaseSchemaPickerModel::ChooseProfileSchemas(
        *GetDefault<UOpenPocketBaseProjectSettings>(),
        AvailableSchemas,
        OutSchemas);
}

#undef LOCTEXT_NAMESPACE
