// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "IDetailCustomization.h"

class UOpenPocketBaseSchema;
class IPropertyUtilities;

class FOpenPocketBaseSchemaDetails final : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
    FReply PreviewChanges();
    FReply RefreshSchema();
    FReply GenerateAccessors();
    void ShowNotification(const FText& Message, bool bSuccess) const;

    TWeakObjectPtr<UOpenPocketBaseSchema> Schema;
    TWeakPtr<IPropertyUtilities> PropertyUtilities;
};
