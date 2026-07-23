#include "AsyncActions/OpenPocketBaseFileAsyncActions.h"

UOpenPocketBaseGetFileTokenAsyncAction*
UOpenPocketBaseGetFileTokenAsyncAction::GetProtectedFileToken(
    UOpenPocketBaseClient* PocketBaseClient,
    FOpenPocketBaseRequestOptions InOptions)
{
    UOpenPocketBaseGetFileTokenAsyncAction* Action =
        NewObject<UOpenPocketBaseGetFileTokenAsyncAction>();
    Action->Client = PocketBaseClient;
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
    return Action;
}

void UOpenPocketBaseGetFileTokenAsyncAction::Activate()
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Client != nullptr ? Client->GetNativeClient() : nullptr;
    if (!NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        if (TryBeginTerminal())
        {
            FOpenPocketBaseError Error;
            Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            Error.ServerMessage = TEXT("A ready PocketBase client is required.");
            Failed.Broadcast(FOpenPocketBaseFileToken(), Error);
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
                    Action->Success.Broadcast(Result.GetValue(), FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(FOpenPocketBaseFileToken(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseFileToken(), Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseGetFileTokenAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseFileToken(), MakeCancelledError());
}

UOpenPocketBaseDownloadFileAsyncAction* UOpenPocketBaseDownloadFileAsyncAction::DownloadFile(
    FOpenPocketBaseCollection InCollection,
    FString InRecordId,
    FString InFileName,
    FOpenPocketBaseFileDownloadOptions InOptions,
    FOpenPocketBaseFileToken InToken)
{
    UOpenPocketBaseDownloadFileAsyncAction* Action =
        NewObject<UOpenPocketBaseDownloadFileAsyncAction>();
    Action->Client = InCollection.Client;
    Action->Collection = MoveTemp(InCollection.Reference);
    Action->RecordId = MoveTemp(InRecordId);
    Action->FileName = MoveTemp(InFileName);
    Action->Options = MoveTemp(InOptions);
    Action->Token = MoveTemp(InToken);
    Action->RegisterWithGameInstance(Action->Client);
    return Action;
}

void UOpenPocketBaseDownloadFileAsyncAction::Activate()
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Client != nullptr ? Client->GetNativeClient() : nullptr;
    if (!NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        if (TryBeginTerminal())
        {
            FOpenPocketBaseError Error;
            Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            Error.ServerMessage = TEXT("A ready PocketBase client is required.");
            Failed.Broadcast(
                FOpenPocketBaseFileDownloadResult(),
                FOpenPocketBaseTransferProgress(),
                Error);
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
                    Action->Success.Broadcast(
                        Result.GetValue(),
                        FOpenPocketBaseTransferProgress(),
                        FOpenPocketBaseError());
                }
                else if (Result.GetError().Kind == EOpenPocketBaseErrorKind::Cancelled)
                {
                    Action->Cancelled.Broadcast(
                        FOpenPocketBaseFileDownloadResult(),
                        FOpenPocketBaseTransferProgress(),
                        Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(
                        FOpenPocketBaseFileDownloadResult(),
                        FOpenPocketBaseTransferProgress(),
                        Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Token),
        [WeakThis](const FOpenPocketBaseTransferProgress& TransferProgress)
        {
            UOpenPocketBaseDownloadFileAsyncAction* Action = WeakThis.Get();
            if (Action != nullptr && !Action->bTerminal && Action->ShouldBroadcastDelegates())
            {
                Action->Progress.Broadcast(
                    FOpenPocketBaseFileDownloadResult(),
                    TransferProgress,
                    FOpenPocketBaseError());
            }
        });
}

void UOpenPocketBaseDownloadFileAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(
        FOpenPocketBaseFileDownloadResult(),
        FOpenPocketBaseTransferProgress(),
        MakeCancelledError());
}
