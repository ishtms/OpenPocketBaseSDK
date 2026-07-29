#include "Files/OpenPocketBaseMultipart.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/OpenPocketBaseJson.h"

namespace
{
FOpenPocketBaseError MakeMultipartError(const FString& Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
    Error.Message = Message;
    return Error;
}

bool IsSafeBoundary(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() > 70)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('-') && Character != TEXT('_'))
        {
            return false;
        }
    }
    return true;
}

bool IsSafeFieldName(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() > 255)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-') &&
            Character != TEXT('.') && Character != TEXT('+'))
        {
            return false;
        }
    }
    return true;
}

bool IsSafeContentType(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() > 127 || !Value.Contains(TEXT("/")))
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        const bool bPunctuation = Character == TEXT('/') || Character == TEXT('-') ||
            Character == TEXT('+') || Character == TEXT('.') || Character == TEXT(';') ||
            Character == TEXT('=') || Character == TEXT(' ');
        if (!FChar::IsAlnum(Character) && !bPunctuation)
        {
            return false;
        }
    }
    return true;
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
    if (Result.IsEmpty() || Result == TEXT(".") || Result == TEXT(".."))
    {
        return TEXT("file.bin");
    }
    return Result;
}

TArray<uint8> ToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

struct FMultipartSegment
{
    TArray<uint8> Memory;
    TSharedPtr<FArchive, ESPMode::ThreadSafe> File;
    int64 Length = 0;
};

class FSegmentedMultipartArchive final : public FArchive
{
public:
    explicit FSegmentedMultipartArchive(TArray<FMultipartSegment> InSegments)
        : Segments(MoveTemp(InSegments))
    {
        SetIsLoading(true);
        SetIsPersistent(true);
        for (const FMultipartSegment& Segment : Segments)
        {
            TotalLength += Segment.Length;
        }
    }

    virtual void Serialize(void* Data, int64 Num) override
    {
        FScopeLock Lock(&Mutex);
        if (Num < 0 || Position < 0 || Position > TotalLength || Num > TotalLength - Position)
        {
            SetError();
            return;
        }

        uint8* Output = static_cast<uint8*>(Data);
        int64 Remaining = Num;
        while (Remaining > 0)
        {
            int32 SegmentIndex = 0;
            int64 SegmentOffset = Position;
            while (Segments.IsValidIndex(SegmentIndex) &&
                SegmentOffset >= Segments[SegmentIndex].Length)
            {
                SegmentOffset -= Segments[SegmentIndex].Length;
                ++SegmentIndex;
            }
            if (!Segments.IsValidIndex(SegmentIndex))
            {
                SetError();
                return;
            }

            FMultipartSegment& Segment = Segments[SegmentIndex];
            const int64 CopyLength = FMath::Min(Remaining, Segment.Length - SegmentOffset);
            if (Segment.File.IsValid())
            {
                Segment.File->Seek(SegmentOffset);
                Segment.File->Serialize(Output, CopyLength);
                if (Segment.File->IsError())
                {
                    SetError();
                    return;
                }
            }
            else
            {
                FMemory::Memcpy(
                    Output,
                    Segment.Memory.GetData() + static_cast<int32>(SegmentOffset),
                    static_cast<SIZE_T>(CopyLength));
            }
            Output += CopyLength;
            Position += CopyLength;
            Remaining -= CopyLength;
        }
    }

    virtual int64 Tell() override
    {
        FScopeLock Lock(&Mutex);
        return Position;
    }

    virtual int64 TotalSize() override
    {
        return TotalLength;
    }

    virtual void Seek(const int64 InPosition) override
    {
        FScopeLock Lock(&Mutex);
        if (InPosition < 0 || InPosition > TotalLength)
        {
            SetError();
            return;
        }
        Position = InPosition;
    }

private:
    FCriticalSection Mutex;
    TArray<FMultipartSegment> Segments;
    int64 Position = 0;
    int64 TotalLength = 0;
};

bool TryAddLength(
    const int64 Addition,
    const FOpenPocketBaseUploadLimits& Limits,
    int64& InOutTotal)
{
    if (Addition < 0 || Addition > Limits.MaxTotalBodyBytes ||
        InOutTotal > Limits.MaxTotalBodyBytes - Addition)
    {
        return false;
    }
    InOutTotal += Addition;
    return true;
}

bool ValidateMultipartLimits(
    const int32 FileCount,
    const FOpenPocketBaseUploadLimits& Limits,
    const FString& Boundary,
    FOpenPocketBaseError& OutError)
{
    if (Boundary.IsEmpty())
    {
        OutError = MakeMultipartError(TEXT("Multipart Boundary is empty. Use a unique boundary containing letters, numbers, hyphens, or underscores."));
        return false;
    }
    if (!IsSafeBoundary(Boundary))
    {
        OutError = MakeMultipartError(TEXT("Multipart Boundary must be 1 to 70 characters and contain only letters, numbers, hyphens, or underscores."));
        return false;
    }
    if (Limits.MaxFiles < 1 || Limits.MaxFiles > 100)
    {
        OutError = MakeMultipartError(FString::Printf(
            TEXT("Max Files is %d. Use a value from 1 to 100."),
            Limits.MaxFiles));
        return false;
    }
    if (FileCount > Limits.MaxFiles)
    {
        OutError = MakeMultipartError(FString::Printf(
            TEXT("The upload contains %d files, but Max Files is %d. Remove files or raise Max Files."),
            FileCount,
            Limits.MaxFiles));
        return false;
    }
    if (Limits.MaxInlineFileBytes < 1 || Limits.MaxInlineFileBytes > 64LL * 1024 * 1024)
    {
        OutError = MakeMultipartError(FString::Printf(
            TEXT("Max Inline File Bytes is %lld. Use a value from 1 to 67108864 bytes."),
            Limits.MaxInlineFileBytes));
        return false;
    }
    if (Limits.MaxSourceFileBytes < 1 || Limits.MaxSourceFileBytes > 16LL * 1024 * 1024 * 1024)
    {
        OutError = MakeMultipartError(FString::Printf(
            TEXT("Max Source File Bytes is %lld. Use a value from 1 to 17179869184 bytes."),
            Limits.MaxSourceFileBytes));
        return false;
    }
    if (Limits.MaxTotalBodyBytes < 1 || Limits.MaxTotalBodyBytes > 16LL * 1024 * 1024 * 1024)
    {
        OutError = MakeMultipartError(FString::Printf(
            TEXT("Max Total Body Bytes is %lld. Use a value from 1 to 17179869184 bytes."),
            Limits.MaxTotalBodyBytes));
        return false;
    }
    return true;
}

FOpenPocketBaseError MakeMultipartBodyLimitError(const int64 MaxTotalBodyBytes)
{
    return MakeMultipartError(FString::Printf(
        TEXT("The multipart request exceeds Max Total Body Bytes of %lld. Remove data or raise the limit."),
        MaxTotalBodyBytes));
}
}

namespace OpenPocketBase::Multipart
{
bool Build(
    const FOpenPocketBaseRecordBody& Body,
    const TArray<FOpenPocketBaseFileInput>& Files,
    const FOpenPocketBaseUploadLimits& Limits,
    const FString& Boundary,
    FBuildResult& OutResult,
    FOpenPocketBaseError& OutError)
{
    OutResult = FBuildResult();
    if (!Body.Data.JsonObject.IsValid())
    {
        OutError = MakeMultipartError(TEXT("The record body has no valid JSON data. Start with New Record Body before adding file fields."));
        return false;
    }
    if (Files.IsEmpty())
    {
        OutError = MakeMultipartError(TEXT("The multipart upload contains no files. Add at least one file input."));
        return false;
    }
    if (!ValidateMultipartLimits(Files.Num(), Limits, Boundary, OutError))
    {
        return false;
    }

    TArray<FMultipartSegment> Segments;
    int64 TotalLength = 0;
    int64 BufferedBytes = 0;
    const auto AddMemory = [&Segments, &TotalLength, &BufferedBytes, &Limits](TArray<uint8> Bytes)
    {
        if (!TryAddLength(Bytes.Num(), Limits, TotalLength))
        {
            return false;
        }
        BufferedBytes += Bytes.Num();
        FMultipartSegment Segment;
        Segment.Length = Bytes.Num();
        Segment.Memory = MoveTemp(Bytes);
        Segments.Add(MoveTemp(Segment));
        return true;
    };

    const TArray<uint8> JsonPayload = OpenPocketBase::Json::SerializeObject(
        Body.Data.JsonObject.ToSharedRef());
    const FString JsonHeader = FString::Printf(
        TEXT("--%s\r\nContent-Disposition: form-data; name=\"@jsonPayload\"\r\n")
        TEXT("Content-Type: application/json\r\n\r\n"),
        *Boundary);
    if (!AddMemory(ToUtf8(JsonHeader)) || !AddMemory(JsonPayload) ||
        !AddMemory(ToUtf8(TEXT("\r\n"))))
    {
        OutError = MakeMultipartBodyLimitError(Limits.MaxTotalBodyBytes);
        return false;
    }

    for (int32 FileIndex = 0; FileIndex < Files.Num(); ++FileIndex)
    {
        const FOpenPocketBaseFileInput& File = Files[FileIndex];
        const FString ModifiedField = FOpenPocketBaseRecordBody::MakeModifiedFieldName(
            File.GetFieldName(),
            File.Modifier);
        if (!IsSafeFieldName(ModifiedField))
        {
            OutError = MakeMultipartError(FString::Printf(
                TEXT("File %d has an invalid field name. Choose a file field from the selected collection."),
                FileIndex + 1));
            return false;
        }
        if (!IsSafeContentType(File.ContentType))
        {
            OutError = MakeMultipartError(FString::Printf(
                TEXT("File %d has an invalid Content Type. Use a MIME type such as image/png or application/octet-stream."),
                FileIndex + 1));
            return false;
        }

        const FString FileName = File.FileName.IsEmpty() && File.bUseFilePath
            ? SanitizeFileName(File.FilePath)
            : SanitizeFileName(File.FileName);
        const FString Header = FString::Printf(
            TEXT("--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n")
            TEXT("Content-Type: %s\r\n\r\n"),
            *Boundary,
            *ModifiedField,
            *FileName,
            *File.ContentType);
        if (!AddMemory(ToUtf8(Header)))
        {
            OutError = MakeMultipartBodyLimitError(Limits.MaxTotalBodyBytes);
            return false;
        }

        FMultipartSegment ContentSegment;
        if (File.bUseFilePath)
        {
            if (File.FilePath.IsEmpty())
            {
                OutError = MakeMultipartError(FString::Printf(
                    TEXT("File %d uses a file path, but File Path is empty."),
                    FileIndex + 1));
                return false;
            }
            const int64 FileSize = IFileManager::Get().FileSize(*File.FilePath);
            if (FileSize < 0)
            {
                OutError = MakeMultipartError(FString::Printf(
                    TEXT("File %d could not be found at File Path '%s'. Choose an existing readable file before starting the upload."),
                    FileIndex + 1,
                    *File.FilePath));
                return false;
            }
            if (FileSize > Limits.MaxSourceFileBytes)
            {
                OutError = MakeMultipartError(FString::Printf(
                    TEXT("File %d is %lld bytes, but Max Source File Bytes is %lld."),
                    FileIndex + 1,
                    FileSize,
                    Limits.MaxSourceFileBytes));
                return false;
            }
            ContentSegment.File = MakeShareable(
                IFileManager::Get().CreateFileReader(*File.FilePath));
            if (!ContentSegment.File.IsValid())
            {
                OutError = MakeMultipartError(FString::Printf(
                    TEXT("File %d could not be opened. Check file permissions and whether another process is using it."),
                    FileIndex + 1));
                return false;
            }
            ContentSegment.Length = FileSize;
        }
        else
        {
            if (File.Bytes.Num() > Limits.MaxInlineFileBytes)
            {
                OutError = MakeMultipartError(FString::Printf(
                    TEXT("File %d contains %d inline bytes, but Max Inline File Bytes is %lld."),
                    FileIndex + 1,
                    File.Bytes.Num(),
                    Limits.MaxInlineFileBytes));
                return false;
            }
            ContentSegment.Memory = File.Bytes;
            ContentSegment.Length = File.Bytes.Num();
            BufferedBytes += File.Bytes.Num();
        }
        if (!TryAddLength(ContentSegment.Length, Limits, TotalLength))
        {
            OutError = MakeMultipartBodyLimitError(Limits.MaxTotalBodyBytes);
            return false;
        }
        Segments.Add(MoveTemp(ContentSegment));
        if (!AddMemory(ToUtf8(TEXT("\r\n"))))
        {
            OutError = MakeMultipartBodyLimitError(Limits.MaxTotalBodyBytes);
            return false;
        }
    }

    if (!AddMemory(ToUtf8(FString::Printf(TEXT("--%s--\r\n"), *Boundary))))
    {
        OutError = MakeMultipartBodyLimitError(Limits.MaxTotalBodyBytes);
        return false;
    }

    OutResult.Stream = MakeShared<FSegmentedMultipartArchive, ESPMode::ThreadSafe>(MoveTemp(Segments));
    OutResult.ContentType = FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary);
    OutResult.ContentLength = TotalLength;
    OutResult.BufferedBytes = BufferedBytes;
    OutError = FOpenPocketBaseError();
    return true;
}

bool BuildForm(
    const TMap<FString, FString>& Fields,
    const TArray<FOpenPocketBaseFileInput>& Files,
    const FOpenPocketBaseUploadLimits& Limits,
    const FString& Boundary,
    FBuildResult& OutResult,
    FOpenPocketBaseError& OutError)
{
    OutResult = FBuildResult();
    if (Fields.IsEmpty() && Files.IsEmpty())
    {
        OutError = MakeMultipartError(TEXT("The multipart form is empty. Add at least one form field or file."));
        return false;
    }
    if (Fields.Num() > 128)
    {
        OutError = MakeMultipartError(FString::Printf(
            TEXT("The multipart form contains %d fields. Use at most 128 fields."),
            Fields.Num()));
        return false;
    }
    if (!ValidateMultipartLimits(Files.Num(), Limits, Boundary, OutError))
    {
        return false;
    }

    TArray<FMultipartSegment> Segments;
    int64 TotalLength = 0;
    int64 BufferedBytes = 0;
    const auto AddMemory = [&Segments, &TotalLength, &BufferedBytes, &Limits](TArray<uint8> Bytes)
    {
        if (!TryAddLength(Bytes.Num(), Limits, TotalLength))
        {
            return false;
        }
        BufferedBytes += Bytes.Num();
        FMultipartSegment Segment;
        Segment.Length = Bytes.Num();
        Segment.Memory = MoveTemp(Bytes);
        Segments.Add(MoveTemp(Segment));
        return true;
    };

    TArray<FString> FieldNames;
    Fields.GetKeys(FieldNames);
    FieldNames.Sort();
    for (const FString& FieldName : FieldNames)
    {
        const FString* Value = Fields.Find(FieldName);
        if (!IsSafeFieldName(FieldName) || Value == nullptr)
        {
            OutError = MakeMultipartError(TEXT("A multipart form field has an invalid name. Use 1 to 255 letters, numbers, underscores, hyphens, periods, or plus signs."));
            return false;
        }
        const FString Header = FString::Printf(
            TEXT("--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n"),
            *Boundary,
            *FieldName);
        if (!AddMemory(ToUtf8(Header)) || !AddMemory(ToUtf8(*Value)) ||
            !AddMemory(ToUtf8(TEXT("\r\n"))))
        {
            OutError = MakeMultipartBodyLimitError(Limits.MaxTotalBodyBytes);
            return false;
        }
    }

    for (int32 FileIndex = 0; FileIndex < Files.Num(); ++FileIndex)
    {
        const FOpenPocketBaseFileInput& File = Files[FileIndex];
        const FString FieldName = File.GetFieldName();
        if (!IsSafeFieldName(FieldName))
        {
            OutError = MakeMultipartError(FString::Printf(
                TEXT("File %d has an invalid field name. Choose a file field from the selected collection."),
                FileIndex + 1));
            return false;
        }
        if (!IsSafeContentType(File.ContentType))
        {
            OutError = MakeMultipartError(FString::Printf(
                TEXT("File %d has an invalid Content Type. Use a MIME type such as image/png or application/octet-stream."),
                FileIndex + 1));
            return false;
        }
        if (File.Modifier != EOpenPocketBaseFieldModifier::Replace)
        {
            OutError = MakeMultipartError(FString::Printf(
                TEXT("File %d uses an append or remove modifier, but multipart forms accept Replace only."),
                FileIndex + 1));
            return false;
        }

        const FString FileName = File.FileName.IsEmpty() && File.bUseFilePath
            ? SanitizeFileName(File.FilePath)
            : SanitizeFileName(File.FileName);
        const FString Header = FString::Printf(
            TEXT("--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n")
            TEXT("Content-Type: %s\r\n\r\n"),
            *Boundary,
            *FieldName,
            *FileName,
            *File.ContentType);
        if (!AddMemory(ToUtf8(Header)))
        {
            OutError = MakeMultipartBodyLimitError(Limits.MaxTotalBodyBytes);
            return false;
        }

        FMultipartSegment ContentSegment;
        if (File.bUseFilePath)
        {
            if (File.FilePath.IsEmpty())
            {
                OutError = MakeMultipartError(FString::Printf(
                    TEXT("File %d uses a file path, but File Path is empty."),
                    FileIndex + 1));
                return false;
            }
            const int64 FileSize = IFileManager::Get().FileSize(*File.FilePath);
            if (FileSize < 0)
            {
                OutError = MakeMultipartError(FString::Printf(
                    TEXT("File %d could not be found at File Path '%s'. Choose an existing readable file before starting the upload."),
                    FileIndex + 1,
                    *File.FilePath));
                return false;
            }
            if (FileSize > Limits.MaxSourceFileBytes)
            {
                OutError = MakeMultipartError(FString::Printf(
                    TEXT("File %d is %lld bytes, but Max Source File Bytes is %lld."),
                    FileIndex + 1,
                    FileSize,
                    Limits.MaxSourceFileBytes));
                return false;
            }
            ContentSegment.File = MakeShareable(
                IFileManager::Get().CreateFileReader(*File.FilePath));
            if (!ContentSegment.File.IsValid())
            {
                OutError = MakeMultipartError(FString::Printf(
                    TEXT("File %d could not be opened. Check file permissions and whether another process is using it."),
                    FileIndex + 1));
                return false;
            }
            ContentSegment.Length = FileSize;
        }
        else
        {
            if (File.Bytes.Num() > Limits.MaxInlineFileBytes)
            {
                OutError = MakeMultipartError(FString::Printf(
                    TEXT("File %d contains %d inline bytes, but Max Inline File Bytes is %lld."),
                    FileIndex + 1,
                    File.Bytes.Num(),
                    Limits.MaxInlineFileBytes));
                return false;
            }
            ContentSegment.Memory = File.Bytes;
            ContentSegment.Length = File.Bytes.Num();
            BufferedBytes += File.Bytes.Num();
        }
        if (!TryAddLength(ContentSegment.Length, Limits, TotalLength))
        {
            OutError = MakeMultipartBodyLimitError(Limits.MaxTotalBodyBytes);
            return false;
        }
        Segments.Add(MoveTemp(ContentSegment));
        if (!AddMemory(ToUtf8(TEXT("\r\n"))))
        {
            OutError = MakeMultipartBodyLimitError(Limits.MaxTotalBodyBytes);
            return false;
        }
    }

    if (!AddMemory(ToUtf8(FString::Printf(TEXT("--%s--\r\n"), *Boundary))))
    {
        OutError = MakeMultipartBodyLimitError(Limits.MaxTotalBodyBytes);
        return false;
    }

    OutResult.Stream = MakeShared<FSegmentedMultipartArchive, ESPMode::ThreadSafe>(
        MoveTemp(Segments));
    OutResult.ContentType = FString::Printf(
        TEXT("multipart/form-data; boundary=%s"), *Boundary);
    OutResult.ContentLength = TotalLength;
    OutResult.BufferedBytes = BufferedBytes;
    OutError = FOpenPocketBaseError();
    return true;
}
}
