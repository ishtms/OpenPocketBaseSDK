#include "AsyncActions/OpenPocketBaseBatchAsyncAction.h"

UOpenPocketBaseSendBatchAsyncAction* UOpenPocketBaseSendBatchAsyncAction::SendBatch(
    FOpenPocketBaseBatchRequest InBatch,
    FOpenPocketBaseBatchOptions InOptions)
{
    UOpenPocketBaseSendBatchAsyncAction* Action = NewObject<UOpenPocketBaseSendBatchAsyncAction>();
    Action->Client = InBatch.GetClient();
    Action->Batch = MoveTemp(InBatch);
    Action->Options = MoveTemp(InOptions);
    Action->RegisterWithGameInstance(Action->Client);
    return Action;
}

void UOpenPocketBaseSendBatchAsyncAction::Activate()
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Client != nullptr ? Client->GetNativeClient() : nullptr;
    if (!Batch.IsValid() || !NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        if (TryBeginTerminal())
        {
            FOpenPocketBaseError Error;
            Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            Error.ServerMessage = Batch.IsValid()
                ? TEXT("A ready PocketBase client is required.")
                : Batch.ErrorMessage;
            Failed.Broadcast(FOpenPocketBaseBatchResult(), Error);
        }
        Finish();
        return;
    }

    const TWeakObjectPtr<UOpenPocketBaseSendBatchAsyncAction> WeakThis(this);
    RequestHandle = NativeClient->SendBatch(
        MoveTemp(Batch),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseBatchResult>&& Result)
        {
            UOpenPocketBaseSendBatchAsyncAction* Action = WeakThis.Get();
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
                    Action->Cancelled.Broadcast(FOpenPocketBaseBatchResult(), Result.GetError());
                }
                else
                {
                    Action->Failed.Broadcast(FOpenPocketBaseBatchResult(), Result.GetError());
                }
            }
            Action->Finish();
        },
        MoveTemp(Options));
}

void UOpenPocketBaseSendBatchAsyncAction::BroadcastCancelled()
{
    Cancelled.Broadcast(FOpenPocketBaseBatchResult(), MakeCancelledError());
}
