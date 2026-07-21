#include "Schema/OpenPocketBaseSchemaDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EditorReimportHandler.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IPropertyUtilities.h"
#include "Misc/MessageDialog.h"
#include "OpenPocketBaseSchema.h"
#include "OpenPocketBaseSchemaCodeGenerator.h"
#include "OpenPocketBaseSchemaDiagnostics.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "OpenPocketBaseSchemaDetails"

TSharedRef<IDetailCustomization> FOpenPocketBaseSchemaDetails::MakeInstance()
{
    return MakeShared<FOpenPocketBaseSchemaDetails>();
}

void FOpenPocketBaseSchemaDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    PropertyUtilities = DetailBuilder.GetPropertyUtilities();
    TArray<TWeakObjectPtr<UObject>> Objects;
    DetailBuilder.GetObjectsBeingCustomized(Objects);
    if (Objects.Num() == 1)
    {
        Schema = Cast<UOpenPocketBaseSchema>(Objects[0].Get());
    }

    IDetailCategoryBuilder& SchemaCategory = DetailBuilder.EditCategory(
        TEXT("Open PocketBase|Schema"),
        LOCTEXT("SchemaCategory", "Open PocketBase Schema"),
        ECategoryPriority::Important);
    SchemaCategory.AddCustomRow(LOCTEXT("SchemaStatusSearch", "Schema Status Validation Refresh Preview"))
        .WholeRowContent()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("SchemaStatus", "Schema Status"))
                .Font(IDetailLayoutBuilder::GetDetailFontBold())
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text_Lambda(
                    [this]()
                    {
                        const UOpenPocketBaseSchema* SchemaAsset = Schema.Get();
                        return SchemaAsset != nullptr
                            ? FText::FromString(
                                  FOpenPocketBaseSchemaDiagnostics::Summarize(*SchemaAsset).ToText())
                            : FText::GetEmpty();
                    })
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("SchemaValidation", "Validation"))
                .Font(IDetailLayoutBuilder::GetDetailFontBold())
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text_Lambda(
                    [this]()
                    {
                        const UOpenPocketBaseSchema* SchemaAsset = Schema.Get();
                        return SchemaAsset != nullptr
                            ? FText::FromString(
                                  FOpenPocketBaseSchemaDiagnostics::Validate(*SchemaAsset).ToText())
                            : FText::GetEmpty();
                    })
                .AutoWrapText(true)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 10.0f, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("PreviewChanges", "Preview Changes"))
                    .ToolTipText(LOCTEXT(
                        "PreviewChangesTooltip",
                        "Reads the source file and shows schema changes without modifying this asset."))
                    .IsEnabled_Lambda(
                        [this]()
                        {
                            const UOpenPocketBaseSchema* SchemaAsset = Schema.Get();
                            return SchemaAsset != nullptr && !SchemaAsset->Source.IsEmpty();
                        })
                    .OnClicked(this, &FOpenPocketBaseSchemaDetails::PreviewChanges)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(8.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("RefreshSchema", "Refresh Schema"))
                    .ToolTipText(LOCTEXT(
                        "RefreshSchemaTooltip",
                        "Previews changes, asks for confirmation, then reimports this schema from its source file."))
                    .IsEnabled_Lambda(
                        [this]()
                        {
                            const UOpenPocketBaseSchema* SchemaAsset = Schema.Get();
                            return SchemaAsset != nullptr && !SchemaAsset->Source.IsEmpty();
                        })
                    .OnClicked(this, &FOpenPocketBaseSchemaDetails::RefreshSchema)
                ]
            ]
        ];

    IDetailCategoryBuilder& CppCategory = DetailBuilder.EditCategory(TEXT("Open PocketBase|C++"));
    CppCategory.AddCustomRow(LOCTEXT("GenerateAccessorsSearch", "Generate C++ Accessors"))
        .WholeRowContent()
        [
            SNew(SButton)
                .Text(LOCTEXT("GenerateAccessors", "Generate C++ Accessors"))
                .ToolTipText(LOCTEXT(
                    "GenerateAccessorsTooltip",
                    "Writes typed collection and field accessors into the project's C++ Source directory."))
                .IsEnabled_Lambda([this]() { return Schema.IsValid(); })
                .OnClicked(this, &FOpenPocketBaseSchemaDetails::GenerateAccessors)
        ];

    DetailBuilder.HideCategory(TEXT("Open PocketBase"));
    DetailBuilder.HideCategory(TEXT("Schema"));
    DetailBuilder.HideCategory(TEXT("C++"));
}

FReply FOpenPocketBaseSchemaDetails::PreviewChanges()
{
    UOpenPocketBaseSchema* SchemaAsset = Schema.Get();
    if (SchemaAsset == nullptr)
    {
        return FReply::Handled();
    }

    UOpenPocketBaseSchema* Preview = nullptr;
    FText Error;
    if (!FOpenPocketBaseSchemaDiagnostics::LoadSourcePreview(
            *SchemaAsset,
            Preview,
            Error))
    {
        ShowNotification(Error, false);
        return FReply::Handled();
    }

    const FOpenPocketBaseSchemaDiff Diff =
        FOpenPocketBaseSchemaDiagnostics::Compare(*SchemaAsset, *Preview);
    FMessageDialog::Open(
        EAppMsgType::Ok,
        FText::FromString(Diff.ToText()),
        LOCTEXT("SchemaChangePreviewTitle", "PocketBase Schema Changes"));
    return FReply::Handled();
}

FReply FOpenPocketBaseSchemaDetails::RefreshSchema()
{
    UOpenPocketBaseSchema* SchemaAsset = Schema.Get();
    if (SchemaAsset == nullptr)
    {
        return FReply::Handled();
    }

    UOpenPocketBaseSchema* Preview = nullptr;
    FText Error;
    if (!FOpenPocketBaseSchemaDiagnostics::LoadSourcePreview(
            *SchemaAsset,
            Preview,
            Error))
    {
        ShowNotification(Error, false);
        return FReply::Handled();
    }

    const FOpenPocketBaseSchemaValidationReport Validation =
        FOpenPocketBaseSchemaDiagnostics::Validate(*Preview);
    if (Validation.HasErrors())
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::FromString(Validation.ToText()),
            LOCTEXT("SchemaValidationFailedTitle", "PocketBase Schema Validation Failed"));
        return FReply::Handled();
    }

    const FOpenPocketBaseSchemaDiff Diff =
        FOpenPocketBaseSchemaDiagnostics::Compare(*SchemaAsset, *Preview);
    if (Diff.IsEmpty())
    {
        ShowNotification(LOCTEXT("SchemaCurrent", "The PocketBase schema is already current."), true);
        return FReply::Handled();
    }

    const FText Confirmation = FText::FromString(
        Diff.ToText() + TEXT("\n\nRefresh this schema asset from its source file?"));
    if (FMessageDialog::Open(
            EAppMsgType::YesNo,
            Confirmation,
            LOCTEXT("RefreshSchemaConfirmationTitle", "Refresh PocketBase Schema")) !=
        EAppReturnType::Yes)
    {
        return FReply::Handled();
    }

    if (!FReimportManager::Instance()->Reimport(
            SchemaAsset,
            false,
            false))
    {
        ShowNotification(LOCTEXT("SchemaRefreshFailed", "PocketBase schema refresh failed."), false);
        return FReply::Handled();
    }

    if (const TSharedPtr<IPropertyUtilities> Utilities = PropertyUtilities.Pin())
    {
        Utilities->ForceRefresh();
    }
    ShowNotification(LOCTEXT("SchemaRefreshed", "PocketBase schema refreshed."), true);
    return FReply::Handled();
}

FReply FOpenPocketBaseSchemaDetails::GenerateAccessors()
{
    UOpenPocketBaseSchema* SchemaAsset = Schema.Get();
    if (SchemaAsset == nullptr)
    {
        return FReply::Handled();
    }

    FString OutputPath = SchemaAsset->GeneratedHeaderPath;
    FText Error;
    if (OutputPath.IsEmpty() &&
        !FOpenPocketBaseSchemaCodeGenerator::FindDefaultOutputPath(*SchemaAsset, OutputPath, Error))
    {
        ShowNotification(Error, false);
        return FReply::Handled();
    }

    FString AbsolutePath;
    if (!FOpenPocketBaseSchemaCodeGenerator::WriteHeader(
            *SchemaAsset,
            SchemaAsset->GeneratedCppNamespace,
            OutputPath,
            AbsolutePath,
            Error))
    {
        ShowNotification(Error, false);
        return FReply::Handled();
    }

    if (SchemaAsset->GeneratedHeaderPath != OutputPath)
    {
        SchemaAsset->Modify();
        SchemaAsset->GeneratedHeaderPath = OutputPath;
        SchemaAsset->MarkPackageDirty();
    }
    ShowNotification(
        FText::Format(
            LOCTEXT("GeneratedAccessorsAt", "Generated PocketBase C++ accessors at {0}"),
            FText::FromString(AbsolutePath)),
        true);
    return FReply::Handled();
}

void FOpenPocketBaseSchemaDetails::ShowNotification(const FText& Message, const bool bSuccess) const
{
    FNotificationInfo Info(Message);
    Info.ExpireDuration = 6.0f;
    Info.bUseSuccessFailIcons = true;
    TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
    if (Item.IsValid())
    {
        Item->SetCompletionState(
            bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
    }
}

#undef LOCTEXT_NAMESPACE
