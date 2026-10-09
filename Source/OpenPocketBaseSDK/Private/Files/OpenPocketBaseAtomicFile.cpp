// Copyright 2026 Ishtmeet Singh.

#include "Files/OpenPocketBaseAtomicFile.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#elif PLATFORM_MAC || PLATFORM_IOS
#include <sys/stdio.h>
#else
#include "HAL/PlatformFileManager.h"
#endif

namespace OpenPocketBase::Files
{
bool MoveCompletedDownload(
    const FString& Destination,
    const FString& Temporary,
    const bool bReplaceExisting)
{
#if PLATFORM_WINDOWS
    return ::MoveFileExW(*Temporary, *Destination, bReplaceExisting ? MOVEFILE_REPLACE_EXISTING : 0) != 0;
#elif PLATFORM_MAC || PLATFORM_IOS
    const FTCHARToUTF8 Source(*Temporary);
    const FTCHARToUTF8 Target(*Destination);
    return ::renamex_np(Source.Get(), Target.Get(), bReplaceExisting ? 0 : RENAME_EXCL) == 0;
#else
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    // Preserve the existing destination when the platform has no atomic replacement implementation.
    return !PlatformFile.FileExists(*Destination) && PlatformFile.MoveFile(*Destination, *Temporary);
#endif
}
}
