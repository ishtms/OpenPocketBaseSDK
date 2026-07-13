#include "OpenPocketBasePackageProbeGameInstance.h"

#include "Async/Async.h"
#include "HttpModule.h"
#include "Dom/JsonValue.h"
#include "Misc/CoreDelegates.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SecureStorage/OpenPocketBaseSecureStore.h"

#include <atomic>

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

class FOpenPocketBasePackageStreamingState final
{
public:
    bool Append(const void* Data, const int64 Length)
    {
        bChunkOffGameThread.store(!IsInGameThread(), std::memory_order_release);
        FScopeLock Lock(&Mutex);
        const int64 Allowed = FMath::Min<int64>(Length, 16 * 1024 - Bytes.Num());
        if (Allowed > 0)
        {
            Bytes.Append(static_cast<const uint8*>(Data), static_cast<int32>(Allowed));
        }
        static const ANSICHAR Marker[] = "PB_CONNECT";
        for (int32 Offset = 0; Offset + UE_ARRAY_COUNT(Marker) - 1 <= Bytes.Num(); ++Offset)
        {
            if (FMemory::Memcmp(
                    Bytes.GetData() + Offset,
                    Marker,
                    UE_ARRAY_COUNT(Marker) - 1) == 0)
            {
                return true;
            }
        }
        return false;
    }

    FCriticalSection Mutex;
    TArray<uint8> Bytes;
    std::atomic<bool> bChunkOffGameThread = false;
    std::atomic<bool> bHandoffScheduled = false;
    bool bGameThreadHandoff = false;
    bool bLifecycleSignals = false;
};

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
    FOpenPocketBaseClientResult ClientResult = FOpenPocketBaseClient::Create(Config);
    if (!ClientResult.IsSuccess())
    {
        FinishTlsProbe(false, ClientResult.GetError());
        return;
    }
    Client = ClientResult.TakeValue();

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
    if (StreamingRequest.IsValid())
    {
        StreamingRequest->CancelRequest();
        StreamingRequest.Reset();
    }
    if (Client.IsValid())
    {
        RealtimeSubscription.Unsubscribe();
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
    FOpenPocketBaseClientResult ClientResult = FOpenPocketBaseClient::Create(Config);
    if (!ClientResult.IsSuccess())
    {
        FinishTransferProbe(false, ClientResult.GetError());
        return;
    }
    Client = ClientResult.TakeValue();

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
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& AuthResult)
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
    if (bSucceeded)
    {
        BeginRealtimeManagerProbe();
        return;
    }
    if (Client.IsValid())
    {
        Client->Shutdown();
        Client.Reset();
    }
    FPlatformMisc::RequestExitWithStatus(true, 4);
}

void UOpenPocketBasePackageProbeGameInstance::BeginRealtimeManagerProbe()
{
    if (!Client.IsValid())
    {
        FinishRealtimeManagerProbe(
            false,
            MakeProbeError(TEXT("The packaged realtime manager client was unavailable.")));
        return;
    }

    const TWeakObjectPtr<UOpenPocketBasePackageProbeGameInstance> WeakThis(this);
    FOpenPocketBaseRealtimeCallbacks Callbacks;
    Callbacks.OnConnectionStateChanged = [WeakThis](
        const EOpenPocketBaseRealtimeConnectionState State)
    {
        UOpenPocketBasePackageProbeGameInstance* Probe = WeakThis.Get();
        if (Probe == nullptr || State != EOpenPocketBaseRealtimeConnectionState::Active ||
            Probe->bRealtimeMutationStarted)
        {
            return;
        }
        Probe->bRealtimeMutationStarted = true;
        FOpenPocketBaseRecordBody Body;
        Body.SetStringField(TEXT("id"), TEXT("pkgrealtime0001"));
        Body.SetStringField(TEXT("title"), TEXT("Packaged realtime manager proof"));
        Probe->Request = Probe->Client->Collection(TEXT("sdk_tasks")).Create(
            MoveTemp(Body),
            [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
            {
                if (UOpenPocketBasePackageProbeGameInstance* Pinned = WeakThis.Get();
                    Pinned != nullptr && !Result.IsSuccess())
                {
                    Pinned->FinishRealtimeManagerProbe(false, Result.GetError());
                }
            });
    };
    Callbacks.OnEvent = [WeakThis](const FOpenPocketBaseRealtimeEvent& Event)
    {
        UOpenPocketBasePackageProbeGameInstance* Probe = WeakThis.Get();
        if (Probe != nullptr && !Probe->bRealtimeManagerFinished &&
            Event.Action == EOpenPocketBaseRealtimeAction::Create && Event.bHasRecord &&
            Event.Record.Id == TEXT("pkgrealtime0001"))
        {
            Probe->RealtimeSubscription.Unsubscribe();
            Probe->DeleteRealtimeManagerRecord();
        }
    };
    Callbacks.OnError = [WeakThis](const FOpenPocketBaseError& Error)
    {
        if (UOpenPocketBasePackageProbeGameInstance* Probe = WeakThis.Get();
            Probe != nullptr && !Probe->bRealtimeManagerFinished)
        {
            Probe->FinishRealtimeManagerProbe(false, Error);
        }
    };

    FOpenPocketBaseError Error;
    RealtimeSubscription = Client->Collection(TEXT("sdk_tasks")).SubscribeToRecords(
        MoveTemp(Callbacks), {}, Error);
    if (!RealtimeSubscription.IsActive())
    {
        FinishRealtimeManagerProbe(false, Error);
    }
}

void UOpenPocketBasePackageProbeGameInstance::DeleteRealtimeManagerRecord()
{
    const TWeakObjectPtr<UOpenPocketBasePackageProbeGameInstance> WeakThis(this);
    Request = Client->Collection(TEXT("sdk_tasks")).Delete(
        TEXT("pkgrealtime0001"),
        [WeakThis](TOpenPocketBaseResult<bool>&& Result)
        {
            if (UOpenPocketBasePackageProbeGameInstance* Probe = WeakThis.Get())
            {
                Probe->FinishRealtimeManagerProbe(
                    Result.IsSuccess() && Result.GetValue(),
                    Result.IsSuccess() ? FOpenPocketBaseError() : Result.GetError());
            }
        });
}

void UOpenPocketBasePackageProbeGameInstance::FinishRealtimeManagerProbe(
    const bool bSucceeded,
    const FOpenPocketBaseError& Error)
{
    if (bRealtimeManagerFinished)
    {
        return;
    }
    bRealtimeManagerFinished = true;
    RealtimeSubscription.Unsubscribe();
    if (bSucceeded)
    {
        UE_LOG(LogTemp, Display, TEXT("OPENPOCKETBASE_PACKAGED_REALTIME_MANAGER_SUCCESS"));
    }
    else
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("OPENPOCKETBASE_PACKAGED_REALTIME_MANAGER_FAILURE kind=%d status=%d request=%s message=%s"),
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
    if (bSucceeded)
    {
        BeginStreamingProbe();
        return;
    }
    FPlatformMisc::RequestExitWithStatus(true, 6);
}

void UOpenPocketBasePackageProbeGameInstance::BeginStreamingProbe()
{
    StreamingState = MakeShared<FOpenPocketBasePackageStreamingState, ESPMode::ThreadSafe>();
    StreamingRequest = FHttpModule::Get().CreateRequest();
    StreamingRequest->SetURL(TransferOrigin + TEXT("/api/realtime"));
    StreamingRequest->SetVerb(TEXT("GET"));
    StreamingRequest->SetHeader(TEXT("Accept"), TEXT("text/event-stream"));
    StreamingRequest->SetTimeout(10.0f);
    StreamingRequest->SetActivityTimeout(5.0f);
    StreamingRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);

    const TWeakObjectPtr<UOpenPocketBasePackageProbeGameInstance> WeakThis(this);
    const TSharedRef<FOpenPocketBasePackageStreamingState, ESPMode::ThreadSafe> State =
        StreamingState.ToSharedRef();
    StreamingRequest->SetResponseBodyReceiveStreamDelegateV2(
        FHttpRequestStreamDelegateV2::CreateLambda(
            [WeakThis, State](void* Data, int64& Length)
            {
                if (Length <= 0 || !State->Append(Data, Length))
                {
                    return;
                }
                bool bExpected = false;
                if (!State->bHandoffScheduled.compare_exchange_strong(
                        bExpected,
                        true,
                        std::memory_order_acq_rel))
                {
                    return;
                }
                AsyncTask(
                    ENamedThreads::GameThread,
                    [WeakThis, State]()
                    {
                        if (UOpenPocketBasePackageProbeGameInstance* Probe = WeakThis.Get())
                        {
                            State->bGameThreadHandoff = IsInGameThread();
                            if (Probe->StreamingRequest.IsValid())
                            {
                                Probe->StreamingRequest->CancelRequest();
                            }
                        }
                    });
            }));
    StreamingRequest->OnProcessRequestComplete().BindLambda(
        [WeakThis](FHttpRequestPtr Request, FHttpResponsePtr Response, const bool bSucceeded)
        {
            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis, bSucceeded]()
                {
                    if (UOpenPocketBasePackageProbeGameInstance* Probe = WeakThis.Get())
                    {
                        Probe->HandleStreamingCancellationComplete(bSucceeded);
                    }
                });
        });
    if (!StreamingRequest->ProcessRequest())
    {
        FinishStreamingProbe(false, TEXT("The packaged streaming request did not start."));
    }
}

void UOpenPocketBasePackageProbeGameInstance::HandleStreamingCancellationComplete(
    const bool bSucceeded)
{
    const bool bCancellationVerified = StreamingState.IsValid() && !bSucceeded &&
        StreamingState->bChunkOffGameThread.load(std::memory_order_acquire) &&
        StreamingState->bGameThreadHandoff;
    StreamingRequest.Reset();
    StreamingState.Reset();
    if (!bCancellationVerified)
    {
        FinishStreamingProbe(
            false,
            TEXT("Incremental delivery, cancellation, or game-thread handoff did not verify."));
        return;
    }
    BeginStreamingTimeoutProbe();
}

void UOpenPocketBasePackageProbeGameInstance::BeginStreamingTimeoutProbe()
{
    StreamingState = MakeShared<FOpenPocketBasePackageStreamingState, ESPMode::ThreadSafe>();
    StreamingRequest = FHttpModule::Get().CreateRequest();
    StreamingRequest->SetURL(TransferOrigin + TEXT("/api/realtime"));
    StreamingRequest->SetVerb(TEXT("GET"));
    StreamingRequest->SetHeader(TEXT("Accept"), TEXT("text/event-stream"));
    StreamingRequest->SetTimeout(10.0f);
    StreamingRequest->SetActivityTimeout(1.0f);
    StreamingRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);
    StreamingTimeoutStartedAt = FPlatformTime::Seconds();

    const TWeakObjectPtr<UOpenPocketBasePackageProbeGameInstance> WeakThis(this);
    const TSharedRef<FOpenPocketBasePackageStreamingState, ESPMode::ThreadSafe> State =
        StreamingState.ToSharedRef();
    StreamingRequest->SetResponseBodyReceiveStreamDelegateV2(
        FHttpRequestStreamDelegateV2::CreateLambda(
            [State](void* Data, int64& Length)
            {
                if (Length > 0)
                {
                    State->Append(Data, Length);
                }
            }));
    StreamingRequest->OnProcessRequestComplete().BindLambda(
        [WeakThis](FHttpRequestPtr Request, FHttpResponsePtr Response, const bool bSucceeded)
        {
            const EHttpFailureReason FailureReason = Request.IsValid()
                ? Request->GetFailureReason()
                : EHttpFailureReason::Other;
            const double CompletedAt = FPlatformTime::Seconds();
            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis, bSucceeded, FailureReason, CompletedAt]()
                {
                    if (UOpenPocketBasePackageProbeGameInstance* Probe = WeakThis.Get())
                    {
                        const double Elapsed = CompletedAt - Probe->StreamingTimeoutStartedAt;
                        const bool bTimedOut = !bSucceeded &&
                            (FailureReason == EHttpFailureReason::TimedOut ||
                                FailureReason == EHttpFailureReason::ConnectionError) &&
                            Elapsed >= 0.75 && Elapsed <= 3.0;
                        Probe->HandleStreamingTimeoutComplete(bTimedOut);
                    }
                });
        });
    if (!StreamingRequest->ProcessRequest())
    {
        FinishStreamingProbe(false, TEXT("The packaged activity-timeout request did not start."));
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("OPENPOCKETBASE_PACKAGED_STREAMING_TIMEOUT_STARTED"));
    FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
    FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
    StreamingState->bLifecycleSignals = true;
}

void UOpenPocketBasePackageProbeGameInstance::HandleStreamingTimeoutComplete(
    const bool bTimedOut)
{
    const bool bVerified = bTimedOut && StreamingState.IsValid() &&
        StreamingState->bChunkOffGameThread.load(std::memory_order_acquire) &&
        StreamingState->bLifecycleSignals;
    StreamingRequest.Reset();
    StreamingState.Reset();
    FinishStreamingProbe(
        bVerified,
        bVerified
            ? TEXT("")
            : TEXT("Activity timeout or lifecycle signal handling did not verify."));
}

void UOpenPocketBasePackageProbeGameInstance::FinishStreamingProbe(
    const bool bSucceeded,
    const TCHAR* Message)
{
    if (bSucceeded)
    {
        UE_LOG(LogTemp, Display, TEXT("OPENPOCKETBASE_PACKAGED_STREAMING_SUCCESS"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("OPENPOCKETBASE_PACKAGED_STREAMING_FAILURE message=%s"), Message);
    }
    FPlatformMisc::RequestExitWithStatus(true, bSucceeded ? 0 : 5);
}
