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
    FOpenPocketBaseRequestOptions InOptions)
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
