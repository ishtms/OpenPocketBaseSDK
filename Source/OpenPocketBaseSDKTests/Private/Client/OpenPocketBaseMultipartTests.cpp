#if WITH_DEV_AUTOMATION_TESTS

#include "Files/OpenPocketBaseMultipart.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
TArray<uint8> ToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

bool ContainsBytes(const TArray<uint8>& Haystack, const TArray<uint8>& Needle)
{
    if (Needle.IsEmpty() || Needle.Num() > Haystack.Num())
    {
        return false;
    }

    for (int32 Offset = 0; Offset <= Haystack.Num() - Needle.Num(); ++Offset)
    {
        if (FMemory::Memcmp(Haystack.GetData() + Offset, Needle.GetData(), Needle.Num()) == 0)
        {
            return true;
        }
    }
    return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseMultipartStreamTest,
    "OpenPocketBase.Client.Files.BuildsSegmentedMultipartStream",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseMultipartStreamTest::RunTest(const FString& Parameters)
{
    const FString TempFile = FPaths::CreateTempFilename(
        *FPaths::ProjectIntermediateDir(),
        TEXT("OpenPocketBaseMultipart-"),
        TEXT(".bin"));
    TArray<uint8> DiskBytes;
    DiskBytes.SetNumUninitialized(32 * 1024);
    for (int32 Index = 0; Index < DiskBytes.Num(); ++Index)
    {
        DiskBytes[Index] = static_cast<uint8>((Index * 29) % 251);
    }
    if (!TestTrue(TEXT("The disk fixture is written"), FFileHelper::SaveArrayToFile(DiskBytes, *TempFile)))
    {
        return false;
    }

    FOpenPocketBaseRecordBody Body;
    Body.SetDynamicStringField(TEXT("title"), TEXT("Multipart record"));
    Body.SetDynamicStringArrayField(TEXT("tags"), {TEXT("one"), TEXT("two")});

    FOpenPocketBaseFileInput InlineFile;
    InlineFile.DynamicFieldName = TEXT("avatar");
    InlineFile.FileName = TEXT("../unsafe\"name.png");
    InlineFile.ContentType = TEXT("image/png");
    InlineFile.Modifier = EOpenPocketBaseFieldModifier::Append;
    InlineFile.bUseFilePath = false;
    InlineFile.Bytes = {0x00, 0x01, 0xfe, 0xff};

    FOpenPocketBaseFileInput DiskFile;
    DiskFile.DynamicFieldName = TEXT("document");
    DiskFile.ContentType = TEXT("application/octet-stream");
    DiskFile.bUseFilePath = true;
    DiskFile.FilePath = TempFile;

    const FString Boundary = TEXT("openpb-test-boundary");
    OpenPocketBase::Multipart::FBuildResult Result;
    FOpenPocketBaseError Error;
    const bool bBuilt = OpenPocketBase::Multipart::Build(
        Body,
        {InlineFile, DiskFile},
        FOpenPocketBaseUploadLimits(),
        Boundary,
        Result,
        Error);
    TestTrue(TEXT("A valid multipart stream is built"), bBuilt);
    if (!bBuilt || !TestTrue(TEXT("The multipart stream exists"), Result.Stream.IsValid()))
    {
        IFileManager::Get().Delete(*TempFile, false, true);
        return false;
    }

    TestEqual(
        TEXT("The multipart content type carries the boundary"),
        Result.ContentType,
        FString(TEXT("multipart/form-data; boundary=openpb-test-boundary")));
    TestEqual(TEXT("The archive reports the content length"), Result.Stream->TotalSize(), Result.ContentLength);

    TArray<uint8> Serialized;
    Serialized.SetNumUninitialized(static_cast<int32>(Result.ContentLength));
    int64 ReadOffset = 0;
    while (ReadOffset < Result.ContentLength)
    {
        const int64 ChunkLength = FMath::Min<int64>(7, Result.ContentLength - ReadOffset);
        Result.Stream->Serialize(Serialized.GetData() + ReadOffset, ChunkLength);
        ReadOffset += ChunkLength;
    }

    TestFalse(TEXT("Segmented streaming reads without archive errors"), Result.Stream->IsError());
    TestTrue(TEXT("The JSON payload part is present"), ContainsBytes(Serialized, ToUtf8(TEXT("name=\"@jsonPayload\""))));
    TestTrue(TEXT("The record field is present"), ContainsBytes(Serialized, ToUtf8(TEXT("\"title\""))));
    TestTrue(TEXT("The record value is present"), ContainsBytes(Serialized, ToUtf8(TEXT("\"Multipart record\""))));
    TestTrue(TEXT("File modifiers are preserved"), ContainsBytes(Serialized, ToUtf8(TEXT("name=\"avatar+\""))));
    TestTrue(TEXT("Unsafe filename characters are replaced"), ContainsBytes(Serialized, ToUtf8(TEXT("filename=\"unsafe_name.png\""))));
    TestTrue(TEXT("Inline file bytes stay binary"), ContainsBytes(Serialized, InlineFile.Bytes));
    TestTrue(TEXT("Disk file bytes stream into the body"), ContainsBytes(Serialized, DiskBytes));
    TestTrue(
        TEXT("The multipart body has a closing boundary"),
        ContainsBytes(Serialized, ToUtf8(TEXT("--openpb-test-boundary--\r\n"))));
    TestEqual(
        TEXT("Disk file contents are not buffered in memory"),
        Result.BufferedBytes,
        Result.ContentLength - DiskBytes.Num());

    Result.Stream->Seek(0);
    TArray<uint8> Prefix;
    Prefix.SetNumUninitialized(Boundary.Len() + 2);
    Result.Stream->Serialize(Prefix.GetData(), Prefix.Num());
    TestTrue(TEXT("The stream supports rewinding"), ContainsBytes(Prefix, ToUtf8(TEXT("--openpb-test-boundary"))));

    FOpenPocketBaseUploadLimits InlineLimits;
    InlineLimits.MaxInlineFileBytes = 2;
    OpenPocketBase::Multipart::FBuildResult Rejected;
    TestFalse(
        TEXT("Oversized inline files are rejected before dispatch"),
        OpenPocketBase::Multipart::Build(Body, {InlineFile}, InlineLimits, Boundary, Rejected, Error));
    TestEqual(TEXT("Bound violations use InvalidArgument"), Error.Kind, EOpenPocketBaseErrorKind::InvalidArgument);

    FOpenPocketBaseUploadLimits BodyLimits;
    BodyLimits.MaxTotalBodyBytes = 32;
    TestFalse(
        TEXT("Oversized multipart bodies are rejected before dispatch"),
        OpenPocketBase::Multipart::Build(Body, {InlineFile}, BodyLimits, Boundary, Rejected, Error));

    FOpenPocketBaseFileInput MissingFile;
    MissingFile.DynamicFieldName = TEXT("document");
    MissingFile.FileName = TEXT("missing.txt");
    MissingFile.ContentType = TEXT("text/plain");
    MissingFile.bUseFilePath = true;
    MissingFile.FilePath = FPaths::Combine(
        FPaths::ProjectIntermediateDir(),
        TEXT("OpenPocketBaseMissingMultipartFile.bin"));
    TestFalse(
        TEXT("Missing record-upload paths are rejected before dispatch"),
        OpenPocketBase::Multipart::Build(
            Body,
            {MissingFile},
            FOpenPocketBaseUploadLimits(),
            Boundary,
            Rejected,
            Error));
    TestTrue(
        TEXT("Record-upload errors identify the missing path"),
        Error.Message.Contains(MissingFile.FilePath));

    TestFalse(
        TEXT("Missing custom-form paths are rejected before dispatch"),
        OpenPocketBase::Multipart::BuildForm(
            {{TEXT("note"), TEXT("missing path")}},
            {MissingFile},
            FOpenPocketBaseUploadLimits(),
            Boundary,
            Rejected,
            Error));
    TestTrue(
        TEXT("Custom-form errors identify the missing path"),
        Error.Message.Contains(MissingFile.FilePath));

    const FString SparseFile = FPaths::CreateTempFilename(
        *FPaths::ProjectIntermediateDir(),
        TEXT("OpenPocketBaseSparseMultipart-"),
        TEXT(".bin"));
    TUniquePtr<FArchive> SparseWriter(IFileManager::Get().CreateFileWriter(*SparseFile));
    if (TestTrue(TEXT("A sparse large-file fixture opens"), SparseWriter.IsValid()))
    {
        constexpr int64 SparseSize = 64LL * 1024 * 1024;
        SparseWriter->Seek(SparseSize - 1);
        uint8 LastByte = 0x7f;
        SparseWriter->Serialize(&LastByte, 1);
        const bool bSparseWritten = !SparseWriter->IsError();
        SparseWriter.Reset();
        TestTrue(TEXT("A sparse large-file fixture is written"), bSparseWritten);

        FOpenPocketBaseFileInput SparseInput;
        SparseInput.DynamicFieldName = TEXT("large_file");
        SparseInput.ContentType = TEXT("application/octet-stream");
        SparseInput.FilePath = SparseFile;
        OpenPocketBase::Multipart::FBuildResult SparseResult;
        const bool bSparseBuilt = OpenPocketBase::Multipart::Build(
            Body,
            {SparseInput},
            FOpenPocketBaseUploadLimits(),
            Boundary,
            SparseResult,
            Error);
        TestTrue(TEXT("A 64 MiB disk source builds without buffering it"), bSparseBuilt);
        if (bSparseBuilt)
        {
            TestTrue(TEXT("The large file contributes to the streamed length"), SparseResult.ContentLength > SparseSize);
            TestTrue(TEXT("Buffered multipart overhead stays below 64 KiB"), SparseResult.BufferedBytes < 64 * 1024);
        }
        SparseResult.Stream.Reset();
    }
    IFileManager::Get().Delete(*SparseFile, false, true);

    IFileManager::Get().Delete(*TempFile, false, true);
    return true;
}

#endif
