#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenPocketBaseAdminBackupLibrary.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseAdminBackupSaveTest,
    "OpenPocketBase.Admin.BackupDownloadSavesAtomically",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseAdminBackupSaveTest::RunTest(const FString& Parameters)
{
    const UFunction* SaveFunction = UOpenPocketBaseAdminBackupLibrary::StaticClass()
        ->FindFunctionByName(TEXT("SaveBackupDownloadToFile"));
    TestNotNull(TEXT("The backup save helper is exposed"), SaveFunction);
    if (SaveFunction != nullptr)
    {
        TestTrue(
            TEXT("The backup save helper is development only"),
            SaveFunction->HasMetaData(TEXT("DevelopmentOnly")));
        TestEqual(
            TEXT("The helper exposes explicit success and failure paths"),
            SaveFunction->GetMetaData(TEXT("ExpandBoolAsExecs")),
            FString(TEXT("ReturnValue")));
    }

    const UFunction* DeleteFunction = UOpenPocketBaseAdminBackupLibrary::StaticClass()
        ->FindFunctionByName(TEXT("DeleteSavedBackupFile"));
    TestNotNull(TEXT("The backup cleanup helper is exposed"), DeleteFunction);
    if (DeleteFunction != nullptr)
    {
        TestTrue(
            TEXT("The backup cleanup helper is development only"),
            DeleteFunction->HasMetaData(TEXT("DevelopmentOnly")));
        TestEqual(
            TEXT("The cleanup helper exposes explicit success and failure paths"),
            DeleteFunction->GetMetaData(TEXT("ExpandBoolAsExecs")),
            FString(TEXT("ReturnValue")));
    }

    const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectIntermediateDir(),
        TEXT("OpenPocketBaseAdminBackupSave"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    TestTrue(
        TEXT("The isolated destination directory is created"),
        IFileManager::Get().MakeDirectory(*Directory, true));

    FOpenPocketBaseAdminBackupDownload Download;
    Download.Bytes = {0x50, 0x4b, 0x03, 0x04, 0x11, 0x22, 0x33, 0x44};
    Download.ContentType = TEXT("application/zip");

    const FString Destination = FPaths::Combine(Directory, TEXT("backup.zip"));
    FString SavedPath = TEXT("old-path");
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::Authentication;
    Error.Message = TEXT("old-error");
    TestTrue(
        TEXT("Backup bytes save to an explicit path"),
        UOpenPocketBaseAdminBackupLibrary::SaveBackupDownloadToFile(
            Download, Destination, false, SavedPath, Error));
    TestFalse(TEXT("A successful save has no error"), Error.IsSet());
    FString ExpectedSavedPath = FPaths::ConvertRelativePathToFull(Destination);
    FPaths::NormalizeFilename(ExpectedSavedPath);
    TestEqual(
        TEXT("The helper returns the normalized absolute path"),
        SavedPath,
        ExpectedSavedPath);
    TArray<uint8> SavedBytes;
    TestTrue(TEXT("The saved backup can be read"), FFileHelper::LoadFileToArray(SavedBytes, *SavedPath));
    TestEqual(TEXT("The disk bytes exactly match memory"), SavedBytes, Download.Bytes);
    TestFalse(
        TEXT("A successful save leaves no temporary file"),
        IFileManager::Get().FileExists(*(SavedPath + TEXT(".tmp"))));

    FOpenPocketBaseAdminBackupDownload Replacement;
    Replacement.Bytes = {0x50, 0x4b, 0x05, 0x06};
    Replacement.ContentType = TEXT("application/zip");
    SavedPath = TEXT("old-path");
    Error = {};
    TestFalse(
        TEXT("An existing file is rejected when replacement is disabled"),
        UOpenPocketBaseAdminBackupLibrary::SaveBackupDownloadToFile(
            Replacement, Destination, false, SavedPath, Error));
    TestEqual(
        TEXT("Existing-file rejection is InvalidArgument"),
        Error.Kind,
        EOpenPocketBaseErrorKind::InvalidArgument);
    TestTrue(
        TEXT("Existing-file rejection explains replacement"),
        Error.Message.Contains(TEXT("Replace Existing")));
    TestEqual(TEXT("Existing-file rejection has no HTTP status"), Error.HttpStatus, 0);
    TestFalse(TEXT("Existing-file rejection is nonretryable"), Error.bMayRetry);
    TestTrue(TEXT("Existing-file rejection has no request ID"), Error.RequestId.IsEmpty());
    TestTrue(TEXT("Existing-file rejection has no field errors"), Error.FieldErrors.IsEmpty());
    TestTrue(TEXT("A failed save clears the returned path"), SavedPath.IsEmpty());
    SavedBytes.Reset();
    TestTrue(TEXT("The original file remains readable"), FFileHelper::LoadFileToArray(SavedBytes, *Destination));
    TestEqual(TEXT("Rejected replacement preserves original bytes"), SavedBytes, Download.Bytes);

    Error = {};
    TestTrue(
        TEXT("An existing file is replaced when replacement is enabled"),
        UOpenPocketBaseAdminBackupLibrary::SaveBackupDownloadToFile(
            Replacement, Destination, true, SavedPath, Error));
    SavedBytes.Reset();
    TestTrue(TEXT("The replacement can be read"), FFileHelper::LoadFileToArray(SavedBytes, *Destination));
    TestEqual(TEXT("Replacement writes exact bytes"), SavedBytes, Replacement.Bytes);
    TestFalse(
        TEXT("Replacement leaves no temporary file"),
        IFileManager::Get().FileExists(*(Destination + TEXT(".tmp"))));

    const auto ExpectInvalid = [this, &Replacement](
        const TCHAR* Label,
        const FString& Path,
        const FString& MessagePart)
    {
        FString OutPath = TEXT("old-path");
        FOpenPocketBaseError OutError;
        TestFalse(
            Label,
            UOpenPocketBaseAdminBackupLibrary::SaveBackupDownloadToFile(
                Replacement, Path, false, OutPath, OutError));
        TestEqual(
            *FString::Printf(TEXT("%s is InvalidArgument"), Label),
            OutError.Kind,
            EOpenPocketBaseErrorKind::InvalidArgument);
        TestTrue(
            *FString::Printf(TEXT("%s has a useful message"), Label),
            OutError.Message.Contains(MessagePart));
        TestEqual(
            *FString::Printf(TEXT("%s has no HTTP status"), Label),
            OutError.HttpStatus,
            0);
        TestFalse(
            *FString::Printf(TEXT("%s is nonretryable"), Label),
            OutError.bMayRetry);
        TestTrue(
            *FString::Printf(TEXT("%s has no code"), Label),
            OutError.Code.IsEmpty());
        TestTrue(
            *FString::Printf(TEXT("%s has no request ID"), Label),
            OutError.RequestId.IsEmpty());
        TestTrue(
            *FString::Printf(TEXT("%s has no field errors"), Label),
            OutError.FieldErrors.IsEmpty());
        TestTrue(
            *FString::Printf(TEXT("%s clears the returned path"), Label),
            OutPath.IsEmpty());
    };

    ExpectInvalid(TEXT("An empty destination is rejected"), {}, TEXT("explicit destination"));
    ExpectInvalid(
        TEXT("A missing parent is rejected"),
        FPaths::Combine(Directory, TEXT("missing"), TEXT("backup.zip")),
        TEXT("parent directory"));
    ExpectInvalid(
        TEXT("A directory destination is rejected"),
        Directory,
        TEXT("points to a directory"));

    const FString OwnedTempDestination = FPaths::Combine(Directory, TEXT("owned-temp.zip"));
    const FString OwnedTempPath = OwnedTempDestination + TEXT(".tmp");
    const TArray<uint8> OwnedTempBytes = {9, 8, 7};
    TestTrue(
        TEXT("A foreign temporary file is created"),
        FFileHelper::SaveArrayToFile(OwnedTempBytes, *OwnedTempPath));
    ExpectInvalid(
        TEXT("A foreign temporary file is not overwritten"),
        OwnedTempDestination,
        TEXT("temporary backup file already exists"));
    TArray<uint8> PreservedTempBytes;
    TestTrue(
        TEXT("The foreign temporary file remains readable"),
        FFileHelper::LoadFileToArray(PreservedTempBytes, *OwnedTempPath));
    TestEqual(
        TEXT("The foreign temporary file remains unchanged"),
        PreservedTempBytes,
        OwnedTempBytes);

    FOpenPocketBaseAdminBackupDownload EmptyDownload;
    SavedPath = TEXT("old-path");
    Error = {};
    TestFalse(
        TEXT("Empty backup bytes are rejected"),
        UOpenPocketBaseAdminBackupLibrary::SaveBackupDownloadToFile(
            EmptyDownload,
            FPaths::Combine(Directory, TEXT("empty.zip")),
            false,
            SavedPath,
            Error));
    TestEqual(
        TEXT("Empty bytes are InvalidArgument"),
        Error.Kind,
        EOpenPocketBaseErrorKind::InvalidArgument);
    TestTrue(TEXT("Empty bytes have a useful message"), Error.Message.Contains(TEXT("empty")));
    TestEqual(TEXT("Empty bytes have no HTTP status"), Error.HttpStatus, 0);
    TestFalse(TEXT("Empty bytes are nonretryable"), Error.bMayRetry);
    TestTrue(TEXT("Empty bytes have no request ID"), Error.RequestId.IsEmpty());
    TestTrue(TEXT("Empty bytes have no field errors"), Error.FieldErrors.IsEmpty());
    TestTrue(TEXT("Empty bytes clear the returned path"), SavedPath.IsEmpty());

    const FString CleanupDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("OpenPocketBaseAdminBackupSave"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    TestTrue(
        TEXT("The cleanup destination directory is created"),
        IFileManager::Get().MakeDirectory(*CleanupDirectory, true));
    const FString CleanupPath = FPaths::Combine(CleanupDirectory, TEXT("cleanup.zip"));
    TestTrue(
        TEXT("A backup is staged under the project Saved directory"),
        UOpenPocketBaseAdminBackupLibrary::SaveBackupDownloadToFile(
            Replacement, CleanupPath, false, SavedPath, Error));
    TestTrue(
        TEXT("The saved backup cleanup succeeds"),
        UOpenPocketBaseAdminBackupLibrary::DeleteSavedBackupFile(SavedPath, Error));
    TestFalse(
        TEXT("The cleanup removes the exact saved file"),
        IFileManager::Get().FileExists(*CleanupPath));
    TestTrue(
        TEXT("Repeated cleanup is idempotent"),
        UOpenPocketBaseAdminBackupLibrary::DeleteSavedBackupFile(CleanupPath, Error));

    Error = {};
    TestFalse(
        TEXT("Cleanup rejects an empty path"),
        UOpenPocketBaseAdminBackupLibrary::DeleteSavedBackupFile({}, Error));
    TestEqual(
        TEXT("Empty cleanup paths are InvalidArgument"),
        Error.Kind,
        EOpenPocketBaseErrorKind::InvalidArgument);
    Error = {};
    TestFalse(
        TEXT("Cleanup rejects files outside Project Saved"),
        UOpenPocketBaseAdminBackupLibrary::DeleteSavedBackupFile(Destination, Error));
    TestEqual(
        TEXT("Outside cleanup paths are InvalidArgument"),
        Error.Kind,
        EOpenPocketBaseErrorKind::InvalidArgument);

    IFileManager::Get().DeleteDirectory(*CleanupDirectory, false, true);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

#endif
