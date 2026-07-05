#include "Files/OpenPocketBaseDownload.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

namespace
{
FOpenPocketBaseError MakeDownloadError(
    const EOpenPocketBaseErrorKind Kind,
    const TCHAR* Message,
    const FString& RequestId = {})
{
    FOpenPocketBaseError Error;
    Error.Kind = Kind;
    Error.ServerMessage = Message;
    Error.RequestId = RequestId;
    return Error;
}

FString FindHeader(const TMap<FString, FString>& Headers, const TCHAR* Name)
{
    for (const TPair<FString, FString>& Header : Headers)
    {
        if (Header.Key.Equals(Name, ESearchCase::IgnoreCase))
        {
            return Header.Value;
        }
    }
    return {};
}

FString SanitizeMetadata(const FString& Value, const int32 MaxLength)
{
    if (Value.Len() > MaxLength)
    {
        return {};
    }
    for (const TCHAR Character : Value)
    {
        if (FChar::IsControl(Character))
        {
            return {};
        }
    }
    return Value;
}

FString SanitizeFileName(const FString& Value)
{
    FString Result = FPaths::GetCleanFilename(Value).Left(128);
    for (TCHAR& Character : Result)
    {
        const bool bSafe = FChar::IsAlnum(Character) || Character == TEXT('.') ||
            Character == TEXT('-') || Character == TEXT('_') || Character == TEXT(' ');
        if (!bSafe)
        {
            Character = TEXT('_');
        }
    }
    return Result.IsEmpty() || Result == TEXT(".") || Result == TEXT("..")
        ? TEXT("download.bin")
        : Result;
}

FString GetFileNameHint(
    const TMap<FString, FString>& Headers,
    const FString& RequestedFileName)
{
    const FString Disposition = FindHeader(Headers, TEXT("Content-Disposition"));
    int32 FilenameOffset = Disposition.Find(TEXT("filename="), ESearchCase::IgnoreCase);
    if (FilenameOffset == INDEX_NONE)
    {
        return SanitizeFileName(RequestedFileName);
    }

    FString Value = Disposition.Mid(FilenameOffset + 9).TrimStartAndEnd();
    int32 Semicolon = INDEX_NONE;
    if (Value.FindChar(TEXT(';'), Semicolon))
    {
        Value.LeftInline(Semicolon, EAllowShrinking::No);
    }
    Value.TrimStartAndEndInline();
    if (Value.Len() >= 2 && Value.StartsWith(TEXT("\"")) && Value.EndsWith(TEXT("\"")))
    {
        Value = Value.Mid(1, Value.Len() - 2);
    }
    return SanitizeFileName(Value);
}
}

TSharedPtr<FOpenPocketBaseDownloadSink, ESPMode::ThreadSafe> FOpenPocketBaseDownloadSink::Create(
    const FOpenPocketBaseFileDownloadOptions& Options,
    const FString& RequestedFileName,
    FOpenPocketBaseError& OutError)
{
    if (Options.MaxBytes < 1 || Options.MaxBytes > 16LL * 1024 * 1024 * 1024 ||
        (Options.Target == EOpenPocketBaseFileDownloadTarget::Memory &&
            Options.MaxBytes > MAX_int32))
    {
        OutError = MakeDownloadError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("The download byte bound is invalid for its destination."));
        return nullptr;
    }

    if (Options.Target == EOpenPocketBaseFileDownloadTarget::Memory)
    {
        OutError = FOpenPocketBaseError();
        return MakeShareable(new FOpenPocketBaseDownloadSink(
            Options.Target,
            Options.MaxBytes,
            {},
            {},
            RequestedFileName,
            false,
            nullptr));
    }

    if (Options.DestinationPath.IsEmpty())
    {
        OutError = MakeDownloadError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("A file download requires an explicit destination path."));
        return nullptr;
    }

    FString DestinationPath = FPaths::ConvertRelativePathToFull(Options.DestinationPath);
    FPaths::NormalizeFilename(DestinationPath);
    const FString ParentPath = FPaths::GetPath(DestinationPath);
    const FString TempPath = DestinationPath + TEXT(".tmp");
    IFileManager& FileManager = IFileManager::Get();
    if (FPaths::GetCleanFilename(DestinationPath).IsEmpty() ||
        !FileManager.DirectoryExists(*ParentPath) ||
        FileManager.DirectoryExists(*DestinationPath) ||
        FileManager.FileExists(*TempPath) ||
        (!Options.bReplaceExisting && FileManager.FileExists(*DestinationPath)))
    {
        OutError = MakeDownloadError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("The download destination is unavailable or already exists."));
        return nullptr;
    }

    TUniquePtr<FArchive> Writer(FileManager.CreateFileWriter(
        *TempPath,
        FILEWRITE_NoReplaceExisting));
    if (!Writer.IsValid())
    {
        OutError = MakeDownloadError(
            EOpenPocketBaseErrorKind::Transport,
            TEXT("The temporary download file could not be opened."));
        return nullptr;
    }

    OutError = FOpenPocketBaseError();
    return MakeShareable(new FOpenPocketBaseDownloadSink(
        Options.Target,
        Options.MaxBytes,
        MoveTemp(DestinationPath),
        TempPath,
        RequestedFileName,
        Options.bReplaceExisting,
        MoveTemp(Writer)));
}

FOpenPocketBaseDownloadSink::FOpenPocketBaseDownloadSink(
    const EOpenPocketBaseFileDownloadTarget InTarget,
    const int64 InMaxBytes,
    FString InDestinationPath,
    FString InTempPath,
    FString InRequestedFileName,
    const bool bInReplaceExisting,
    TUniquePtr<FArchive> InWriter)
    : Target(InTarget)
    , MaxBytes(InMaxBytes)
    , DestinationPath(MoveTemp(InDestinationPath))
    , TempPath(MoveTemp(InTempPath))
    , RequestedFileName(MoveTemp(InRequestedFileName))
    , bReplaceExisting(bInReplaceExisting)
    , Writer(MoveTemp(InWriter))
{
}

FOpenPocketBaseDownloadSink::~FOpenPocketBaseDownloadSink()
{
    Abort();
}

void FOpenPocketBaseDownloadSink::Receive(const TArrayView<const uint8> Chunk)
{
    if (Chunk.IsEmpty())
    {
        return;
    }

    FScopeLock Lock(&Mutex);
    if (bClosed || Failure.IsSet())
    {
        return;
    }
    if (Chunk.Num() > MaxBytes || TransferredBytes > MaxBytes - Chunk.Num())
    {
        Failure = MakeDownloadError(
            EOpenPocketBaseErrorKind::Transport,
            TEXT("The file download exceeded its configured byte bound."));
        return;
    }

    if (Target == EOpenPocketBaseFileDownloadTarget::Memory)
    {
        Bytes.Append(Chunk.GetData(), Chunk.Num());
    }
    else if (Writer.IsValid())
    {
        Writer->Serialize(const_cast<uint8*>(Chunk.GetData()), Chunk.Num());
        if (Writer->IsError())
        {
            Failure = MakeDownloadError(
                EOpenPocketBaseErrorKind::Transport,
                TEXT("The temporary download file could not be written."));
            return;
        }
    }
    else
    {
        Failure = MakeDownloadError(
            EOpenPocketBaseErrorKind::Internal,
            TEXT("The download destination closed unexpectedly."));
        return;
    }
    TransferredBytes += Chunk.Num();
}

int64 FOpenPocketBaseDownloadSink::GetTransferredBytes() const
{
    FScopeLock Lock(&Mutex);
    return TransferredBytes;
}

bool FOpenPocketBaseDownloadSink::Finalize(
    const FOpenPocketBaseHttpResponse& Response,
    FOpenPocketBaseFileDownloadResult& OutResult,
    FOpenPocketBaseError& OutError)
{
    int64 DeclaredContentLength = 0;
    const FString ContentLengthHeader = FindHeader(Response.Headers, TEXT("Content-Length"));
    const FString ContentEncoding = FindHeader(Response.Headers, TEXT("Content-Encoding"));
    const bool bHasDeclaredContentLength = !ContentLengthHeader.IsEmpty() &&
        LexTryParseString(DeclaredContentLength, *ContentLengthHeader) &&
        DeclaredContentLength >= 0 &&
        (ContentEncoding.IsEmpty() || ContentEncoding.Equals(TEXT("identity"), ESearchCase::IgnoreCase));
    FString LocalTempPath;
    {
        FScopeLock Lock(&Mutex);
        if (bClosed)
        {
            OutError = MakeDownloadError(
                EOpenPocketBaseErrorKind::Internal,
                TEXT("The download destination was already closed."),
                Response.RequestId);
            return false;
        }
        bClosed = true;
        Writer.Reset();
        if (!Failure.IsSet() && bHasDeclaredContentLength &&
            DeclaredContentLength != TransferredBytes)
        {
            Failure = MakeDownloadError(
                EOpenPocketBaseErrorKind::Transport,
                TEXT("The file download ended before its declared content length."));
        }
        if (Failure.IsSet())
        {
            Failure.RequestId = Response.RequestId;
            OutError = Failure;
            LocalTempPath = TempPath;
        }
        else
        {
            OutResult.Bytes = MoveTemp(Bytes);
        }
    }

    if (OutError.IsSet())
    {
        if (!LocalTempPath.IsEmpty())
        {
            IFileManager::Get().Delete(*LocalTempPath, false, true, true);
        }
        return false;
    }

    if (Target == EOpenPocketBaseFileDownloadTarget::File &&
        !IFileManager::Get().Move(
            *DestinationPath,
            *TempPath,
            bReplaceExisting,
            false,
            false,
            true))
    {
        IFileManager::Get().Delete(*TempPath, false, true, true);
        OutError = MakeDownloadError(
            EOpenPocketBaseErrorKind::Transport,
            TEXT("The completed download could not be published to its destination."),
            Response.RequestId);
        return false;
    }

    OutResult.bSavedToFile = Target == EOpenPocketBaseFileDownloadTarget::File;
    OutResult.DestinationPath = OutResult.bSavedToFile ? DestinationPath : FString();
    OutResult.HttpStatus = Response.HttpStatus;
    OutResult.ContentLength = TransferredBytes;
    OutResult.ContentType = SanitizeMetadata(FindHeader(Response.Headers, TEXT("Content-Type")), 127);
    OutResult.FileName = GetFileNameHint(Response.Headers, RequestedFileName);
    OutResult.ETag = SanitizeMetadata(FindHeader(Response.Headers, TEXT("ETag")), 512);
    OutResult.LastModified = SanitizeMetadata(FindHeader(Response.Headers, TEXT("Last-Modified")), 128);
    OutError = FOpenPocketBaseError();
    return true;
}

void FOpenPocketBaseDownloadSink::Abort()
{
    FString LocalTempPath;
    {
        FScopeLock Lock(&Mutex);
        if (bClosed)
        {
            return;
        }
        bClosed = true;
        Writer.Reset();
        LocalTempPath = TempPath;
        Bytes.Reset();
    }
    if (!LocalTempPath.IsEmpty())
    {
        IFileManager::Get().Delete(*LocalTempPath, false, true, true);
    }
}
