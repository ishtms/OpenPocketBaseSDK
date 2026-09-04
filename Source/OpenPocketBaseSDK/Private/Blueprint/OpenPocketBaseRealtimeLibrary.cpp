// Copyright 2026 Ishtmeet Singh.

#include "OpenPocketBaseRealtimeLibrary.h"

namespace
{
FOpenPocketBaseError MakeRealtimeEntryError(const TCHAR* Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
    Error.Message = Message;
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
        OutError = MakeRealtimeEntryError(TEXT("The realtime subscription did not start and no lower-level error was returned. Check the collection, topic, and client state."));
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
        Error = MakeRealtimeEntryError(TEXT("The collection is missing, stale, or belongs to an inactive client. Choose the collection again before subscribing."));
        return false;
    }
    return SetSubscriptionResult(
        Collection.Client->SubscribeToRecords(MoveTemp(Collection.Reference), Options, Error),
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
        Error = MakeRealtimeEntryError(TEXT("The collection is missing, stale, or belongs to an inactive client. Choose the collection again before subscribing to a record."));
        return false;
    }
    return SetSubscriptionResult(
        Collection.Client->SubscribeToRecord(
            MoveTemp(Collection.Reference),
            RecordId,
            Options,
            Error),
        Subscription,
        Error);
}

bool UOpenPocketBaseRealtimeLibrary::DynamicSubscribeToTopic(
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
        Error = MakeRealtimeEntryError(TEXT("The PocketBase client is missing or has already shut down. Create or retrieve an active client before subscribing to a dynamic topic."));
        return false;
    }
    return SetSubscriptionResult(
        PocketBaseClient->DynamicSubscribeToTopic(Topic, Options, Error),
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
