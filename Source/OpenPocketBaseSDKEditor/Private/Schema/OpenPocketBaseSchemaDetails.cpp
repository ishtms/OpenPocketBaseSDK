#include "Schema/OpenPocketBaseSchemaDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/Notifications/NotificationManager.h"
#include "OpenPocketBaseSchema.h"
#include "OpenPocketBaseSchemaCodeGenerator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "OpenPocketBaseSchemaDetails"

TSharedRef<IDetailCustomization> FOpenPocketBaseSchemaDetails::MakeInstance()
{
    return MakeShared<FOpenPocketBaseSchemaDetails>();
}

void FOpenPocketBaseSchemaDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    TArray<TWeakObjectPtr<UObject>> Objects;
    DetailBuilder.GetObjectsBeingCustomized(Objects);
    if (Objects.Num() == 1)
    {
        Schema = Cast<UOpenPocketBaseSchema>(Objects[0].Get());
    }

    IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("Open PocketBase|C++"));
    Category.AddCustomRow(LOCTEXT("GenerateAccessorsSearch", "Generate C++ Accessors"))
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
