#pragma once

#include "OpenPocketBaseError.h"
#include "OpenPocketBaseFile.h"
#include "OpenPocketBaseRecord.h"
#include "Serialization/Archive.h"
#include "Templates/SharedPointer.h"

namespace OpenPocketBase::Multipart
{
struct FBuildResult
{
    TSharedPtr<FArchive, ESPMode::ThreadSafe> Stream;
    FString ContentType;
    int64 ContentLength = 0;
    int64 BufferedBytes = 0;
};

OPENPOCKETBASESDK_API bool Build(
    const FOpenPocketBaseRecordBody& Body,
    const TArray<FOpenPocketBaseFileInput>& Files,
    const FOpenPocketBaseUploadLimits& Limits,
    const FString& Boundary,
    FBuildResult& OutResult,
    FOpenPocketBaseError& OutError);
}
