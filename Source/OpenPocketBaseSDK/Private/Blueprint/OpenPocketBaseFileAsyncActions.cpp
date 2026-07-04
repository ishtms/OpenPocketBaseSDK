#include "AsyncActions/OpenPocketBaseFileAsyncActions.h"

UOpenPocketBaseGetFileTokenAsyncAction*
UOpenPocketBaseGetFileTokenAsyncAction::GetProtectedFileToken(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseGetFileTokenAsyncAction* Action =
        NewObject<UOpenPocketBaseGetFileTokenAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseGetFileTokenAsyncAction::Activate()
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

    const TWeakObjectPtr<UOpenPocketBaseGetFileTokenAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Files().GetToken(
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseFileToken>&& Result)
        {
            UOpenPocketBaseGetFileTokenAsyncAction* Action = WeakThis.Get();
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

void UOpenPocketBaseGetFileTokenAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}

UOpenPocketBaseDownloadFileAsyncAction* UOpenPocketBaseDownloadFileAsyncAction::DownloadFile(
    const UObject* WorldContextObject,
    UOpenPocketBaseClient* PocketBaseClient,
    FString InCollection,
    FString InRecordId,
    FString InFileName,
    FOpenPocketBaseFileDownloadOptions InOptions,
    FOpenPocketBaseFileToken InToken)
{
    UOpenPocketBaseDownloadFileAsyncAction* Action =
        NewObject<UOpenPocketBaseDownloadFileAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Collection = MoveTemp(InCollection);
    Action->RecordId = MoveTemp(InRecordId);
    Action->FileName = MoveTemp(InFileName);
    Action->Options = MoveTemp(InOptions);
    Action->Token = MoveTemp(InToken);
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void UOpenPocketBaseDownloadFileAsyncAction::Activate()
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

    const TWeakObjectPtr<UOpenPocketBaseDownloadFileAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->Files().Download(
        MoveTemp(Collection),
        MoveTemp(RecordId),
        MoveTemp(FileName),
        MoveTemp(Options),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&& Result)
        {
            UOpenPocketBaseDownloadFileAsyncAction* Action = WeakThis.Get();
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
        MoveTemp(Token));
}

void UOpenPocketBaseDownloadFileAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast();
}
