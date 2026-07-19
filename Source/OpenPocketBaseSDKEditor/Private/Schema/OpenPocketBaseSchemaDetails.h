#pragma once

#include "IDetailCustomization.h"

class UOpenPocketBaseSchema;

class FOpenPocketBaseSchemaDetails final : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
    FReply GenerateAccessors();
    void ShowNotification(const FText& Message, bool bSuccess) const;

    TWeakObjectPtr<UOpenPocketBaseSchema> Schema;
};
