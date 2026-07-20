#include "Schema/OpenPocketBaseSchemaGraphPinFactory.h"

#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "OpenPocketBaseSchemaPicker.h"
#include "OpenPocketBaseRecord.h"
#include "Schema/SOpenPocketBaseMultiSelectChoicePin.h"
#include "Schema/SOpenPocketBaseSchemaPin.h"
#include "Schema/SOpenPocketBaseSelectChoicePin.h"

TSharedPtr<SGraphPin> FOpenPocketBaseSchemaGraphPinFactory::CreatePin(UEdGraphPin* Pin) const
{
    if (Pin == nullptr)
    {
        return nullptr;
    }

    if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_String &&
        Pin->GetOwningNode() != nullptr &&
        !Pin->GetOwningNode()->GetPinMetaData(
            Pin->PinName,
            TEXT("OpenPocketBaseSelectField")).IsEmpty())
    {
        return SNew(SOpenPocketBaseSelectChoicePin, Pin);
    }
    if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Struct)
    {
        return nullptr;
    }

    const UScriptStruct* Struct = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get());
    if (Struct == FOpenPocketBaseSelectValues::StaticStruct() &&
        Pin->GetOwningNode() != nullptr &&
        !Pin->GetOwningNode()->GetPinMetaData(
            Pin->PinName,
            TEXT("OpenPocketBaseSelectField")).IsEmpty())
    {
        return SNew(SOpenPocketBaseMultiSelectChoicePin, Pin);
    }
    if (!FOpenPocketBaseSchemaPickerModel::SupportsCollectionStruct(Struct) &&
        !FOpenPocketBaseSchemaPickerModel::SupportsFieldStruct(Struct))
    {
        return nullptr;
    }

    return SNew(SOpenPocketBaseSchemaPin, Pin);
}
