#pragma once

#include "OpenPocketBaseError.h"
#include "OpenPocketBaseFile.h"
#include "Transport/OpenPocketBaseTransport.h"

class FOpenPocketBaseDownloadSink final
{
public:
    static TSharedPtr<FOpenPocketBaseDownloadSink, ESPMode::ThreadSafe> Create(
        const FOpenPocketBaseFileDownloadOptions& Options,
        const FString& RequestedFileName,
        FOpenPocketBaseError& OutError);

    ~FOpenPocketBaseDownloadSink();

    void Receive(TArrayView<const uint8> Chunk);
    int64 GetTransferredBytes() const;
    bool Finalize(
        const FOpenPocketBaseHttpResponse& Response,
        FOpenPocketBaseFileDownloadResult& OutResult,
        FOpenPocketBaseError& OutError);
    void Abort();

private:
    FOpenPocketBaseDownloadSink(
        EOpenPocketBaseFileDownloadTarget InTarget,
        int64 InMaxBytes,
        FString InDestinationPath,
        FString InTempPath,
        FString InRequestedFileName,
        bool bInReplaceExisting,
        TUniquePtr<FArchive> InWriter);

    mutable FCriticalSection Mutex;
    EOpenPocketBaseFileDownloadTarget Target = EOpenPocketBaseFileDownloadTarget::Memory;
    int64 MaxBytes = 0;
    int64 TransferredBytes = 0;
    FString DestinationPath;
    FString TempPath;
    FString RequestedFileName;
    bool bReplaceExisting = false;
    bool bClosed = false;
    TUniquePtr<FArchive> Writer;
    TArray<uint8> Bytes;
    FOpenPocketBaseError Failure;
};
