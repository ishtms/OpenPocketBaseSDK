#pragma once

#include "CoreMinimal.h"

namespace OpenPocketBase::Date
{
OPENPOCKETBASESDK_API bool TryParse(const FString& Value, FDateTime& OutDateTime);
OPENPOCKETBASESDK_API FString Format(const FDateTime& Value);
}
