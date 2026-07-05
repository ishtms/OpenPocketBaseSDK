#include "OpenPocketBasePackageProbeGameInstance.h"

#include "Dom/JsonValue.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SecureStorage/OpenPocketBaseSecureStore.h"

namespace
{
FOpenPocketBaseError MakeProbeError(const TCHAR* Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::Internal;
    Error.ServerMessage = Message;
    return Error;
}
}

void UOpenPocketBasePackageProbeGameInstance::Init()
{
    Super::Init();

    const FString Origin = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENPOCKETBASE_PACKAGE_TLS_ORIGIN"));
    TransferOrigin = FPlatformMisc::GetEnvironmentVariable(
        TEXT("OPENPOCKETBASE_PACKAGE_TRANSFER_ORIGIN"));
    if (Origin.IsEmpty())
    {
        return;
    }

    const TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore =
        CreateOpenPocketBaseSecureStore();
    FString UnavailableReason;
    FOpenPocketBaseError SecureError;
    const FString SecureKey = FString::Printf(
        TEXT("openpocketbase.packaged-probe.%u"),
        FPlatformProcess::GetCurrentProcessId());
    const FTCHARToUTF8 SecureValue(TEXT("ephemeral-packaged-keychain-probe"));
    const TConstArrayView<uint8> Expected(
        reinterpret_cast<const uint8*>(SecureValue.Get()),
        SecureValue.Length());
    TArray<uint8> Loaded;
    bool bFound = false;
    const bool bSecureRoundTrip = SecureStore->IsAvailable(UnavailableReason) &&
        SecureStore->Save(SecureKey, Expected, SecureError) &&
        SecureStore->Load(SecureKey, Loaded, bFound, SecureError) &&
        bFound && Loaded.Num() == Expected.Num() &&
        FMemory::Memcmp(Loaded.GetData(), Expected.GetData(), Expected.Num()) == 0 &&
        SecureStore->Delete(SecureKey, SecureError);
    if (!bSecureRoundTrip)
    {
        SecureStore->Delete(SecureKey, SecureError);
        if (!UnavailableReason.IsEmpty() && SecureError.ServerMessage.IsEmpty())
        {
            SecureError.Kind = EOpenPocketBaseErrorKind::SecureStorage;
            SecureError.ServerMessage = UnavailableReason;
        }
        FinishTlsProbe(false, SecureError);
        return;
    }
    UE_LOG(LogTemp, Display, TEXT("OPENPOCKETBASE_PACKAGED_SECURE_STORAGE_SUCCESS"));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = Origin;
    Config.ProfileName = TEXT("packaged-tls-probe");
    FOpenPocketBaseError Error;
    Client = FOpenPocketBaseClient::Create(Config, Error);
    if (!Client.IsValid())
    {
        FinishTlsProbe(false, Error);
        return;
    }

    FOpenPocketBaseRequestOptions Options;
    Options.TotalTimeoutSeconds = 20;
    Options.ActivityTimeoutSeconds = 10;
    Options.bRetryEligibleReads = false;
    const TWeakObjectPtr<UOpenPocketBasePackageProbeGameInstance> WeakThis(this);
    Request = Client->Collection(TEXT("tls_probe")).GetOne(
        TEXT("missing"),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            if (Result.IsSuccess())
            {
                WeakThis->FinishTlsProbe(true, FOpenPocketBaseError());
                return;
            }

            const FOpenPocketBaseError& RequestError = Result.GetError();
            const bool bReachedTrustedHttp = RequestError.Kind == EOpenPocketBaseErrorKind::Http ||
                RequestError.Kind == EOpenPocketBaseErrorKind::PocketBase ||
                RequestError.Kind == EOpenPocketBaseErrorKind::Serialization;
            WeakThis->FinishTlsProbe(bReachedTrustedHttp, RequestError);
        },
        Options);
}

void UOpenPocketBasePackageProbeGameInstance::Shutdown()
{
    if (Client.IsValid())
    {
        Client->Shutdown();
        Client.Reset();
    }
    Super::Shutdown();
}

void UOpenPocketBasePackageProbeGameInstance::FinishTlsProbe(
    const bool bReachedTrustedHttp,
    const FOpenPocketBaseError& Error)
{
    if (bReachedTrustedHttp)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("OPENPOCKETBASE_PACKAGED_TLS_SUCCESS kind=%d status=%d request=%s"),
            static_cast<int32>(Error.Kind),
            Error.HttpStatus,
            *Error.RequestId);
    }
    else
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("OPENPOCKETBASE_PACKAGED_TLS_FAILURE kind=%d status=%d request=%s"),
            static_cast<int32>(Error.Kind),
            Error.HttpStatus,
            *Error.RequestId);
    }

    if (Client.IsValid())
    {
        Client->Shutdown();
        Client.Reset();
    }
    if (bReachedTrustedHttp && !TransferOrigin.IsEmpty())
    {
        BeginTransferProbe();
        return;
    }
    FPlatformMisc::RequestExitWithStatus(true, bReachedTrustedHttp ? 0 : 3);
}

void UOpenPocketBasePackageProbeGameInstance::BeginTransferProbe()
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TransferOrigin;
    Config.ProfileName = TEXT("packaged-transfer-probe");
    FOpenPocketBaseError Error;
    Client = FOpenPocketBaseClient::Create(Config, Error);
    if (!Client.IsValid())
    {
        FinishTransferProbe(false, Error);
        return;
    }

    ExpectedTransferBytes.SetNumUninitialized(256 * 1024 + 17);
    for (int32 Index = 0; Index < ExpectedTransferBytes.Num(); ++Index)
    {
        ExpectedTransferBytes[Index] = Index % 79 == 78
            ? static_cast<uint8>('\n')
            : static_cast<uint8>('a' + Index % 26);
    }
    const FString ProbeSuffix = LexToString(FPlatformProcess::GetCurrentProcessId());
    UploadPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("OpenPocketBaseUpload-") + ProbeSuffix + TEXT(".txt"));
    DownloadPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("OpenPocketBaseDownload-") + ProbeSuffix + TEXT(".txt"));
    IFileManager::Get().Delete(*UploadPath, false, true, true);
    IFileManager::Get().Delete(*DownloadPath, false, true, true);
    IFileManager::Get().Delete(*(DownloadPath + TEXT(".tmp")), false, true, true);
    if (!FFileHelper::SaveArrayToFile(ExpectedTransferBytes, *UploadPath))
    {
        FinishTransferProbe(false, MakeProbeError(TEXT("The packaged upload fixture could not be written.")));
        return;
    }

    const TWeakObjectPtr<UOpenPocketBasePackageProbeGameInstance> WeakThis(this);
    Request = Client->Collection(TEXT("sdk_users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("correct-horse-battery"),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& AuthResult)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }
            if (!AuthResult.IsSuccess())
            {
                WeakThis->FinishTransferProbe(false, AuthResult.GetError());
                return;
            }

            FOpenPocketBaseRecordBody Body;
            Body.SetStringField(TEXT("id"), TEXT("pkgprobe0000001"));
            Body.SetStringField(TEXT("title"), TEXT("Packaged transfer proof"));
            FOpenPocketBaseFileInput File;
            File.FieldName = TEXT("attachments");
            File.FileName = TEXT("packaged-proof.txt");
            File.ContentType = TEXT("text/plain");
            File.FilePath = WeakThis->UploadPath;
            WeakThis->Request = WeakThis->Client->Collection(TEXT("sdk_tasks")).CreateWithFiles(
                MoveTemp(Body),
                {MoveTemp(File)},
                [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& CreateResult)
                {
                    if (!WeakThis.IsValid())
                    {
                        return;
                    }
                    if (!CreateResult.IsSuccess())
                    {
                        WeakThis->FinishTransferProbe(false, CreateResult.GetError());
                        return;
                    }

                    WeakThis->TransferRecordId = CreateResult.GetValue().Id;
                    const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
                    if (!CreateResult.GetValue().Data.JsonObject.IsValid() ||
                        !CreateResult.GetValue().Data.JsonObject->TryGetArrayField(
                            TEXT("attachments"),
                            Files) ||
                        Files == nullptr || Files->IsEmpty() || !(*Files)[0].IsValid())
                    {
                        WeakThis->DeleteTransferRecord(
                            false,
                            MakeProbeError(TEXT("The packaged upload response omitted its file name.")));
                        return;
                    }
                    WeakThis->TransferFileName = (*Files)[0]->AsString();
                    WeakThis->Request = WeakThis->Client->Files().GetToken(
                        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseFileToken>&& TokenResult)
                        {
                            if (!WeakThis.IsValid())
                            {
                                return;
                            }
                            if (!TokenResult.IsSuccess() || !TokenResult.GetValue().IsSet())
                            {
                                WeakThis->DeleteTransferRecord(
                                    false,
                                    TokenResult.IsSuccess()
                                        ? MakeProbeError(TEXT("The packaged protected-file token was empty."))
                                        : TokenResult.GetError());
                                return;
                            }
                            WeakThis->DownloadTransferFile(MoveTemp(TokenResult.GetValue()));
                        });
                },
                {},
                {},
                [WeakThis](const FOpenPocketBaseTransferProgress& Progress)
                {
                    if (WeakThis.IsValid() &&
                        Progress.Phase == EOpenPocketBaseTransferPhase::Finalizing &&
                        Progress.bHasTotalBytes && Progress.TotalBytes > 0 &&
                        Progress.TransferredBytes == Progress.TotalBytes)
                    {
                        WeakThis->bUploadProgressVerified = true;
                    }
                });
        });
}

void UOpenPocketBasePackageProbeGameInstance::DownloadTransferFile(
    FOpenPocketBaseFileToken Token)
{
    FOpenPocketBaseFileDownloadOptions Options;
    Options.Target = EOpenPocketBaseFileDownloadTarget::File;
    Options.DestinationPath = DownloadPath;
    Options.MaxBytes = 1024 * 1024;
    const TWeakObjectPtr<UOpenPocketBasePackageProbeGameInstance> WeakThis(this);
    Request = Client->Files().Download(
        TEXT("sdk_tasks"),
        TransferRecordId,
        TransferFileName,
        MoveTemp(Options),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&& Result)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }
            if (!Result.IsSuccess())
            {
                WeakThis->DeleteTransferRecord(false, Result.GetError());
                return;
            }

            TArray<uint8> DownloadedBytes;
            const bool bVerified = Result.GetValue().bSavedToFile &&
                FFileHelper::LoadFileToArray(DownloadedBytes, *WeakThis->DownloadPath) &&
                DownloadedBytes == WeakThis->ExpectedTransferBytes &&
                WeakThis->bUploadProgressVerified &&
                WeakThis->bDownloadProgressVerified;
            WeakThis->DeleteTransferRecord(
                bVerified,
                bVerified
                    ? FOpenPocketBaseError()
                    : MakeProbeError(TEXT("The packaged transfer bytes or progress did not verify.")));
        },
        MoveTemp(Token),
        [WeakThis](const FOpenPocketBaseTransferProgress& Progress)
        {
            if (WeakThis.IsValid() &&
                Progress.Phase == EOpenPocketBaseTransferPhase::Finalizing &&
                Progress.bHasTotalBytes && Progress.TotalBytes > 0 &&
                Progress.TransferredBytes == Progress.TotalBytes)
            {
                WeakThis->bDownloadProgressVerified = true;
            }
        });
}

void UOpenPocketBasePackageProbeGameInstance::DeleteTransferRecord(
    const bool bTransferSucceeded,
    FOpenPocketBaseError Error)
{
    if (!Client.IsValid() || TransferRecordId.IsEmpty())
    {
        FinishTransferProbe(bTransferSucceeded, Error);
        return;
    }

    const TWeakObjectPtr<UOpenPocketBasePackageProbeGameInstance> WeakThis(this);
    Request = Client->Collection(TEXT("sdk_tasks")).Delete(
        TransferRecordId,
        [WeakThis, bTransferSucceeded, Error = MoveTemp(Error)](
            TOpenPocketBaseResult<bool>&& DeleteResult) mutable
        {
            if (!WeakThis.IsValid())
            {
                return;
            }
            if (!DeleteResult.IsSuccess() && bTransferSucceeded)
            {
                WeakThis->FinishTransferProbe(false, DeleteResult.GetError());
                return;
            }
            WeakThis->FinishTransferProbe(bTransferSucceeded, Error);
        });
}

void UOpenPocketBasePackageProbeGameInstance::FinishTransferProbe(
    const bool bSucceeded,
    const FOpenPocketBaseError& Error)
{
    IFileManager::Get().Delete(*UploadPath, false, true, true);
    IFileManager::Get().Delete(*DownloadPath, false, true, true);
    IFileManager::Get().Delete(*(DownloadPath + TEXT(".tmp")), false, true, true);
    if (bSucceeded)
    {
        UE_LOG(LogTemp, Display, TEXT("OPENPOCKETBASE_PACKAGED_TRANSFER_SUCCESS"));
    }
    else
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("OPENPOCKETBASE_PACKAGED_TRANSFER_FAILURE kind=%d status=%d request=%s message=%s"),
            static_cast<int32>(Error.Kind),
            Error.HttpStatus,
            *Error.RequestId,
            *Error.ServerMessage);
    }
    if (Client.IsValid())
    {
        Client->Shutdown();
        Client.Reset();
    }
    FPlatformMisc::RequestExitWithStatus(true, bSucceeded ? 0 : 4);
}
