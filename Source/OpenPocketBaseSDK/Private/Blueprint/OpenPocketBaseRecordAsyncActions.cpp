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

FOpenPocketBaseError UOpenPocketBaseAsyncActionBase::MakeCancelledError()
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::Cancelled;
    return Error;
}

UOpenPocketBaseHealthAsyncAction* UOpenPocketBaseHealthAsyncAction::CheckHealth(
    UOpenPocketBaseClient* PocketBaseClient,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseHealthAsyncAction* Action =
        NewObject<UOpenPocketBaseHealthAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
    return Action;
}

void UOpenPocketBaseHealthAsyncAction::Activate()
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
            Failed.Broadcast(FOpenPocketBaseHealthResult(), Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseHealthAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Health(
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseHealthResult>&& Result)
        {
            UOpenPocketBaseHealthAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(FOpenPocketBaseHealthResult(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseHealthResult(), Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseHealthAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseHealthResult(), MakeCancelledError());
}

UOpenPocketBaseCustomRouteAsyncAction*
UOpenPocketBaseCustomRouteAsyncAction::SendCustomRoute(
    UOpenPocketBaseClient* PocketBaseClient,
    FOpenPocketBaseCustomRouteRequest InRequest)
{
    UOpenPocketBaseCustomRouteAsyncAction* Action =
        NewObject<UOpenPocketBaseCustomRouteAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Request = MoveTemp(InRequest);
    Action->RegisterWithGameInstance(Action->Client);
    return Action;
}

void UOpenPocketBaseCustomRouteAsyncAction::Activate()
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
            Failed.Broadcast(FOpenPocketBaseCustomRouteResponse(), Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseCustomRouteAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->SendCustomRoute(
        MoveTemp(Request),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseCustomRouteResponse>&& Result)
        {
            UOpenPocketBaseCustomRouteAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(FOpenPocketBaseCustomRouteResponse(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseCustomRouteResponse(), Result.GetError());
                }
            }
            Action->Finish();
        });
}

void UOpenPocketBaseCustomRouteAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseCustomRouteResponse(), MakeCancelledError());
}

UOpenPocketBaseGetRecordAsyncAction* UOpenPocketBaseGetRecordAsyncAction::GetRecord(
    FOpenPocketBaseCollection InCollection,
    FString InRecordId,
    FOpenPocketBaseRecordOptions InOptions)
{
    UOpenPocketBaseGetRecordAsyncAction* Action = NewObject<UOpenPocketBaseGetRecordAsyncAction>();
    Action->Client = InCollection.Client;
    Action->Collection = MoveTemp(InCollection.Name);
    Action->RecordId = MoveTemp(InRecordId);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
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
            Failed.Broadcast(FOpenPocketBaseRecord(), Error);
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
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(FOpenPocketBaseRecord(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseRecord(), Result.GetError());
                }
            }
            Action->Finish();
        },
        Options);
}

void UOpenPocketBaseGetRecordAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseRecord(), MakeCancelledError());
}

UOpenPocketBaseGetFirstRecordAsyncAction* UOpenPocketBaseGetFirstRecordAsyncAction::GetFirstRecord(
    FOpenPocketBaseCollection InCollection,
    FOpenPocketBaseFilter InFilter,
    FOpenPocketBaseRecordOptions InOptions)
{
    UOpenPocketBaseGetFirstRecordAsyncAction* Action = NewObject<UOpenPocketBaseGetFirstRecordAsyncAction>();
    Action->Client = InCollection.Client;
    Action->Collection = MoveTemp(InCollection.Name);
    Action->Filter = MoveTemp(InFilter);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
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
            Failed.Broadcast(FOpenPocketBaseRecord(), Error);
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
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(FOpenPocketBaseRecord(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseRecord(), Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseGetFirstRecordAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseRecord(), MakeCancelledError());
}

UOpenPocketBaseCreateRecordAsyncAction* UOpenPocketBaseCreateRecordAsyncAction::CreateRecord(
    FOpenPocketBaseCollection InCollection,
    FOpenPocketBaseRecordBody InBody,
    FOpenPocketBaseRecordOptions InOptions)
{
    UOpenPocketBaseCreateRecordAsyncAction* Action = NewObject<UOpenPocketBaseCreateRecordAsyncAction>();
    Action->Client = InCollection.Client;
    Action->Collection = MoveTemp(InCollection.Name);
    Action->Body = MoveTemp(InBody);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
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
            Failed.Broadcast(FOpenPocketBaseRecord(), Error);
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
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(FOpenPocketBaseRecord(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseRecord(), Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseCreateRecordAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseRecord(), MakeCancelledError());
}

UOpenPocketBaseCreateRecordWithFilesAsyncAction*
UOpenPocketBaseCreateRecordWithFilesAsyncAction::CreateRecordWithFiles(
    FOpenPocketBaseCollection InCollection,
    FOpenPocketBaseRecordBody InBody,
    TArray<FOpenPocketBaseFileInput> InFiles,
    FOpenPocketBaseRecordOptions InOptions,
    FOpenPocketBaseUploadLimits InLimits)
{
    UOpenPocketBaseCreateRecordWithFilesAsyncAction* Action =
        NewObject<UOpenPocketBaseCreateRecordWithFilesAsyncAction>();
    Action->Client = InCollection.Client;
    Action->Collection = MoveTemp(InCollection.Name);
    Action->Body = MoveTemp(InBody);
    Action->Files = MoveTemp(InFiles);
    Action->Options = MoveTemp(InOptions);
    Action->Limits = MoveTemp(InLimits);
    Action->RegisterWithGameInstance(Action->Client);
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
            Failed.Broadcast(
                FOpenPocketBaseRecord(),
                FOpenPocketBaseTransferProgress(),
                Error);
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
                    Action->Success.Broadcast(
                        Result.GetValue(),
                        FOpenPocketBaseTransferProgress(),
                        FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(
                        FOpenPocketBaseRecord(),
                        FOpenPocketBaseTransferProgress(),
                        Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(
                        FOpenPocketBaseRecord(),
                        FOpenPocketBaseTransferProgress(),
                        Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Options),
        MoveTemp(Limits),
        [WeakThis](const FOpenPocketBaseTransferProgress& TransferProgress)
        {
            UOpenPocketBaseCreateRecordWithFilesAsyncAction* Action = WeakThis.Get();
            if (Action != nullptr && !Action->bTerminal && Action->ShouldBroadcastDelegates())
            {
                Action->Progress.Broadcast(
                    FOpenPocketBaseRecord(),
                    TransferProgress,
                    FOpenPocketBaseError());
            }
        });
}

void UOpenPocketBaseCreateRecordWithFilesAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(
        FOpenPocketBaseRecord(),
        FOpenPocketBaseTransferProgress(),
        MakeCancelledError());
}

UOpenPocketBaseUpdateRecordAsyncAction* UOpenPocketBaseUpdateRecordAsyncAction::UpdateRecord(
    FOpenPocketBaseCollection InCollection,
    FString InRecordId,
    FOpenPocketBaseRecordBody InBody,
    FOpenPocketBaseRecordOptions InOptions)
{
    UOpenPocketBaseUpdateRecordAsyncAction* Action = NewObject<UOpenPocketBaseUpdateRecordAsyncAction>();
    Action->Client = InCollection.Client;
    Action->Collection = MoveTemp(InCollection.Name);
    Action->RecordId = MoveTemp(InRecordId);
    Action->Body = MoveTemp(InBody);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
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
            Failed.Broadcast(FOpenPocketBaseRecord(), Error);
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
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(FOpenPocketBaseRecord(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseRecord(), Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseUpdateRecordAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseRecord(), MakeCancelledError());
}

UOpenPocketBaseUpdateRecordWithFilesAsyncAction*
UOpenPocketBaseUpdateRecordWithFilesAsyncAction::UpdateRecordWithFiles(
    FOpenPocketBaseCollection InCollection,
    FString InRecordId,
    FOpenPocketBaseRecordBody InBody,
    TArray<FOpenPocketBaseFileInput> InFiles,
    FOpenPocketBaseRecordOptions InOptions,
    FOpenPocketBaseUploadLimits InLimits)
{
    UOpenPocketBaseUpdateRecordWithFilesAsyncAction* Action =
        NewObject<UOpenPocketBaseUpdateRecordWithFilesAsyncAction>();
    Action->Client = InCollection.Client;
    Action->Collection = MoveTemp(InCollection.Name);
    Action->RecordId = MoveTemp(InRecordId);
    Action->Body = MoveTemp(InBody);
    Action->Files = MoveTemp(InFiles);
    Action->Options = MoveTemp(InOptions);
    Action->Limits = MoveTemp(InLimits);
    Action->RegisterWithGameInstance(Action->Client);
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
            Failed.Broadcast(
                FOpenPocketBaseRecord(),
                FOpenPocketBaseTransferProgress(),
                Error);
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
                    Action->Success.Broadcast(
                        Result.GetValue(),
                        FOpenPocketBaseTransferProgress(),
                        FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(
                        FOpenPocketBaseRecord(),
                        FOpenPocketBaseTransferProgress(),
                        Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(
                        FOpenPocketBaseRecord(),
                        FOpenPocketBaseTransferProgress(),
                        Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Options),
        MoveTemp(Limits),
        [WeakThis](const FOpenPocketBaseTransferProgress& TransferProgress)
        {
            UOpenPocketBaseUpdateRecordWithFilesAsyncAction* Action = WeakThis.Get();
            if (Action != nullptr && !Action->bTerminal && Action->ShouldBroadcastDelegates())
            {
                Action->Progress.Broadcast(
                    FOpenPocketBaseRecord(),
                    TransferProgress,
                    FOpenPocketBaseError());
            }
        });
}

void UOpenPocketBaseUpdateRecordWithFilesAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(
        FOpenPocketBaseRecord(),
        FOpenPocketBaseTransferProgress(),
        MakeCancelledError());
}

UOpenPocketBaseDeleteRecordAsyncAction* UOpenPocketBaseDeleteRecordAsyncAction::DeleteRecord(
    FOpenPocketBaseCollection InCollection,
    FString InRecordId,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseDeleteRecordAsyncAction* Action = NewObject<UOpenPocketBaseDeleteRecordAsyncAction>();
    Action->Client = InCollection.Client;
    Action->Collection = MoveTemp(InCollection.Name);
    Action->RecordId = MoveTemp(InRecordId);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
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
                    Action->Success.Broadcast(FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(Result.GetError());
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
    Cancelled.Broadcast(MakeCancelledError());
}

UOpenPocketBaseListRecordsAsyncAction* UOpenPocketBaseListRecordsAsyncAction::ListRecords(
    FOpenPocketBaseCollection InCollection,
    FOpenPocketBaseListOptions InOptions)
{
    UOpenPocketBaseListRecordsAsyncAction* Action = NewObject<UOpenPocketBaseListRecordsAsyncAction>();
    Action->Client = InCollection.Client;
    Action->Collection = MoveTemp(InCollection.Name);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
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
            Failed.Broadcast(FOpenPocketBaseRecordPage(), Error);
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
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(FOpenPocketBaseRecordPage(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseRecordPage(), Result.GetError());
                }
            }
            Action->Finish();
        });
}

void UOpenPocketBaseListRecordsAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseRecordPage(), MakeCancelledError());
}

UOpenPocketBaseGetFullListAsyncAction* UOpenPocketBaseGetFullListAsyncAction::GetFullList(
    FOpenPocketBaseCollection InCollection,
    FOpenPocketBaseFullListOptions InOptions)
{
    UOpenPocketBaseGetFullListAsyncAction* Action = NewObject<UOpenPocketBaseGetFullListAsyncAction>();
    Action->Client = InCollection.Client;
    Action->Collection = MoveTemp(InCollection.Name);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
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
            Failed.Broadcast(FOpenPocketBaseFullListResult(), Error);
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
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(FOpenPocketBaseFullListResult(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseFullListResult(), Result.GetError());
                }
            }
            Action->Finish();
        });
}

void UOpenPocketBaseGetFullListAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseFullListResult(), MakeCancelledError());
}

UOpenPocketBaseRefreshAuthAsyncAction* UOpenPocketBaseRefreshAuthAsyncAction::RefreshAuth(
    UOpenPocketBaseClient* PocketBaseClient,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseRefreshAuthAsyncAction* Action = NewObject<UOpenPocketBaseRefreshAuthAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
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
            Failed.Broadcast(FOpenPocketBaseAuthResult(), Error);
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
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(FOpenPocketBaseAuthResult(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseAuthResult(), Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseRefreshAuthAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseAuthResult(), MakeCancelledError());
}

UOpenPocketBaseRestoreSessionAsyncAction* UOpenPocketBaseRestoreSessionAsyncAction::RestoreSession(
    UOpenPocketBaseClient* PocketBaseClient,
    const bool bInVerifyWithServer,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseRestoreSessionAsyncAction* Action =
        NewObject<UOpenPocketBaseRestoreSessionAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->bVerifyWithServer = bInVerifyWithServer;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
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
            Failed.Broadcast(FOpenPocketBaseSessionRestoreResult(), Error);
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
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(FOpenPocketBaseSessionRestoreResult(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseSessionRestoreResult(), Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseRestoreSessionAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseSessionRestoreResult(), MakeCancelledError());
}

UOpenPocketBaseListAuthMethodsAsyncAction*
UOpenPocketBaseListAuthMethodsAsyncAction::ListAuthenticationMethods(
    FOpenPocketBaseCollection InAuthCollection,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseListAuthMethodsAsyncAction* Action =
        NewObject<UOpenPocketBaseListAuthMethodsAsyncAction>();
    Action->Client = InAuthCollection.Client;
    Action->AuthCollection = MoveTemp(InAuthCollection.Name);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
    return Action;
}

void UOpenPocketBaseListAuthMethodsAsyncAction::Activate()
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
            Failed.Broadcast(FOpenPocketBaseAuthMethods(), Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseListAuthMethodsAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(AuthCollection).ListAuthMethods(
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAuthMethods>&& Result)
        {
            UOpenPocketBaseListAuthMethodsAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(FOpenPocketBaseAuthMethods(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseAuthMethods(), Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseListAuthMethodsAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseAuthMethods(), MakeCancelledError());
}

UOpenPocketBaseRequestOtpAsyncAction* UOpenPocketBaseRequestOtpAsyncAction::RequestOneTimePassword(
    FOpenPocketBaseCollection InAuthCollection,
    FString InEmail,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseRequestOtpAsyncAction* Action = NewObject<UOpenPocketBaseRequestOtpAsyncAction>();
    Action->Client = InAuthCollection.Client;
    Action->AuthCollection = MoveTemp(InAuthCollection.Name);
    Action->Email = MoveTemp(InEmail);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
    return Action;
}

void UOpenPocketBaseRequestOtpAsyncAction::Activate()
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
            Failed.Broadcast(FOpenPocketBaseOtpRequest(), Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseRequestOtpAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(AuthCollection).RequestOtp(
        MoveTemp(Email),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseOtpRequest>&& Result)
        {
            UOpenPocketBaseRequestOtpAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(FOpenPocketBaseOtpRequest(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseOtpRequest(), Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseRequestOtpAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseOtpRequest(), MakeCancelledError());
}

UOpenPocketBaseOtpAuthAsyncAction* UOpenPocketBaseOtpAuthAsyncAction::LogInWithOneTimePassword(
    FOpenPocketBaseCollection InAuthCollection,
    FString InOtpId,
    FString InOneTimePassword,
    FOpenPocketBaseMfaContinuation InMfa,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseOtpAuthAsyncAction* Action = NewObject<UOpenPocketBaseOtpAuthAsyncAction>();
    Action->Client = InAuthCollection.Client;
    Action->AuthCollection = MoveTemp(InAuthCollection.Name);
    Action->OtpId = MoveTemp(InOtpId);
    Action->Password = MoveTemp(InOneTimePassword);
    Action->Mfa = MoveTemp(InMfa);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
    return Action;
}

void UOpenPocketBaseOtpAuthAsyncAction::Activate()
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
            Failed.Broadcast(
                FOpenPocketBaseAuthResult(),
                FOpenPocketBaseMfaContinuation(),
                Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseOtpAuthAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(AuthCollection).AuthWithOtp(
        MoveTemp(OtpId),
        MoveTemp(Password),
        MoveTemp(Mfa),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
            UOpenPocketBaseOtpAuthAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess() &&
                    Result.GetValue().Status == EOpenPocketBaseAuthAttemptStatus::Authenticated)
                {
                    Action->Success.Broadcast(
                        Result.GetValue().Authentication,
                        FOpenPocketBaseMfaContinuation(),
                        FOpenPocketBaseError());
                }
                else if (Result.IsSuccess())
                {
                    Action->MfaRequired.Broadcast(
                        FOpenPocketBaseAuthResult(),
                        Result.GetValue().Mfa,
                        FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(
                        FOpenPocketBaseAuthResult(),
                        FOpenPocketBaseMfaContinuation(),
                        Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(
                        FOpenPocketBaseAuthResult(),
                        FOpenPocketBaseMfaContinuation(),
                        Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseOtpAuthAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(
        FOpenPocketBaseAuthResult(),
        FOpenPocketBaseMfaContinuation(),
        MakeCancelledError());
}

UOpenPocketBaseBeginOAuth2AsyncAction* UOpenPocketBaseBeginOAuth2AsyncAction::BeginManualOAuth2(
    FOpenPocketBaseCollection InAuthCollection,
    FOpenPocketBaseOAuth2StartOptions InOptions)
{
    UOpenPocketBaseBeginOAuth2AsyncAction* Action =
        NewObject<UOpenPocketBaseBeginOAuth2AsyncAction>();
    Action->Client = InAuthCollection.Client;
    Action->AuthCollection = MoveTemp(InAuthCollection.Name);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
    return Action;
}

void UOpenPocketBaseBeginOAuth2AsyncAction::Activate()
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
            Failed.Broadcast(FOpenPocketBaseOAuth2Authorization(), Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseBeginOAuth2AsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(AuthCollection).BeginOAuth2(
        MoveTemp(Options),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseOAuth2Authorization>&& Result)
        {
            UOpenPocketBaseBeginOAuth2AsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(FOpenPocketBaseOAuth2Authorization(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseOAuth2Authorization(), Result.GetError());
                }
            }
            Action->Finish();
        });
}

void UOpenPocketBaseBeginOAuth2AsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseOAuth2Authorization(), MakeCancelledError());
}

UOpenPocketBaseCompleteOAuth2AsyncAction*
UOpenPocketBaseCompleteOAuth2AsyncAction::CompleteManualOAuth2(
    FOpenPocketBaseCollection InAuthCollection,
    FOpenPocketBaseOAuth2Callback InCallback)
{
    UOpenPocketBaseCompleteOAuth2AsyncAction* Action =
        NewObject<UOpenPocketBaseCompleteOAuth2AsyncAction>();
    Action->Client = InAuthCollection.Client;
    Action->AuthCollection = MoveTemp(InAuthCollection.Name);
    Action->Callback = MoveTemp(InCallback);
    Action->RegisterWithGameInstance(Action->Client);
    return Action;
}

void UOpenPocketBaseCompleteOAuth2AsyncAction::Activate()
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
            Failed.Broadcast(
                FOpenPocketBaseAuthResult(),
                FOpenPocketBaseMfaContinuation(),
                Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseCompleteOAuth2AsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(AuthCollection).CompleteOAuth2(
        MoveTemp(Callback),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
            UOpenPocketBaseCompleteOAuth2AsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess() &&
                    Result.GetValue().Status == EOpenPocketBaseAuthAttemptStatus::Authenticated)
                {
                    Action->Success.Broadcast(
                        Result.GetValue().Authentication,
                        FOpenPocketBaseMfaContinuation(),
                        FOpenPocketBaseError());
                }
                else if (Result.IsSuccess())
                {
                    Action->MfaRequired.Broadcast(
                        FOpenPocketBaseAuthResult(),
                        Result.GetValue().Mfa,
                        FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(
                        FOpenPocketBaseAuthResult(),
                        FOpenPocketBaseMfaContinuation(),
                        Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(
                        FOpenPocketBaseAuthResult(),
                        FOpenPocketBaseMfaContinuation(),
                        Result.GetError());
                }
            }
            Action->Finish();
        });
}

void UOpenPocketBaseCompleteOAuth2AsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(
        FOpenPocketBaseAuthResult(),
        FOpenPocketBaseMfaContinuation(),
        MakeCancelledError());
}

UOpenPocketBaseAssistedOAuth2AsyncAction*
UOpenPocketBaseAssistedOAuth2AsyncAction::LogInWithOAuth2(
    FOpenPocketBaseCollection InAuthCollection,
    FOpenPocketBaseAssistedOAuth2Options InOptions)
{
    UOpenPocketBaseAssistedOAuth2AsyncAction* Action =
        NewObject<UOpenPocketBaseAssistedOAuth2AsyncAction>();
    Action->Client = InAuthCollection.Client;
    Action->AuthCollection = MoveTemp(InAuthCollection.Name);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
    return Action;
}

void UOpenPocketBaseAssistedOAuth2AsyncAction::Activate()
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
            Failed.Broadcast(
                FOpenPocketBaseAuthResult(),
                FOpenPocketBaseMfaContinuation(),
                Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseAssistedOAuth2AsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(AuthCollection).AuthWithOAuth2(
        MoveTemp(Options),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
            UOpenPocketBaseAssistedOAuth2AsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess() &&
                    Result.GetValue().Status == EOpenPocketBaseAuthAttemptStatus::Authenticated)
                {
                    Action->Success.Broadcast(
                        Result.GetValue().Authentication,
                        FOpenPocketBaseMfaContinuation(),
                        FOpenPocketBaseError());
                }
                else if (Result.IsSuccess())
                {
                    Action->MfaRequired.Broadcast(
                        FOpenPocketBaseAuthResult(),
                        Result.GetValue().Mfa,
                        FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(
                        FOpenPocketBaseAuthResult(),
                        FOpenPocketBaseMfaContinuation(),
                        Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(
                        FOpenPocketBaseAuthResult(),
                        FOpenPocketBaseMfaContinuation(),
                        Result.GetError());
                }
            }
            Action->Finish();
        });
}

void UOpenPocketBaseAssistedOAuth2AsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(
        FOpenPocketBaseAuthResult(),
        FOpenPocketBaseMfaContinuation(),
        MakeCancelledError());
}

UOpenPocketBaseAccountAsyncAction* UOpenPocketBaseAccountAsyncAction::CreateAction(
    const EOpenPocketBaseAccountActionKind InKind,
    FOpenPocketBaseCollection InAuthCollection,
    FString InPrimary,
    FString InSecondary,
    FString InTertiary,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAccountAsyncAction* Action =
        NewObject<UOpenPocketBaseAccountAsyncAction>();
    Action->Client = InAuthCollection.Client;
    Action->Kind = InKind;
    Action->AuthCollection = MoveTemp(InAuthCollection.Name);
    Action->Primary = MoveTemp(InPrimary);
    Action->Secondary = MoveTemp(InSecondary);
    Action->Tertiary = MoveTemp(InTertiary);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
    return Action;
}

UOpenPocketBaseAccountAsyncAction* UOpenPocketBaseAccountAsyncAction::RequestPasswordReset(
    FOpenPocketBaseCollection AuthCollection,
    FString Email,
    FOpenPocketBaseRequestOptions Options)
{
    return CreateAction(
        EOpenPocketBaseAccountActionKind::RequestPasswordReset,
        MoveTemp(AuthCollection),
        MoveTemp(Email),
        {},
        {},
        MoveTemp(Options));
}

UOpenPocketBaseAccountAsyncAction* UOpenPocketBaseAccountAsyncAction::ConfirmPasswordReset(
    FOpenPocketBaseCollection AuthCollection,
    FString Token,
    FString NewPassword,
    FString ConfirmPassword,
    FOpenPocketBaseRequestOptions Options)
{
    return CreateAction(
        EOpenPocketBaseAccountActionKind::ConfirmPasswordReset,
        MoveTemp(AuthCollection),
        MoveTemp(Token),
        MoveTemp(NewPassword),
        MoveTemp(ConfirmPassword),
        MoveTemp(Options));
}

UOpenPocketBaseAccountAsyncAction* UOpenPocketBaseAccountAsyncAction::RequestVerification(
    FOpenPocketBaseCollection AuthCollection,
    FString Email,
    FOpenPocketBaseRequestOptions Options)
{
    return CreateAction(
        EOpenPocketBaseAccountActionKind::RequestVerification,
        MoveTemp(AuthCollection),
        MoveTemp(Email),
        {},
        {},
        MoveTemp(Options));
}

UOpenPocketBaseAccountAsyncAction* UOpenPocketBaseAccountAsyncAction::ConfirmVerification(
    FOpenPocketBaseCollection AuthCollection,
    FString Token,
    FOpenPocketBaseRequestOptions Options)
{
    return CreateAction(
        EOpenPocketBaseAccountActionKind::ConfirmVerification,
        MoveTemp(AuthCollection),
        MoveTemp(Token),
        {},
        {},
        MoveTemp(Options));
}

UOpenPocketBaseAccountAsyncAction* UOpenPocketBaseAccountAsyncAction::RequestEmailChange(
    FOpenPocketBaseCollection AuthCollection,
    FString NewEmail,
    FOpenPocketBaseRequestOptions Options)
{
    return CreateAction(
        EOpenPocketBaseAccountActionKind::RequestEmailChange,
        MoveTemp(AuthCollection),
        MoveTemp(NewEmail),
        {},
        {},
        MoveTemp(Options));
}

UOpenPocketBaseAccountAsyncAction* UOpenPocketBaseAccountAsyncAction::ConfirmEmailChange(
    FOpenPocketBaseCollection AuthCollection,
    FString Token,
    FString CurrentPassword,
    FOpenPocketBaseRequestOptions Options)
{
    return CreateAction(
        EOpenPocketBaseAccountActionKind::ConfirmEmailChange,
        MoveTemp(AuthCollection),
        MoveTemp(Token),
        MoveTemp(CurrentPassword),
        {},
        MoveTemp(Options));
}

UOpenPocketBaseAccountAsyncAction* UOpenPocketBaseAccountAsyncAction::UnlinkExternalAuth(
    FOpenPocketBaseCollection AuthCollection,
    FString RecordId,
    FString Provider,
    FOpenPocketBaseRequestOptions Options)
{
    return CreateAction(
        EOpenPocketBaseAccountActionKind::UnlinkExternalAuth,
        MoveTemp(AuthCollection),
        MoveTemp(RecordId),
        MoveTemp(Provider),
        {},
        MoveTemp(Options));
}

void UOpenPocketBaseAccountAsyncAction::Activate()
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

    const TWeakObjectPtr<UOpenPocketBaseAccountAsyncAction> WeakThis(this);
    FOpenPocketBaseBoolCallback OnComplete =
        [WeakThis](TOpenPocketBaseResult<bool>&& Result)
        {
            UOpenPocketBaseAccountAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    Action->Success.Broadcast(FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(Result.GetError());
                }
            }
            Action->Finish();
        };

    FOpenPocketBaseCollectionService Auth = NativeClient->Collection(AuthCollection);
    switch (Kind)
    {
    case EOpenPocketBaseAccountActionKind::RequestPasswordReset:
        RequestHandle = Auth.RequestPasswordReset(
            MoveTemp(Primary), MoveTemp(OnComplete), MoveTemp(Options));
        break;
    case EOpenPocketBaseAccountActionKind::ConfirmPasswordReset:
        RequestHandle = Auth.ConfirmPasswordReset(
            MoveTemp(Primary),
            MoveTemp(Secondary),
            MoveTemp(Tertiary),
            MoveTemp(OnComplete),
            MoveTemp(Options));
        break;
    case EOpenPocketBaseAccountActionKind::RequestVerification:
        RequestHandle = Auth.RequestVerification(
            MoveTemp(Primary), MoveTemp(OnComplete), MoveTemp(Options));
        break;
    case EOpenPocketBaseAccountActionKind::ConfirmVerification:
        RequestHandle = Auth.ConfirmVerification(
            MoveTemp(Primary), MoveTemp(OnComplete), MoveTemp(Options));
        break;
    case EOpenPocketBaseAccountActionKind::RequestEmailChange:
        RequestHandle = Auth.RequestEmailChange(
            MoveTemp(Primary), MoveTemp(OnComplete), MoveTemp(Options));
        break;
    case EOpenPocketBaseAccountActionKind::ConfirmEmailChange:
        RequestHandle = Auth.ConfirmEmailChange(
            MoveTemp(Primary),
            MoveTemp(Secondary),
            MoveTemp(OnComplete),
            MoveTemp(Options));
        break;
    case EOpenPocketBaseAccountActionKind::UnlinkExternalAuth:
        RequestHandle = Auth.UnlinkExternalAuth(
            MoveTemp(Primary),
            MoveTemp(Secondary),
            MoveTemp(OnComplete),
            MoveTemp(Options));
        break;
    }
}

void UOpenPocketBaseAccountAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(MakeCancelledError());
}

UOpenPocketBaseListExternalAuthsAsyncAction*
UOpenPocketBaseListExternalAuthsAsyncAction::ListLinkedExternalAuths(
    FOpenPocketBaseCollection InAuthCollection,
    FString InRecordId,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseListExternalAuthsAsyncAction* Action =
        NewObject<UOpenPocketBaseListExternalAuthsAsyncAction>();
    Action->Client = InAuthCollection.Client;
    Action->AuthCollection = MoveTemp(InAuthCollection.Name);
    Action->RecordId = MoveTemp(InRecordId);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
    return Action;
}

void UOpenPocketBaseListExternalAuthsAsyncAction::Activate()
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
            Failed.Broadcast(FOpenPocketBaseExternalAuthList(), Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseListExternalAuthsAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(AuthCollection).ListExternalAuths(
        MoveTemp(RecordId),
        [WeakThis](TOpenPocketBaseResult<TArray<FOpenPocketBaseExternalAuth>>&& Result)
        {
            UOpenPocketBaseListExternalAuthsAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }
            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess())
                {
                    FOpenPocketBaseExternalAuthList ExternalAuths;
                    ExternalAuths.Items = MoveTemp(Result.GetValue());
                    Action->Success.Broadcast(ExternalAuths, FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(FOpenPocketBaseExternalAuthList(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseExternalAuthList(), Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseListExternalAuthsAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseExternalAuthList(), MakeCancelledError());
}

UOpenPocketBasePasswordAuthAsyncAction* UOpenPocketBasePasswordAuthAsyncAction::LogInWithPassword(
    FOpenPocketBaseCollection InAuthCollection,
    FString InIdentity,
    FString InPassword,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBasePasswordAuthAsyncAction* Action = NewObject<UOpenPocketBasePasswordAuthAsyncAction>();
    Action->Client = InAuthCollection.Client;
    Action->AuthCollection = MoveTemp(InAuthCollection.Name);
    Action->Identity = MoveTemp(InIdentity);
    Action->Password = MoveTemp(InPassword);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
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
            Failed.Broadcast(
                FOpenPocketBaseAuthResult(),
                FOpenPocketBaseMfaContinuation(),
                Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBasePasswordAuthAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(AuthCollection).AuthWithPassword(
        MoveTemp(Identity),
        MoveTemp(Password),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
        {
            UOpenPocketBasePasswordAuthAsyncAction* Action = WeakThis.Get();
            if (Action == nullptr || !Action->TryBeginTerminal())
            {
                return;
            }

            if (Action->ShouldBroadcastDelegates())
            {
                if (Result.IsSuccess() &&
                    Result.GetValue().Status == EOpenPocketBaseAuthAttemptStatus::Authenticated)
                {
                    Action->Success.Broadcast(
                        Result.GetValue().Authentication,
                        FOpenPocketBaseMfaContinuation(),
                        FOpenPocketBaseError());
                }
                else if (Result.IsSuccess())
                {
                    Action->MfaRequired.Broadcast(
                        FOpenPocketBaseAuthResult(),
                        Result.GetValue().Mfa,
                        FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(
                        FOpenPocketBaseAuthResult(),
                        FOpenPocketBaseMfaContinuation(),
                        Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(
                        FOpenPocketBaseAuthResult(),
                        FOpenPocketBaseMfaContinuation(),
                        Result.GetError());
                }
            }
            Action->Finish();
        },
        Options);
}

void UOpenPocketBasePasswordAuthAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(
        FOpenPocketBaseAuthResult(),
        FOpenPocketBaseMfaContinuation(),
        MakeCancelledError());
}
