#include "AsyncActions/OpenPocketBaseRecordAsyncActions.h"

void UOpenPocketBaseAsyncActionBase::Cancel()
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

bool UOpenPocketBaseAsyncActionBase::TryBeginTerminal()
{
    if (bTerminal)
    {
        return false;
    }
    bTerminal = true;
    return true;
}

void UOpenPocketBaseAsyncActionBase::Finish()
{
    Client = nullptr;
    SetReadyToDestroy();
}

UOpenPocketBaseGetRecordAsyncAction* UOpenPocketBaseGetRecordAsyncAction::GetRecord(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InCollection,
    FString InRecordId,
    FOpenPocketBaseRecordOptions InOptions)
{
    UOpenPocketBaseGetRecordAsyncAction* Action = NewObject<UOpenPocketBaseGetRecordAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Collection = MoveTemp(InCollection);
    Action->RecordId = MoveTemp(InRecordId);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseGetRecordAsyncAction::Activate()
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Client != nullptr ? Client->GetNativeClient() : nullptr;
    if (!NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
        {
            FOpenPocketBaseError Error;
            Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            Error.ServerMessage = TEXT("A ready PocketBase client is required.");
            Failed.Broadcast(Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseGetRecordAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(Collection).GetOne(
        MoveTemp(RecordId),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            UOpenPocketBaseGetRecordAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }

            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue());
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
        Options);
}

void UOpenPocketBaseGetRecordAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseGetFirstRecordAsyncAction* UOpenPocketBaseGetFirstRecordAsyncAction::GetFirstRecord(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InCollection,
    FString InFilter,
    FOpenPocketBaseRecordOptions InOptions)
{
    UOpenPocketBaseGetFirstRecordAsyncAction* Action = NewObject<UOpenPocketBaseGetFirstRecordAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Collection = MoveTemp(InCollection);
    Action->Filter = MoveTemp(InFilter);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseGetFirstRecordAsyncAction::Activate()
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Client != nullptr ? Client->GetNativeClient() : nullptr;
    if (!NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
        {
            FOpenPocketBaseError Error;
            Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            Error.ServerMessage = TEXT("A ready PocketBase client is required.");
            Failed.Broadcast(Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseGetFirstRecordAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(Collection).GetFirstListItem(
        MoveTemp(Filter),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            UOpenPocketBaseGetFirstRecordAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }

            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue());
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

void UOpenPocketBaseGetFirstRecordAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseCreateRecordAsyncAction* UOpenPocketBaseCreateRecordAsyncAction::CreateRecord(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InCollection,
    FOpenPocketBaseRecordBody InBody,
    FOpenPocketBaseRecordOptions InOptions)
{
    UOpenPocketBaseCreateRecordAsyncAction* Action = NewObject<UOpenPocketBaseCreateRecordAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Collection = MoveTemp(InCollection);
    Action->Body = MoveTemp(InBody);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseCreateRecordAsyncAction::Activate()
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Client != nullptr ? Client->GetNativeClient() : nullptr;
    if (!NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
        {
            FOpenPocketBaseError Error;
            Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            Error.ServerMessage = TEXT("A ready PocketBase client is required.");
            Failed.Broadcast(Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseCreateRecordAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(Collection).Create(
        MoveTemp(Body),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            UOpenPocketBaseCreateRecordAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }

            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue());
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

void UOpenPocketBaseCreateRecordAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseCreateRecordWithFilesAsyncAction*
UOpenPocketBaseCreateRecordWithFilesAsyncAction::CreateRecordWithFiles(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InCollection,
    FOpenPocketBaseRecordBody InBody,
    TArray<FOpenPocketBaseFileInput> InFiles,
    FOpenPocketBaseRecordOptions InOptions,
    FOpenPocketBaseUploadLimits InLimits)
{
    UOpenPocketBaseCreateRecordWithFilesAsyncAction* Action =
        NewObject<UOpenPocketBaseCreateRecordWithFilesAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Collection = MoveTemp(InCollection);
    Action->Body = MoveTemp(InBody);
    Action->Files = MoveTemp(InFiles);
    Action->Options = MoveTemp(InOptions);
    Action->Limits = MoveTemp(InLimits);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseCreateRecordWithFilesAsyncAction::Activate()
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Client != nullptr ? Client->GetNativeClient() : nullptr;
    if (!NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
        {
            FOpenPocketBaseError Error;
            Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            Error.ServerMessage = TEXT("A ready PocketBase client is required.");
            Failed.Broadcast(Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseCreateRecordWithFilesAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(Collection).CreateWithFiles(
        MoveTemp(Body),
        MoveTemp(Files),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            UOpenPocketBaseCreateRecordWithFilesAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }

            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue());
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
        MoveTemp(Options),
        MoveTemp(Limits));
}

void UOpenPocketBaseCreateRecordWithFilesAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseUpdateRecordAsyncAction* UOpenPocketBaseUpdateRecordAsyncAction::UpdateRecord(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InCollection,
    FString InRecordId,
    FOpenPocketBaseRecordBody InBody,
    FOpenPocketBaseRecordOptions InOptions)
{
    UOpenPocketBaseUpdateRecordAsyncAction* Action = NewObject<UOpenPocketBaseUpdateRecordAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Collection = MoveTemp(InCollection);
    Action->RecordId = MoveTemp(InRecordId);
    Action->Body = MoveTemp(InBody);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseUpdateRecordAsyncAction::Activate()
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Client != nullptr ? Client->GetNativeClient() : nullptr;
    if (!NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
        {
            FOpenPocketBaseError Error;
            Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            Error.ServerMessage = TEXT("A ready PocketBase client is required.");
            Failed.Broadcast(Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseUpdateRecordAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(Collection).Update(
        MoveTemp(RecordId),
        MoveTemp(Body),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            UOpenPocketBaseUpdateRecordAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }

            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue());
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

void UOpenPocketBaseUpdateRecordAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseUpdateRecordWithFilesAsyncAction*
UOpenPocketBaseUpdateRecordWithFilesAsyncAction::UpdateRecordWithFiles(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InCollection,
    FString InRecordId,
    FOpenPocketBaseRecordBody InBody,
    TArray<FOpenPocketBaseFileInput> InFiles,
    FOpenPocketBaseRecordOptions InOptions,
    FOpenPocketBaseUploadLimits InLimits)
{
    UOpenPocketBaseUpdateRecordWithFilesAsyncAction* Action =
        NewObject<UOpenPocketBaseUpdateRecordWithFilesAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Collection = MoveTemp(InCollection);
    Action->RecordId = MoveTemp(InRecordId);
    Action->Body = MoveTemp(InBody);
    Action->Files = MoveTemp(InFiles);
    Action->Options = MoveTemp(InOptions);
    Action->Limits = MoveTemp(InLimits);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseUpdateRecordWithFilesAsyncAction::Activate()
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Client != nullptr ? Client->GetNativeClient() : nullptr;
    if (!NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
        {
            FOpenPocketBaseError Error;
            Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            Error.ServerMessage = TEXT("A ready PocketBase client is required.");
            Failed.Broadcast(Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseUpdateRecordWithFilesAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(Collection).UpdateWithFiles(
        MoveTemp(RecordId),
        MoveTemp(Body),
        MoveTemp(Files),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            UOpenPocketBaseUpdateRecordWithFilesAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }

            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue());
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
        MoveTemp(Options),
        MoveTemp(Limits));
}

void UOpenPocketBaseUpdateRecordWithFilesAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseDeleteRecordAsyncAction* UOpenPocketBaseDeleteRecordAsyncAction::DeleteRecord(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InCollection,
    FString InRecordId,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseDeleteRecordAsyncAction* Action = NewObject<UOpenPocketBaseDeleteRecordAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Collection = MoveTemp(InCollection);
    Action->RecordId = MoveTemp(InRecordId);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseDeleteRecordAsyncAction::Activate()
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Client != nullptr ? Client->GetNativeClient() : nullptr;
    if (!NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
        {
            FOpenPocketBaseError Error;
            Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            Error.ServerMessage = TEXT("A ready PocketBase client is required.");
            Failed.Broadcast(Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseDeleteRecordAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(Collection).Delete(
        MoveTemp(RecordId),
        [WeakThis](TOpenPocketBaseResult<bool>&& Result)
        {
            UOpenPocketBaseDeleteRecordAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }

            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast();
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

void UOpenPocketBaseDeleteRecordAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseListRecordsAsyncAction* UOpenPocketBaseListRecordsAsyncAction::ListRecords(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InCollection,
    FOpenPocketBaseListOptions InOptions)
{
    UOpenPocketBaseListRecordsAsyncAction* Action = NewObject<UOpenPocketBaseListRecordsAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Collection = MoveTemp(InCollection);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseListRecordsAsyncAction::Activate()
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Client != nullptr ? Client->GetNativeClient() : nullptr;
    if (!NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
        {
            FOpenPocketBaseError Error;
            Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            Error.ServerMessage = TEXT("A ready PocketBase client is required.");
            Failed.Broadcast(Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseListRecordsAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(Collection).GetList(
        MoveTemp(Options),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseRecordPage>&& Result)
        {
            UOpenPocketBaseListRecordsAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }

            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue());
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
        });
}

void UOpenPocketBaseListRecordsAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseGetFullListAsyncAction* UOpenPocketBaseGetFullListAsyncAction::GetFullList(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InCollection,
    FOpenPocketBaseFullListOptions InOptions)
{
    UOpenPocketBaseGetFullListAsyncAction* Action = NewObject<UOpenPocketBaseGetFullListAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Collection = MoveTemp(InCollection);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseGetFullListAsyncAction::Activate()
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Client != nullptr ? Client->GetNativeClient() : nullptr;
    if (!NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
        {
            FOpenPocketBaseError Error;
            Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            Error.ServerMessage = TEXT("A ready PocketBase client is required.");
            Failed.Broadcast(Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseGetFullListAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(Collection).GetFullList(
        MoveTemp(Options),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseFullListResult>&& Result)
        {
            UOpenPocketBaseGetFullListAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }

            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue());
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
        });
}

void UOpenPocketBaseGetFullListAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseRefreshAuthAsyncAction* UOpenPocketBaseRefreshAuthAsyncAction::RefreshAuth(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseRefreshAuthAsyncAction* Action = NewObject<UOpenPocketBaseRefreshAuthAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseRefreshAuthAsyncAction::Activate()
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Client != nullptr ? Client->GetNativeClient() : nullptr;
    if (!NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
        {
            FOpenPocketBaseError Error;
            Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            Error.ServerMessage = TEXT("A ready PocketBase client is required.");
            Failed.Broadcast(Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseRefreshAuthAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->RefreshAuth(
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result)
        {
            UOpenPocketBaseRefreshAuthAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }

            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue());
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

void UOpenPocketBaseRefreshAuthAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseRestoreSessionAsyncAction* UOpenPocketBaseRestoreSessionAsyncAction::RestoreSession(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    const bool bInVerifyWithServer,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseRestoreSessionAsyncAction* Action =
        NewObject<UOpenPocketBaseRestoreSessionAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->bVerifyWithServer = bInVerifyWithServer;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseRestoreSessionAsyncAction::Activate()
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Client != nullptr ? Client->GetNativeClient() : nullptr;
    if (!NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
        {
            FOpenPocketBaseError Error;
            Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            Error.ServerMessage = TEXT("A ready PocketBase client is required.");
            Failed.Broadcast(Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseRestoreSessionAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->RestoreSession(
        bVerifyWithServer,
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseSessionRestoreResult>&& Result)
        {
            UOpenPocketBaseRestoreSessionAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }

            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue());
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

void UOpenPocketBaseRestoreSessionAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBasePasswordAuthAsyncAction* UOpenPocketBasePasswordAuthAsyncAction::LogInWithPassword(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InAuthCollection,
    FString InIdentity,
    FString InPassword,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBasePasswordAuthAsyncAction* Action = NewObject<UOpenPocketBasePasswordAuthAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->AuthCollection = MoveTemp(InAuthCollection);
    Action->Identity = MoveTemp(InIdentity);
    Action->Password = MoveTemp(InPassword);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBasePasswordAuthAsyncAction::Activate()
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Client != nullptr ? Client->GetNativeClient() : nullptr;
    if (!NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        if (TryBeginTerminal() && ShouldBroadcastDelegates())
        {
            FOpenPocketBaseError Error;
            Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            Error.ServerMessage = TEXT("A ready PocketBase client is required.");
            Failed.Broadcast(Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBasePasswordAuthAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(AuthCollection).AuthWithPassword(
        MoveTemp(Identity),
        MoveTemp(Password),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result)
        {
            UOpenPocketBasePasswordAuthAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }

            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue());
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
        Options);
}

void UOpenPocketBasePasswordAuthAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}
