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

bool UOpenPocketBaseAdminAsyncActionBase::TryGetNativeClient(
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe>& OutClient)
{
    OutClient = AdminClient != nullptr ? AdminClient->GetNativeClient() : nullptr;
    return OutClient.IsValid();
}

UOpenPocketBaseAuthenticateSuperuserAsyncAction*
UOpenPocketBaseAuthenticateSuperuserAsyncAction::AuthenticateSuperuser(
    const UObject* WorldContextObject,
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
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseAuthenticateSuperuserAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
        {
            Failed.Broadcast(MakeAdminClientNotReadyError());
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
                if (Result.IsSuccess()) Action->Success.Broadcast(Result.GetValue());
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast();
                else Action->Failed.Broadcast(Result.GetError());
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseAuthenticateSuperuserAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseAdminPageAsyncAction* UOpenPocketBaseAdminPageAsyncAction::ListCollections(
    const UObject* WorldContextObject,
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseAdminListOptions InOptions)
{
    UOpenPocketBaseAdminPageAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminPageAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::Collections;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

UOpenPocketBaseAdminPageAsyncAction* UOpenPocketBaseAdminPageAsyncAction::ListLogs(
    const UObject* WorldContextObject,
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseAdminListOptions InOptions)
{
    UOpenPocketBaseAdminPageAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminPageAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::Logs;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseAdminPageAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
            Failed.Broadcast(MakeAdminClientNotReadyError());
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
                if (Result.IsSuccess()) Action->Success.Broadcast(Result.GetValue());
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast();
                else Action->Failed.Broadcast(Result.GetError());
            }
            Action->Finish();
        };
    RequestHandle = Operation == EOperation::Collections
        ? Native->ListCollections(MoveTemp(Options), MoveTemp(Callback))
        : Native->ListLogs(MoveTemp(Options), MoveTemp(Callback));
}

void UOpenPocketBaseAdminPageAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseAdminDocumentAsyncAction* UOpenPocketBaseAdminDocumentAsyncAction::GetCollection(
    const UObject* WorldContextObject,
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString Collection,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminDocumentAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminDocumentAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::GetCollection;
    Action->Target = MoveTemp(Collection);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

UOpenPocketBaseAdminDocumentAsyncAction* UOpenPocketBaseAdminDocumentAsyncAction::CreateCollection(
    const UObject* WorldContextObject,
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
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

UOpenPocketBaseAdminDocumentAsyncAction* UOpenPocketBaseAdminDocumentAsyncAction::UpdateCollection(
    const UObject* WorldContextObject,
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString Collection,
    FOpenPocketBaseAdminDocument InBody,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminDocumentAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminDocumentAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::UpdateCollection;
    Action->Target = MoveTemp(Collection);
    Action->Body = MoveTemp(InBody);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

UOpenPocketBaseAdminDocumentAsyncAction* UOpenPocketBaseAdminDocumentAsyncAction::GetSettings(
    const UObject* WorldContextObject,
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminDocumentAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminDocumentAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::GetSettings;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

UOpenPocketBaseAdminDocumentAsyncAction* UOpenPocketBaseAdminDocumentAsyncAction::UpdateSettings(
    const UObject* WorldContextObject,
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
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

UOpenPocketBaseAdminDocumentAsyncAction* UOpenPocketBaseAdminDocumentAsyncAction::GetLog(
    const UObject* WorldContextObject,
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
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseAdminDocumentAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
            Failed.Broadcast(MakeAdminClientNotReadyError());
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
                if (Result.IsSuccess()) Action->Success.Broadcast(Result.GetValue());
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast();
                else Action->Failed.Broadcast(Result.GetError());
            }
            Action->Finish();
        };
    switch (Operation)
    {
    case EOperation::GetCollection:
        RequestHandle = Native->GetCollection(
            MoveTemp(Target), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::CreateCollection:
        RequestHandle = Native->CreateCollection(
            MoveTemp(Body), MoveTemp(Callback), MoveTemp(Options));
        break;
    case EOperation::UpdateCollection:
        RequestHandle = Native->UpdateCollection(
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
    Cancelled.Broadcast();
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::DeleteCollection(
    const UObject* WorldContextObject,
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString Collection,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminCommandAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminCommandAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::DeleteCollection;
    Action->Target = MoveTemp(Collection);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::ImportCollections(
    const UObject* WorldContextObject,
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
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::TestS3(
    const UObject* WorldContextObject,
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
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::TestEmail(
    const UObject* WorldContextObject,
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
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::CreateBackup(
    const UObject* WorldContextObject,
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
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::UploadBackup(
    const UObject* WorldContextObject,
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseFileInput InFile,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminCommandAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminCommandAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Operation = EOperation::UploadBackup;
    Action->File = MoveTemp(InFile);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::RestoreBackup(
    const UObject* WorldContextObject,
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
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::DeleteBackup(
    const UObject* WorldContextObject,
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
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

UOpenPocketBaseAdminCommandAsyncAction* UOpenPocketBaseAdminCommandAsyncAction::RunCron(
    const UObject* WorldContextObject,
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
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseAdminCommandAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
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
                if (Result.IsSuccess()) Action->Success.Broadcast();
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast();
                else Action->Failed.Broadcast(Result.GetError());
            }
            Action->Finish();
        };
    switch (Operation)
    {
    case EOperation::DeleteCollection:
        RequestHandle = Native->DeleteCollection(
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
            MoveTemp(File), MoveTemp(Callback), MoveTemp(Options));
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
    Cancelled.Broadcast();
}

UOpenPocketBaseAdminBackupListAsyncAction*
UOpenPocketBaseAdminBackupListAsyncAction::ListBackups(
    const UObject* WorldContextObject,
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminBackupListAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminBackupListAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseAdminBackupListAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
            Failed.Broadcast(MakeAdminClientNotReadyError());
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
                if (Result.IsSuccess()) Action->Success.Broadcast(Result.GetValue());
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast();
                else Action->Failed.Broadcast(Result.GetError());
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseAdminBackupListAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseAdminBackupDownloadAsyncAction*
UOpenPocketBaseAdminBackupDownloadAsyncAction::DownloadBackup(
    const UObject* WorldContextObject,
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString InKey,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminBackupDownloadAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminBackupDownloadAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Key = MoveTemp(InKey);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseAdminBackupDownloadAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
            Failed.Broadcast(MakeAdminClientNotReadyError());
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
                if (Result.IsSuccess()) Action->Success.Broadcast(Result.GetValue());
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast();
                else Action->Failed.Broadcast(Result.GetError());
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseAdminBackupDownloadAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseAdminDocumentListAsyncAction*
UOpenPocketBaseAdminDocumentListAsyncAction::ListCrons(
    const UObject* WorldContextObject,
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminDocumentListAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminDocumentListAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseAdminDocumentListAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
            Failed.Broadcast(MakeAdminClientNotReadyError());
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
                if (Result.IsSuccess()) Action->Success.Broadcast(Result.GetValue());
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast();
                else Action->Failed.Broadcast(Result.GetError());
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseAdminDocumentListAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseAdminSqlAsyncAction* UOpenPocketBaseAdminSqlAsyncAction::RunSql(
    const UObject* WorldContextObject,
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString InQuery,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAdminSqlAsyncAction* Action =
        NewObject<UOpenPocketBaseAdminSqlAsyncAction>();
    Action->AdminClient = PocketBaseAdminClient;
    Action->Query = MoveTemp(InQuery);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseAdminSqlAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
            Failed.Broadcast(MakeAdminClientNotReadyError());
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
                if (Result.IsSuccess()) Action->Success.Broadcast(Result.GetValue());
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                    Action->Cancelled.Broadcast();
                else Action->Failed.Broadcast(Result.GetError());
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseAdminSqlAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseAdminImpersonateAsyncAction*
UOpenPocketBaseAdminImpersonateAsyncAction::Impersonate(
    const UObject* WorldContextObject,
    UOpenPocketBaseAdminClient* PocketBaseAdminClient,
    FString InAuthCollection,
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
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseAdminImpersonateAsyncAction::Activate()
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native;
    if (!TryGetNativeClient(Native))
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
            Failed.Broadcast(MakeAdminClientNotReadyError());
        Finish();
        return;
    }
    const TWeakObjectPtr<UOpenPocketBaseAdminImpersonateAsyncAction> WeakThis(this);
    RequestHandle = Native->Impersonate(
        MoveTemp(AuthCollection),
        MoveTemp(RecordId),
        DurationSeconds,
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
                        Action->Success.Broadcast(Client, Result.GetValue().Record);
                    }
                    else
                    {
                        FOpenPocketBaseError Error;
                        Error.Kind = EOpenPocketBaseErrorKind::Internal;
                        Error.ServerMessage = TEXT("The impersonated client could not be wrapped.");
                        Action->Failed.Broadcast(Error);
                    }
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast();
                }
                else
                {
                    Action->Failed.Broadcast(Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseAdminImpersonateAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}
