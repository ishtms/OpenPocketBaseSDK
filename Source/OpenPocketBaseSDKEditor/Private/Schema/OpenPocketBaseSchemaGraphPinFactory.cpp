#include "Schema/OpenPocketBaseSchemaGraphPinFactory.h"

#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "OpenPocketBaseSchemaPicker.h"
#include "Schema/SOpenPocketBaseSchemaPin.h"

TSharedPtr<SGraphPin> FOpenPocketBaseSchemaGraphPinFactory::CreatePin(UEdGraphPin* Pin) const
{
    if (Pin == nullptr || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Struct)
    {
        return nullptr;
    }

    const UScriptStruct* Struct = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get());
    if (!FOpenPocketBaseSchemaPickerModel::SupportsCollectionStruct(Struct) &&
        !FOpenPocketBaseSchemaPickerModel::SupportsFieldStruct(Struct))
    {
        return nullptr;
    }

    return SNew(SOpenPocketBaseSchemaPin, Pin);
}
