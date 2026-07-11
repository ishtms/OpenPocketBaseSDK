#include "OpenPocketBaseRealtimeLibrary.h"

namespace
{
FOpenPocketBaseError MakeRealtimeEntryError(const TCHAR* Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
    Error.ServerMessage = Message;
    return Error;
}

bool SetSubscriptionResult(
    UOpenPocketBaseSubscription* CreatedSubscription,
    UOpenPocketBaseSubscription*& OutSubscription,
    FOpenPocketBaseError& OutError)
{
    OutSubscription = CreatedSubscription;
    if (CreatedSubscription != nullptr)
    {
        OutError = {};
        return true;
    }
    if (!OutError.IsSet())
    {
        OutError = MakeRealtimeEntryError(TEXT("The realtime subscription could not be started."));
    }
    return false;
}
}

bool UOpenPocketBaseRealtimeLibrary::SubscribeToRecords(
    FOpenPocketBaseCollection Collection,
    const FOpenPocketBaseRealtimeOptions& Options,
    UOpenPocketBaseSubscription*& Subscription,
    FOpenPocketBaseError& Error)
{
    Subscription = nullptr;
    Error = {};
    if (!Collection.IsValid())
    {
        Error = MakeRealtimeEntryError(TEXT("A valid PocketBase collection is required."));
        return false;
    }
    return SetSubscriptionResult(
        Collection.Client->SubscribeToRecords(MoveTemp(Collection.Name), Options, Error),
        Subscription,
        Error);
}

bool UOpenPocketBaseRealtimeLibrary::SubscribeToRecord(
    FOpenPocketBaseCollection Collection,
    const FString& RecordId,
    const FOpenPocketBaseRealtimeOptions& Options,
    UOpenPocketBaseSubscription*& Subscription,
    FOpenPocketBaseError& Error)
{
    Subscription = nullptr;
    Error = {};
    if (!Collection.IsValid())
    {
        Error = MakeRealtimeEntryError(TEXT("A valid PocketBase collection is required."));
        return false;
    }
    return SetSubscriptionResult(
        Collection.Client->SubscribeToRecord(
            MoveTemp(Collection.Name),
            RecordId,
            Options,
            Error),
        Subscription,
        Error);
}

bool UOpenPocketBaseRealtimeLibrary::SubscribeToTopic(
    UOpenPocketBaseClient* PocketBaseClient,
    const FString& Topic,
    const FOpenPocketBaseRealtimeOptions& Options,
    UOpenPocketBaseSubscription*& Subscription,
    FOpenPocketBaseError& Error)
{
    Subscription = nullptr;
    Error = {};
    if (PocketBaseClient == nullptr)
    {
        Error = MakeRealtimeEntryError(TEXT("A ready PocketBase client is required."));
        return false;
    }
    return SetSubscriptionResult(
        PocketBaseClient->SubscribeToTopic(Topic, Options, Error),
        Subscription,
        Error);
}

void UOpenPocketBaseRealtimeLibrary::UnsubscribeAllRealtime(
    UOpenPocketBaseClient* PocketBaseClient)
{
    if (PocketBaseClient != nullptr)
    {
        PocketBaseClient->UnsubscribeAllRealtime();
    }
}
