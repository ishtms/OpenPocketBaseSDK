#pragma once

#include "OpenPocketBaseSchema.h"
#include "OpenPocketBaseSchemaPicker.h"

class UEdGraphPin;

namespace OpenPocketBase::Editor
{
bool FindCollectionContext(
    const UEdGraphPin& OriginPin,
    FOpenPocketBaseCollectionRef& OutCollection);

EOpenPocketBaseCollectionRequirement FindCollectionRequirement(
    const UEdGraphPin& OriginPin);
}
