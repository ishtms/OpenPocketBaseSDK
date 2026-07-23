#include "AsyncActions/OpenPocketBaseAdminAsyncActions.h"

namespace
{
FOpenPocketBaseError MakeAdminClientNotReadyError()
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
    Error.ServerMessage = TEXT("A ready privileged PocketBase client is required.");
    return Error;
}
}

void UOpenPocketBaseAdminAsyncActionBase::Cancel()
{
    if (!TryBeginTerminal())
    {
        return;
    }
    RequestHandle.Cancel();
    if (ShouldBroadcastDelegates())
    {
        BroadcastCancelled();
    }
    Finish();
}

bool UOpenPocketBaseAdminAsyncActionBase::TryBeginTerminal()
{
    if (bTerminal)
    {
        return false;
    }
    bTerminal = true;
    return true;
}

void UOpenPocketBaseAdminAsyncActionBase::Finish()
{
    AdminClient = nullptr;
    SetReadyToDestroy();
}

FOpenPocketBaseError UOpenPocketBaseAdminAsyncActionBase::MakeCancelledError()
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::Cancelled;
    return Error;
}

bool UOpenPocketBaseAdminAsyncActionBase::TryGetNativeClient(
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe>& OutClient)
{
    OutClient = AdminClient != nullptr ? AdminClient->GetNativeClient() : nullptr;
    return OutClient.IsValid();
}

UOpenPocketBaseAuthenticateSuperuserAsyncAction*
UOpenPocketBaseAuthenticateSuperuserAsyncAction::AuthenticateSuperuser(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString InEmail,
    FString InPassword,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAuthenticateSuperuserAsyncAction* Action =
        NewObject<UOpenPocketBaseAuthenticateSuperuserAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Email = MoveTemp(InEmail);
    Action->Password = MoveTemp(InPassword);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

void UOpenPocketBaseAuthenticateSuperuserAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal())
        {
            Failed.Broadcast(FOpenPocketBaseAdminIdentity(), MakeAdminClientNotReadyError());
        }
        Finish();
        return;
    }
    const TWeakObjectPtr<UOpenPocketBaseAuthenticateSuperuserAsyncAction> WeakThis(this);
    RequestHandle = Native->AuthenticateSuperuser(
        MoveTemp(Email),
        MoveTemp(Password),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAdminIdentity>&& Result)
        {
            UOpenPocketBaseAuthenticateSuperuserAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast(FOpenPocketBaseAdminIdentity(), Result.GetError());
                else Action->Failed.Broadcast(FOpenPocketBaseAdminIdentity(), Result.GetError());
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseAuthenticateSuperuserAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseAdminIdentity(), MakeCancelledError());
}

UOpenPocketBaseAdminPageAsyncAction* UOpenPocketBaseAdminPageAsyncAction::ListCollections(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseAdminCollectionListOptions InOptions)
{
    UOpenPocketBaseAdminPageAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminPageAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::Collections;
    Action->CollectionOptions = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminPageAsyncAction*
UOpenPocketBaseAdminPageAsyncAction::ListDynamicCollections(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseDynamicAdminListOptions InOptions)
{
    UOpenPocketBaseAdminPageAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminPageAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::DynamicCollections;
    Action->DynamicOptions = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminPageAsyncAction* UOpenPocketBaseAdminPageAsyncAction::ListLogs(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseAdminLogListOptions InOptions)
{
    UOpenPocketBaseAdminPageAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminPageAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::Logs;
    Action->LogOptions = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminPageAsyncAction* UOpenPocketBaseAdminPageAsyncAction::ListDynamicLogs(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseDynamicAdminListOptions InOptions)
{
    UOpenPocketBaseAdminPageAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminPageAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::DynamicLogs;
    Action->DynamicOptions = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

void UOpenPocketBaseAdminPageAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal())
            Failed.Broadcast(FOpenPocketBaseAdminPage(), MakeAdminClientNotReadyError());
        Finish();
        return;
    }
    const TWeakObjectPtr<UOpenPocketBaseAdminPageAsyncAction> WeakThis(this);
    FOpenPocketBaseAdminPageCallback Callback =
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAdminPage>&& Result)
        {
            UOpenPocketBaseAdminPageAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal()) return;
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast(FOpenPocketBaseAdminPage(), Result.GetError());
                else Action->Failed.Broadcast(FOpenPocketBaseAdminPage(), Result.GetError());
            }
            Action->Finish();
        };
    switch (Operation)
    {
    case EOperation::Collections:
        RequestHandle = Native->ListCollections(
            MoveTemp(CollectionOptions), MoveTemp(Callback));
        break;
    case EOperation::DynamicCollections:
        RequestHandle = Native->DynamicListCollections(
            MoveTemp(DynamicOptions), MoveTemp(Callback));
        break;
    case EOperation::Logs:
        RequestHandle = Native->ListLogs(MoveTemp(LogOptions), MoveTemp(Callback));
        break;
    case EOperation::DynamicLogs:
        RequestHandle = Native->DynamicListLogs(
            MoveTemp(DynamicOptions), MoveTemp(Callback));
        break;
    }
}

void UOpenPocketBaseAdminPageAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseAdminPage(), MakeCancelledError());
}

UOpenPocketBaseAdminDocumentAsyncAction* UOpenPocketBaseAdminDocumentAsyncAction::GetCollection(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseCollectionRef InCollection,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminDocumentAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminDocumentAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::GetCollection;
    Action->Collection = MoveTemp(InCollection);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminDocumentAsyncAction*
UOpenPocketBaseAdminDocumentAsyncAction::GetDynamicCollection(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString CollectionNameOrId,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminDocumentAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminDocumentAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::GetDynamicCollection;
    Action->Target = MoveTemp(CollectionNameOrId);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminDocumentAsyncAction* UOpenPocketBaseAdminDocumentAsyncAction::CreateCollection(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseAdminDocument InBody,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminDocumentAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminDocumentAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::CreateCollection;
    Action->Body = MoveTemp(InBody);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminDocumentAsyncAction* UOpenPocketBaseAdminDocumentAsyncAction::UpdateCollection(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseCollectionRef InCollection,
    FOpenPocketBaseAdminDocument InBody,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminDocumentAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminDocumentAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::UpdateCollection;
    Action->Collection = MoveTemp(InCollection);
    Action->Body = MoveTemp(InBody);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminDocumentAsyncAction*
UOpenPocketBaseAdminDocumentAsyncAction::UpdateDynamicCollection(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString CollectionNameOrId,
    FOpenPocketBaseAdminDocument InBody,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminDocumentAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminDocumentAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::UpdateDynamicCollection;
    Action->Target = MoveTemp(CollectionNameOrId);
    Action->Body = MoveTemp(InBody);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminDocumentAsyncAction* UOpenPocketBaseAdminDocumentAsyncAction::GetSettings(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminDocumentAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminDocumentAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::GetSettings;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminDocumentAsyncAction* UOpenPocketBaseAdminDocumentAsyncAction::UpdateSettings(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseAdminDocument InBody,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminDocumentAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminDocumentAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::UpdateSettings;
    Action->Body = MoveTemp(InBody);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminDocumentAsyncAction* UOpenPocketBaseAdminDocumentAsyncAction::GetLog(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString LogId,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminDocumentAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminDocumentAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::GetLog;
    Action->Target = MoveTemp(LogId);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

void UOpenPocketBaseAdminDocumentAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal())
            Failed.Broadcast(FOpenPocketBaseAdminDocument(), MakeAdminClientNotReadyError());
        Finish();
        return;
    }
    const TWeakObjectPtr<UOpenPocketBaseAdminDocumentAsyncAction> WeakThis(this);
    FOpenPocketBaseAdminDocumentCallback Callback =
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>&& Result)
        {
            UOpenPocketBaseAdminDocumentAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal()) return;
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast(FOpenPocketBaseAdminDocument(), Result.GetError());
                else Action->Failed.Broadcast(FOpenPocketBaseAdminDocument(), Result.GetError());
            }
            Action->Finish();
        };
    switch (Operation)
    {
    case EOperation::GetCollection:
        RequestHandle = Native->GetCollection(
            MoveTemp(Collection), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::GetDynamicCollection:
        RequestHandle = Native->DynamicGetCollection(
            MoveTemp(Target), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::CreateCollection:
        RequestHandle = Native->CreateCollection(
            MoveTemp(Body), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::UpdateCollection:
        RequestHandle = Native->UpdateCollection(
            MoveTemp(Collection), MoveTemp(Body), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::UpdateDynamicCollection:
        RequestHandle = Native->DynamicUpdateCollection(
            MoveTemp(Target), MoveTemp(Body), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::GetSettings:
        RequestHandle = Native->GetSettings(MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::UpdateSettings:
        RequestHandle = Native->UpdateSettings(
            MoveTemp(Body), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::GetLog:
        RequestHandle = Native->GetLog(
            MoveTemp(Target), MoveTemp(Callback), MoveTemp(Options));
        break;
    }
}

void UOpenPocketBaseAdminDocumentAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseAdminDocument(), MakeCancelledError());
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::DeleteCollection(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseCollectionRef InCollection,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminCommandAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminCommandAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::DeleteCollection;
    Action->Collection = MoveTemp(InCollection);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction*
UOpenPocketBaseAdminCommandAsyncAction::DeleteDynamicCollection(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString CollectionNameOrId,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminCommandAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminCommandAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::DeleteDynamicCollection;
    Action->Target = MoveTemp(CollectionNameOrId);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::ImportCollections(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseAdminDocument InBody,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminCommandAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminCommandAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::ImportCollections;
    Action->Body = MoveTemp(InBody);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::TestS3(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseAdminDocument InBody,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminCommandAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminCommandAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::TestS3;
    Action->Body = MoveTemp(InBody);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::TestEmail(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseAdminDocument InBody,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminCommandAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminCommandAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::TestEmail;
    Action->Body = MoveTemp(InBody);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::CreateBackup(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString Name,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminCommandAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminCommandAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::CreateBackup;
    Action->Target = MoveTemp(Name);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction*
UOpenPocketBaseAdminCommandAsyncAction::UploadBackupFromPath(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString FilePath,
    FString FileName,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminCommandAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminCommandAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::UploadBackup;
    Action->Backup = FOpenPocketBaseAdminBackupInput::FromPath(
        MoveTemp(FilePath), MoveTemp(FileName));
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction*
UOpenPocketBaseAdminCommandAsyncAction::UploadBackupFromBytes(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    TArray<uint8> Bytes,
    FString FileName,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminCommandAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminCommandAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::UploadBackup;
    Action->Backup = FOpenPocketBaseAdminBackupInput::FromBytes(
        MoveTemp(Bytes), MoveTemp(FileName));
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::RestoreBackup(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString Key,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminCommandAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminCommandAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::RestoreBackup;
    Action->Target = MoveTemp(Key);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::DeleteBackup(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString Key,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminCommandAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminCommandAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::DeleteBackup;
    Action->Target = MoveTemp(Key);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::RunCron(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString CronId,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminCommandAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminCommandAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::RunCron;
    Action->Target = MoveTemp(CronId);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

void UOpenPocketBaseAdminCommandAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal())
            Failed.Broadcast(MakeAdminClientNotReadyError());
        Finish();
        return;
    }
    const TWeakObjectPtr<UOpenPocketBaseAdminCommandAsyncAction> WeakThis(this);
    FOpenPocketBaseBoolCallback Callback =
        [WeakThis](TOpenPocketBaseResult<bool>&& Result)
        {
            UOpenPocketBaseAdminCommandAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal()) return;
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess()) Action->Success.Broadcast(FOpenPocketBaseError());
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast(Result.GetError());
                else Action->Failed.Broadcast(Result.GetError());
            }
            Action->Finish();
        };
    switch (Operation)
    {
    case EOperation::DeleteCollection:
        RequestHandle = Native->DeleteCollection(
            MoveTemp(Collection), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::DeleteDynamicCollection:
        RequestHandle = Native->DynamicDeleteCollection(
            MoveTemp(Target), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::ImportCollections:
        RequestHandle = Native->ImportCollections(
            MoveTemp(Body), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::TestS3:
        RequestHandle = Native->TestS3(
            MoveTemp(Body), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::TestEmail:
        RequestHandle = Native->TestEmail(
            MoveTemp(Body), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::CreateBackup:
        RequestHandle = Native->CreateBackup(
            MoveTemp(Target), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::UploadBackup:
        RequestHandle = Native->UploadBackup(
            MoveTemp(Backup), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::RestoreBackup:
        RequestHandle = Native->RestoreBackup(
            MoveTemp(Target), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::DeleteBackup:
        RequestHandle = Native->DeleteBackup(
            MoveTemp(Target), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::RunCron:
        RequestHandle = Native->RunCron(
            MoveTemp(Target), MoveTemp(Callback), MoveTemp(Options));
        break;
    }
}

void UOpenPocketBaseAdminCommandAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(MakeCancelledError());
}

UOpenPocketBaseAdminBackupListAsyncAction*
UOpenPocketBaseAdminBackupListAsyncAction::ListBackups(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminBackupListAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminBackupListAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

void UOpenPocketBaseAdminBackupListAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal())
            Failed.Broadcast(FOpenPocketBaseAdminBackupList(), MakeAdminClientNotReadyError());
        Finish();
        return;
    }
    const TWeakObjectPtr<UOpenPocketBaseAdminBackupListAsyncAction> WeakThis(this);
    RequestHandle = Native->ListBackups(
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAdminBackupList>&& Result)
        {
            UOpenPocketBaseAdminBackupListAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal()) return;
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast(FOpenPocketBaseAdminBackupList(), Result.GetError());
                else Action->Failed.Broadcast(FOpenPocketBaseAdminBackupList(), Result.GetError());
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseAdminBackupListAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseAdminBackupList(), MakeCancelledError());
}

UOpenPocketBaseAdminBackupDownloadAsyncAction*
UOpenPocketBaseAdminBackupDownloadAsyncAction::DownloadBackup(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString InKey,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminBackupDownloadAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminBackupDownloadAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Key = MoveTemp(InKey);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

void UOpenPocketBaseAdminBackupDownloadAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal())
            Failed.Broadcast(FOpenPocketBaseAdminBackupDownload(), MakeAdminClientNotReadyError());
        Finish();
        return;
    }
    const TWeakObjectPtr<UOpenPocketBaseAdminBackupDownloadAsyncAction> WeakThis(this);
    RequestHandle = Native->DownloadBackup(
        MoveTemp(Key),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAdminBackupDownload>&& Result)
        {
            UOpenPocketBaseAdminBackupDownloadAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal()) return;
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast(FOpenPocketBaseAdminBackupDownload(), Result.GetError());
                else Action->Failed.Broadcast(FOpenPocketBaseAdminBackupDownload(), Result.GetError());
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseAdminBackupDownloadAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseAdminBackupDownload(), MakeCancelledError());
}

UOpenPocketBaseAdminDocumentListAsyncAction*
UOpenPocketBaseAdminDocumentListAsyncAction::ListCrons(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminDocumentListAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminDocumentListAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

void UOpenPocketBaseAdminDocumentListAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal())
            Failed.Broadcast(FOpenPocketBaseAdminDocumentList(), MakeAdminClientNotReadyError());
        Finish();
        return;
    }
    const TWeakObjectPtr<UOpenPocketBaseAdminDocumentListAsyncAction> WeakThis(this);
    RequestHandle = Native->ListCrons(
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAdminDocumentList>&& Result)
        {
            UOpenPocketBaseAdminDocumentListAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal()) return;
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast(FOpenPocketBaseAdminDocumentList(), Result.GetError());
                else Action->Failed.Broadcast(FOpenPocketBaseAdminDocumentList(), Result.GetError());
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseAdminDocumentListAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseAdminDocumentList(), MakeCancelledError());
}

UOpenPocketBaseAdminSqlAsyncAction* UOpenPocketBaseAdminSqlAsyncAction::RunSql(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString InQuery,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminSqlAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminSqlAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Query = MoveTemp(InQuery);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

void UOpenPocketBaseAdminSqlAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal())
            Failed.Broadcast(FOpenPocketBaseAdminSqlResult(), MakeAdminClientNotReadyError());
        Finish();
        return;
    }
    const TWeakObjectPtr<UOpenPocketBaseAdminSqlAsyncAction> WeakThis(this);
    RequestHandle = Native->RunSql(
        MoveTemp(Query),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAdminSqlResult>&& Result)
        {
            UOpenPocketBaseAdminSqlAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal()) return;
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast(FOpenPocketBaseAdminSqlResult(), Result.GetError());
                else Action->Failed.Broadcast(FOpenPocketBaseAdminSqlResult(), Result.GetError());
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseAdminSqlAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseAdminSqlResult(), MakeCancelledError());
}

UOpenPocketBaseAdminImpersonateAsyncAction*
UOpenPocketBaseAdminImpersonateAsyncAction::Impersonate(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseAuthCollectionRef InAuthCollection,
    FString InRecordId,
    const int64 InDurationSeconds,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminImpersonateAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminImpersonateAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->AuthCollection = MoveTemp(InAuthCollection);
    Action->RecordId = MoveTemp(InRecordId);
    Action->DurationSeconds = InDurationSeconds;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

UOpenPocketBaseAdminImpersonateAsyncAction*
UOpenPocketBaseAdminImpersonateAsyncAction::ImpersonateDynamicUser(
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString AuthCollectionNameOrId,
    FString InRecordId,
    const int64 InDurationSeconds,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminImpersonateAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminImpersonateAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->DynamicAuthCollection = MoveTemp(AuthCollectionNameOrId);
    Action->RecordId = MoveTemp(InRecordId);
    Action->DurationSeconds = InDurationSeconds;
    Action->Options = MoveTemp(InOptions);
    Action->bDynamicCollection = true;
    Action->RegisterWithGameInstance(Action->AdminClient);
    return Action;
}

void UOpenPocketBaseAdminImpersonateAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal())
            Failed.Broadcast(
                nullptr,
                FOpenPocketBaseRecord(),
                MakeAdminClientNotReadyError());
        Finish();
        return;
    }
    const TWeakObjectPtr<UOpenPocketBaseAdminImpersonateAsyncAction> WeakThis(this);
    FOpenPocketBaseAdminImpersonationCallback Callback =
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAdminImpersonationResult>&& Result)
        {
            UOpenPocketBaseAdminImpersonateAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal()) return;
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    UOpenPocketBaseClient* Client = UOpenPocketBaseClient::Wrap(
                        Action->AdminClient,
                        Result.GetValue().Client);
                    if (Client != nullptr)
                    {
                        Action->Success.Broadcast(
                            Client,
                            Result.GetValue().Record,
                            FOpenPocketBaseError());
                    }
                    else
                    {
                        FOpenPocketBaseError Error;
                        Error.Kind = EOpenPocketBaseErrorKind::Internal;
                        Error.ServerMessage = TEXT("The impersonated client could not be wrapped.");
                        Action->Failed.Broadcast(nullptr, FOpenPocketBaseRecord(), Error);
                    }
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(
                        nullptr,
                        FOpenPocketBaseRecord(),
                        Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(
                        nullptr,
                        FOpenPocketBaseRecord(),
                        Result.GetError());
                }
            }
            Action->Finish();
        };
    RequestHandle = bDynamicCollection
        ? Native->DynamicImpersonate(
            MoveTemp(DynamicAuthCollection),
            MoveTemp(RecordId),
            DurationSeconds,
            MoveTemp(Callback),
            MoveTemp(Options))
        : Native->Impersonate(
            MoveTemp(AuthCollection),
            MoveTemp(RecordId),
            DurationSeconds,
            MoveTemp(Callback),
            MoveTemp(Options));
}

void UOpenPocketBaseAdminImpersonateAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(nullptr, FOpenPocketBaseRecord(), MakeCancelledError());
}
