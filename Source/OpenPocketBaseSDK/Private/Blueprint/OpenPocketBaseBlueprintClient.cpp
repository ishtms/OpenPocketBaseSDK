#include "OpenPocketBaseBlueprintClient.h"

#include "UObject/Package.h"

bool FOpenPocketBaseCollection::IsValid() const
{
    return Client != nullptr && Client->IsReady() && !Reference.Name.IsEmpty();
}

bool FOpenPocketBaseWritableCollection::IsValid() const
{
    return Client != nullptr && Client->IsReady() &&
        FOpenPocketBaseWritableCollectionRef::Accepts(Reference);
}

bool FOpenPocketBaseAuthCollection::IsValid() const
{
    return Client != nullptr && Client->IsReady() &&
        FOpenPocketBaseAuthCollectionRef::Accepts(Reference);
}

UOpenPocketBaseClient* UOpenPocketBaseClient::Create(
    UObject* Outer,
    const FOpenPocketBaseClientConfig& Config,
    FOpenPocketBaseError& OutError)
{
    FOpenPocketBaseClientResult Result = FOpenPocketBaseClient::Create(Config);
    if (!Result.IsSuccess())
    {
        OutError = Result.GetError();
        return nullptr;
    }
    OutError = {};
    return Wrap(Outer, Result.TakeValue());
}

UOpenPocketBaseClient* UOpenPocketBaseClient::Create(
    UObject* Outer,
    const FOpenPocketBaseClientConfig& Config,
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
    FOpenPocketBaseError& OutError)
{
    FOpenPocketBaseClientDependencies Dependencies;
    Dependencies.Transport = MoveTemp(Transport);
    FOpenPocketBaseClientResult Result =
        FOpenPocketBaseClient::Create(Config, MoveTemp(Dependencies));
    if (!Result.IsSuccess())
    {
        OutError = Result.GetError();
        return nullptr;
    }
    OutError = {};
    return Wrap(Outer, Result.TakeValue());
}

UOpenPocketBaseClient* UOpenPocketBaseClient::Wrap(
    UObject* Outer,
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client)
{
    if (!Client.IsValid() || Client->IsShutdown())
    {
        return nullptr;
    }

    UOpenPocketBaseClient* Wrapper = NewObject<UOpenPocketBaseClient>(
        Outer != nullptr ? Outer : GetTransientPackage());
    Wrapper->NativeClient = MoveTemp(Client);
    Wrapper->BindNativeSessionEvents();
    return Wrapper;
}

UWorld* UOpenPocketBaseClient::GetWorld() const
{
    return GetOuter() != nullptr ? GetOuter()->GetWorld() : nullptr;
}

TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> UOpenPocketBaseClient::GetNativeClient() const
{
    return NativeClient;
}

bool UOpenPocketBaseClient::IsReady() const
{
    return NativeClient.IsValid() && !NativeClient->IsShutdown();
}

FString UOpenPocketBaseClient::GetBaseUrl() const
{
    return NativeClient.IsValid() ? NativeClient->GetBaseUrl() : FString();
}

FOpenPocketBaseCollection UOpenPocketBaseClient::Collection(
    FOpenPocketBaseCollectionRef Reference)
{
    FOpenPocketBaseCollection Collection;
    Collection.Client = this;
    Collection.Reference = MoveTemp(Reference);
    return Collection;
}

FOpenPocketBaseWritableCollection UOpenPocketBaseClient::WritableCollection(
    FOpenPocketBaseWritableCollectionRef Reference)
{
    FOpenPocketBaseWritableCollection Collection;
    Collection.Client = this;
    Collection.Reference = MoveTemp(Reference);
    return Collection;
}

FOpenPocketBaseAuthCollection UOpenPocketBaseClient::AuthCollection(
    FOpenPocketBaseAuthCollectionRef Reference)
{
    FOpenPocketBaseAuthCollection Collection;
    Collection.Client = this;
    Collection.Reference = MoveTemp(Reference);
    return Collection;
}

FOpenPocketBaseCollection UOpenPocketBaseClient::DynamicCollection(FString Name)
{
    Name.TrimStartAndEndInline();
    FOpenPocketBaseCollection Collection;
    Collection.Client = this;
    Collection.Reference.Name = MoveTemp(Name);
    return Collection;
}

FOpenPocketBaseCapabilityInfo UOpenPocketBaseClient::GetCapability(
    const EOpenPocketBaseCapability Capability) const
{
    if (NativeClient.IsValid())
    {
        return NativeClient->GetCapability(Capability);
    }

    FOpenPocketBaseCapabilityInfo Info;
    Info.Capability = Capability;
    Info.Status = EOpenPocketBaseCapabilityStatus::Unavailable;
    Info.Reason = TEXT("The PocketBase client is not ready.");
    return Info;
}

FOpenPocketBaseCapabilityReport UOpenPocketBaseClient::GetCapabilityReport() const
{
    return NativeClient.IsValid()
        ? NativeClient->GetCapabilityReport()
        : FOpenPocketBaseCapabilityReport();
}

bool UOpenPocketBaseClient::IsAuthenticated() const
{
    return NativeClient.IsValid() && NativeClient->IsAuthenticated();
}

bool UOpenPocketBaseClient::GetCurrentAuthRecord(FOpenPocketBaseRecord& OutRecord) const
{
    return NativeClient.IsValid() && NativeClient->GetCurrentAuthRecord(OutRecord);
}

bool UOpenPocketBaseClient::TryGetCurrentAuthRecord(FOpenPocketBaseRecord& OutRecord) const
{
    return GetCurrentAuthRecord(OutRecord);
}

bool UOpenPocketBaseClient::GetCurrentSession(FOpenPocketBaseSessionSnapshot& OutSession) const
{
    return NativeClient.IsValid() && NativeClient->GetCurrentSession(OutSession);
}

bool UOpenPocketBaseClient::TryGetCurrentSession(FOpenPocketBaseSessionSnapshot& OutSession) const
{
    return GetCurrentSession(OutSession);
}

void UOpenPocketBaseClient::Logout()
{
    if (NativeClient.IsValid())
    {
        NativeClient->Logout();
    }
}

void UOpenPocketBaseClient::Shutdown()
{
    const TArray<TObjectPtr<UOpenPocketBaseSubscription>> Subscriptions = ActiveSubscriptions;
    for (UOpenPocketBaseSubscription* Subscription : Subscriptions)
    {
        if (Subscription != nullptr)
        {
            Subscription->Unsubscribe();
        }
    }
    ActiveSubscriptions.Reset();

    if (NativeClient.IsValid())
    {
        NativeClient->Shutdown();
    }
}

UOpenPocketBaseSubscription* UOpenPocketBaseClient::SubscribeToRecords(
    FOpenPocketBaseCollectionRef Collection,
    const FOpenPocketBaseRealtimeOptions& Options,
    FOpenPocketBaseError& OutError)
{
    OutError = {};
    if (!IsReady())
    {
        OutError = FOpenPocketBaseError();
        OutError.Kind = EOpenPocketBaseErrorKind::Cancelled;
        OutError.ServerMessage = TEXT("The PocketBase client is not ready.");
        return nullptr;
    }

    UOpenPocketBaseSubscription* Subscription = NewObject<UOpenPocketBaseSubscription>(this);
    FOpenPocketBaseSubscriptionResult Result = NativeClient->Collection(MoveTemp(Collection))
        .SubscribeToRecords(MakeRealtimeCallbacks(Subscription), Options);
    if (!Result.IsSuccess())
    {
        OutError = Result.GetError();
        return nullptr;
    }
    FOpenPocketBaseSubscriptionHandle Handle = Result.TakeValue();
    Subscription->Initialize(this, MoveTemp(Handle));
    RetainSubscription(Subscription);
    return Subscription;
}

UOpenPocketBaseSubscription* UOpenPocketBaseClient::SubscribeToRecord(
    FOpenPocketBaseCollectionRef Collection,
    FString RecordId,
    const FOpenPocketBaseRealtimeOptions& Options,
    FOpenPocketBaseError& OutError)
{
    OutError = {};
    if (!IsReady())
    {
        OutError = FOpenPocketBaseError();
        OutError.Kind = EOpenPocketBaseErrorKind::Cancelled;
        OutError.ServerMessage = TEXT("The PocketBase client is not ready.");
        return nullptr;
    }

    UOpenPocketBaseSubscription* Subscription = NewObject<UOpenPocketBaseSubscription>(this);
    FOpenPocketBaseSubscriptionResult Result = NativeClient->Collection(MoveTemp(Collection))
        .SubscribeToRecord(
            MoveTemp(RecordId),
            MakeRealtimeCallbacks(Subscription),
            Options);
    if (!Result.IsSuccess())
    {
        OutError = Result.GetError();
        return nullptr;
    }
    FOpenPocketBaseSubscriptionHandle Handle = Result.TakeValue();
    Subscription->Initialize(this, MoveTemp(Handle));
    RetainSubscription(Subscription);
    return Subscription;
}

UOpenPocketBaseSubscription* UOpenPocketBaseClient::DynamicSubscribeToTopic(
    FString Topic,
    const FOpenPocketBaseRealtimeOptions& Options,
    FOpenPocketBaseError& OutError)
{
    OutError = {};
    if (!IsReady())
    {
        OutError = FOpenPocketBaseError();
        OutError.Kind = EOpenPocketBaseErrorKind::Cancelled;
        OutError.ServerMessage = TEXT("The PocketBase client is not ready.");
        return nullptr;
    }

    UOpenPocketBaseSubscription* Subscription = NewObject<UOpenPocketBaseSubscription>(this);
    FOpenPocketBaseSubscriptionResult Result = NativeClient->DynamicSubscribe(
        MoveTemp(Topic),
        MakeRealtimeCallbacks(Subscription),
        Options);
    if (!Result.IsSuccess())
    {
        OutError = Result.GetError();
        return nullptr;
    }
    FOpenPocketBaseSubscriptionHandle Handle = Result.TakeValue();
    Subscription->Initialize(this, MoveTemp(Handle));
    RetainSubscription(Subscription);
    return Subscription;
}

void UOpenPocketBaseClient::UnsubscribeAllRealtime()
{
    const TArray<TObjectPtr<UOpenPocketBaseSubscription>> Subscriptions = ActiveSubscriptions;
    for (UOpenPocketBaseSubscription* Subscription : Subscriptions)
    {
        if (Subscription != nullptr)
        {
            Subscription->Unsubscribe();
        }
    }
    ActiveSubscriptions.Reset();
    if (NativeClient.IsValid())
    {
        NativeClient->UnsubscribeAllRealtime();
    }
}

void UOpenPocketBaseClient::BeginDestroy()
{
    if (NativeClient.IsValid() && SessionChangedHandle.IsValid())
    {
        NativeClient->OnSessionChanged().Remove(SessionChangedHandle);
        SessionChangedHandle.Reset();
    }
    Shutdown();
    NativeClient.Reset();
    Super::BeginDestroy();
}

void UOpenPocketBaseClient::BindNativeSessionEvents()
{
    if (!NativeClient.IsValid())
    {
        return;
    }
    SessionChangedHandle = NativeClient->OnSessionChanged().AddUObject(
        this,
        &UOpenPocketBaseClient::HandleNativeSessionChanged);
}

void UOpenPocketBaseClient::HandleNativeSessionChanged(
    const FOpenPocketBaseSessionSnapshot& Session)
{
    SessionChanged.Broadcast(Session);
}

FOpenPocketBaseRealtimeCallbacks UOpenPocketBaseClient::MakeRealtimeCallbacks(
    UOpenPocketBaseSubscription* Subscription) const
{
    const TWeakObjectPtr<UOpenPocketBaseSubscription> WeakSubscription = Subscription;
    FOpenPocketBaseRealtimeCallbacks Callbacks;
    Callbacks.OnEvent = [WeakSubscription](const FOpenPocketBaseRealtimeEvent& Event)
    {
        if (UOpenPocketBaseSubscription* Pinned = WeakSubscription.Get())
        {
            Pinned->HandleNativeEvent(Event);
        }
    };
    Callbacks.OnConnectionStateChanged = [WeakSubscription](
        const EOpenPocketBaseRealtimeConnectionState State)
    {
        if (UOpenPocketBaseSubscription* Pinned = WeakSubscription.Get())
        {
            Pinned->HandleNativeState(State);
        }
    };
    Callbacks.OnError = [WeakSubscription](const FOpenPocketBaseError& Error)
    {
        if (UOpenPocketBaseSubscription* Pinned = WeakSubscription.Get())
        {
            Pinned->HandleNativeError(Error);
        }
    };
    Callbacks.OnResyncRequired = [WeakSubscription]()
    {
        if (UOpenPocketBaseSubscription* Pinned = WeakSubscription.Get())
        {
            Pinned->HandleNativeResyncRequired();
        }
    };
    return Callbacks;
}

void UOpenPocketBaseClient::RetainSubscription(UOpenPocketBaseSubscription* Subscription)
{
    if (Subscription != nullptr)
    {
        ActiveSubscriptions.AddUnique(Subscription);
    }
}

void UOpenPocketBaseClient::ReleaseSubscription(UOpenPocketBaseSubscription* Subscription)
{
    ActiveSubscriptions.Remove(Subscription);
}
