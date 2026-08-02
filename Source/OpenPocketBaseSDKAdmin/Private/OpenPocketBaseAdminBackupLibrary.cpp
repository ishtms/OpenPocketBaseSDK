#include "OpenPocketBaseAdminBackupLibrary.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace
{
bool FailBackupSave(
    FOpenPocketBaseError& Error,
    const EOpenPocketBaseErrorKind Kind,
    const TCHAR* Message)
{
    Error = {};
    Error.Kind = Kind;
    Error.Message = Message;
    return false;
}
}

bool UOpenPocketBaseAdminBackupLibrary::SaveBackupDownloadToFile(
    const FOpenPocketBaseAdminBackupDownload& Download,
    FString DestinationPath,
    const bool bReplaceExisting,
    FString& SavedPath,
    FOpenPocketBaseError& Error)
{
    SavedPath.Reset();
    Error = {};

    if (Download.Bytes.IsEmpty())
    {
        return FailBackupSave(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Backup Download Bytes is empty. Download a backup before saving it to disk."));
    }
    if (DestinationPath.IsEmpty())
    {
        return FailBackupSave(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Saving a backup requires an explicit destination path."));
    }

    DestinationPath = FPaths::ConvertRelativePathToFull(DestinationPath);
    FPaths::NormalizeFilename(DestinationPath);
    const FString ParentPath = FPaths::GetPath(DestinationPath);
    const FString TempPath = DestinationPath + TEXT(".tmp");
    IFileManager& FileManager = IFileManager::Get();

    if (FPaths::GetCleanFilename(DestinationPath).IsEmpty())
    {
        return FailBackupSave(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Destination Path must include a file name."));
    }
    if (!FileManager.DirectoryExists(*ParentPath))
    {
        return FailBackupSave(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("The parent directory for Destination Path does not exist. Create the directory before saving the backup."));
    }
    if (FileManager.DirectoryExists(*DestinationPath))
    {
        return FailBackupSave(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Destination Path points to a directory. Choose a full file path instead."));
    }
    if (FileManager.FileExists(*TempPath))
    {
        return FailBackupSave(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("A temporary backup file already exists at Destination Path. Remove the stale .tmp file and try again."));
    }
    if (!bReplaceExisting && FileManager.FileExists(*DestinationPath))
    {
        return FailBackupSave(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("A file already exists at Destination Path. Enable Replace Existing or choose another destination."));
    }

    TUniquePtr<FArchive> Writer(FileManager.CreateFileWriter(
        *TempPath,
        FILEWRITE_NoReplaceExisting));
    if (!Writer.IsValid())
    {
        return FailBackupSave(
            Error,
            EOpenPocketBaseErrorKind::Transport,
            TEXT("The temporary backup file could not be opened. Check that the destination directory is writable and try again."));
    }

    Writer->Serialize(
        const_cast<uint8*>(Download.Bytes.GetData()),
        Download.Bytes.Num());
    const bool bClosed = Writer->Close();
    const bool bWriteFailed = !bClosed || Writer->IsError();
    Writer.Reset();
    if (bWriteFailed)
    {
        FileManager.Delete(*TempPath, false, true, true);
        return FailBackupSave(
            Error,
            EOpenPocketBaseErrorKind::Transport,
            TEXT("The temporary backup file could not be written. Check available disk space and directory permissions."));
    }

    if (!FileManager.Move(
        *DestinationPath,
        *TempPath,
        bReplaceExisting,
        false,
        false,
        true))
    {
        FileManager.Delete(*TempPath, false, true, true);
        return FailBackupSave(
            Error,
            EOpenPocketBaseErrorKind::Transport,
            TEXT("The completed backup could not replace or move to Destination Path. Check file permissions and whether another process is using the file."));
    }

    SavedPath = MoveTemp(DestinationPath);
    return true;
}

bool UOpenPocketBaseAdminBackupLibrary::DeleteSavedBackupFile(
    FString SavedPath,
    FOpenPocketBaseError& Error)
{
    Error = {};
    if (SavedPath.IsEmpty())
    {
        return FailBackupSave(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Saved Backup Path is empty. Pass the path returned by Save Backup Download to File."));
    }

    SavedPath = FPaths::ConvertRelativePathToFull(SavedPath);
    FPaths::NormalizeFilename(SavedPath);
    FString SavedDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    FPaths::NormalizeDirectoryName(SavedDirectory);
    const FString FileName = FPaths::GetCleanFilename(SavedPath);
    if (!FPaths::IsUnderDirectory(SavedPath, SavedDirectory) ||
        (!FileName.EndsWith(TEXT(".zip"), ESearchCase::IgnoreCase) &&
         !FileName.EndsWith(TEXT(".zip.tmp"), ESearchCase::IgnoreCase)))
    {
        return FailBackupSave(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Saved Backup Path must name a .zip or .zip.tmp file under the project Saved directory."));
    }

    IFileManager& FileManager = IFileManager::Get();
    if (FileManager.DirectoryExists(*SavedPath))
    {
        return FailBackupSave(
            Error,
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Saved Backup Path points to a directory, not a backup file."));
    }
    if (!FileManager.FileExists(*SavedPath))
    {
        return true;
    }
    if (!FileManager.Delete(*SavedPath, false, true, true))
    {
        return FailBackupSave(
            Error,
            EOpenPocketBaseErrorKind::Transport,
            TEXT("The saved backup file could not be deleted. Check file permissions and whether another process is using it."));
    }
    return true;
}
