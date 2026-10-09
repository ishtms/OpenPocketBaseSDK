// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"

namespace OpenPocketBase::Files
{
OPENPOCKETBASESDK_API bool MoveCompletedDownload(
    const FString& Destination,
    const FString& Temporary,
    bool bReplaceExisting);
}
