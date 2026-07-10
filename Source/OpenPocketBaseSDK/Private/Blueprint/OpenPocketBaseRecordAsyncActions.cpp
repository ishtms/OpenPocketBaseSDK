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
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseHealthAsyncAction* Action =
        NewObject<UOpenPocketBaseHealthAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
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
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FOpenPocketBaseCustomRouteRequest InRequest)
{
    UOpenPocketBaseCustomRouteAsyncAction* Action =
        NewObject<UOpenPocketBaseCustomRouteAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Request = MoveTemp(InRequest);
    Action->RegisterWithGameInstance(WorldContextObject);
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
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InCollection,
    FOpenPocketBaseFilter InFilter,
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
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InAuthCollection,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseListAuthMethodsAsyncAction* Action =
        NewObject<UOpenPocketBaseListAuthMethodsAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->AuthCollection = MoveTemp(InAuthCollection);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
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
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InAuthCollection,
    FString InEmail,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseRequestOtpAsyncAction* Action = NewObject<UOpenPocketBaseRequestOtpAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->AuthCollection = MoveTemp(InAuthCollection);
    Action->Email = MoveTemp(InEmail);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
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
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InAuthCollection,
    FString InOtpId,
    FString InPassword,
    FOpenPocketBaseMfaContinuation InMfa,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseOtpAuthAsyncAction* Action = NewObject<UOpenPocketBaseOtpAuthAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->AuthCollection = MoveTemp(InAuthCollection);
    Action->OtpId = MoveTemp(InOtpId);
    Action->Password = MoveTemp(InPassword);
    Action->Mfa = MoveTemp(InMfa);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
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
    RequestHandle = NativeClient->Collection(AuthCollection).AuthenticateWithOtp(
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
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InAuthCollection,
    FOpenPocketBaseOAuth2StartOptions InOptions)
{
    UOpenPocketBaseBeginOAuth2AsyncAction* Action =
        NewObject<UOpenPocketBaseBeginOAuth2AsyncAction>();
    Action->Client = PocketBaseClient;
    Action->AuthCollection = MoveTemp(InAuthCollection);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
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
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InAuthCollection,
    FOpenPocketBaseOAuth2Callback InCallback)
{
    UOpenPocketBaseCompleteOAuth2AsyncAction* Action =
        NewObject<UOpenPocketBaseCompleteOAuth2AsyncAction>();
    Action->Client = PocketBaseClient;
    Action->AuthCollection = MoveTemp(InAuthCollection);
    Action->Callback = MoveTemp(InCallback);
    Action->RegisterWithGameInstance(WorldContextObject);
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
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InAuthCollection,
    FOpenPocketBaseAssistedOAuth2Options InOptions)
{
    UOpenPocketBaseAssistedOAuth2AsyncAction* Action =
        NewObject<UOpenPocketBaseAssistedOAuth2AsyncAction>();
    Action->Client = PocketBaseClient;
    Action->AuthCollection = MoveTemp(InAuthCollection);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
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
    RequestHandle = NativeClient->Collection(AuthCollection).AuthenticateWithOAuth2(
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
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    const EOpenPocketBaseAccountActionKind InKind,
    FString InAuthCollection,
    FString InPrimary,
    FString InSecondary,
    FString InTertiary,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseAccountAsyncAction* Action =
        NewObject<UOpenPocketBaseAccountAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Kind = InKind;
    Action->AuthCollection = MoveTemp(InAuthCollection);
    Action->Primary = MoveTemp(InPrimary);
    Action->Secondary = MoveTemp(InSecondary);
    Action->Tertiary = MoveTemp(InTertiary);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

UOpenPocketBaseAccountAsyncAction* UOpenPocketBaseAccountAsyncAction::RequestPasswordReset(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString AuthCollection,
    FString Email,
    FOpenPocketBaseRequestOptions Options)
{
    return CreateAction(
        WorldContextObject,
        PocketBaseClient,
        EOpenPocketBaseAccountActionKind::RequestPasswordReset,
        MoveTemp(AuthCollection),
        MoveTemp(Email),
        {},
        {},
        MoveTemp(Options));
}

UOpenPocketBaseAccountAsyncAction* UOpenPocketBaseAccountAsyncAction::ConfirmPasswordReset(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString AuthCollection,
    FString Token,
    FString Password,
    FString PasswordConfirm,
    FOpenPocketBaseRequestOptions Options)
{
    return CreateAction(
        WorldContextObject,
        PocketBaseClient,
        EOpenPocketBaseAccountActionKind::ConfirmPasswordReset,
        MoveTemp(AuthCollection),
        MoveTemp(Token),
        MoveTemp(Password),
        MoveTemp(PasswordConfirm),
        MoveTemp(Options));
}

UOpenPocketBaseAccountAsyncAction* UOpenPocketBaseAccountAsyncAction::RequestVerification(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString AuthCollection,
    FString Email,
    FOpenPocketBaseRequestOptions Options)
{
    return CreateAction(
        WorldContextObject,
        PocketBaseClient,
        EOpenPocketBaseAccountActionKind::RequestVerification,
        MoveTemp(AuthCollection),
        MoveTemp(Email),
        {},
        {},
        MoveTemp(Options));
}

UOpenPocketBaseAccountAsyncAction* UOpenPocketBaseAccountAsyncAction::ConfirmVerification(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString AuthCollection,
    FString Token,
    FOpenPocketBaseRequestOptions Options)
{
    return CreateAction(
        WorldContextObject,
        PocketBaseClient,
        EOpenPocketBaseAccountActionKind::ConfirmVerification,
        MoveTemp(AuthCollection),
        MoveTemp(Token),
        {},
        {},
        MoveTemp(Options));
}

UOpenPocketBaseAccountAsyncAction* UOpenPocketBaseAccountAsyncAction::RequestEmailChange(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString AuthCollection,
    FString NewEmail,
    FOpenPocketBaseRequestOptions Options)
{
    return CreateAction(
        WorldContextObject,
        PocketBaseClient,
        EOpenPocketBaseAccountActionKind::RequestEmailChange,
        MoveTemp(AuthCollection),
        MoveTemp(NewEmail),
        {},
        {},
        MoveTemp(Options));
}

UOpenPocketBaseAccountAsyncAction* UOpenPocketBaseAccountAsyncAction::ConfirmEmailChange(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString AuthCollection,
    FString Token,
    FString Password,
    FOpenPocketBaseRequestOptions Options)
{
    return CreateAction(
        WorldContextObject,
        PocketBaseClient,
        EOpenPocketBaseAccountActionKind::ConfirmEmailChange,
        MoveTemp(AuthCollection),
        MoveTemp(Token),
        MoveTemp(Password),
        {},
        MoveTemp(Options));
}

UOpenPocketBaseAccountAsyncAction* UOpenPocketBaseAccountAsyncAction::UnlinkExternalAuth(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString AuthCollection,
    FString RecordId,
    FString Provider,
    FOpenPocketBaseRequestOptions Options)
{
    return CreateAction(
        WorldContextObject,
        PocketBaseClient,
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
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InAuthCollection,
    FString InRecordId,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseListExternalAuthsAsyncAction* Action =
        NewObject<UOpenPocketBaseListExternalAuthsAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->AuthCollection = MoveTemp(InAuthCollection);
    Action->RecordId = MoveTemp(InRecordId);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
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
            Failed.Broadcast(
                FOpenPocketBaseAuthResult(),
                FOpenPocketBaseMfaContinuation(),
                Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBasePasswordAuthAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Collection(AuthCollection).AuthenticateWithPassword(
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
