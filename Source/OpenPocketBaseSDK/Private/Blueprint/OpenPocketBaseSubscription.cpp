#include "OpenPocketBaseSubscription.h"

#include "OpenPocketBaseBlueprintClient.h"

void UOpenPocketBaseSubscription::Initialize(
    UOpenPocketBaseClient* InOwner,
    FOpenPocketBaseSubscriptionHandle InHandle)
{
    OwnerClient = InOwner;
    NativeHandle = MakeUnique<FOpenPocketBaseSubscriptionHandle>(MoveTemp(InHandle));
}

void UOpenPocketBaseSubscription::Unsubscribe()
{
    if (ConnectionState == EOpenPocketBaseRealtimeConnectionState::Stopped)
    {
        return;
    }

    if (NativeHandle.IsValid())
    {
        NativeHandle->Unsubscribe();
        NativeHandle.Reset();
    }
    HandleNativeState(EOpenPocketBaseRealtimeConnectionState::Stopped);
}

bool UOpenPocketBaseSubscription::IsActive() const
{
    return NativeHandle.IsValid() && NativeHandle->IsActive() &&
        ConnectionState != EOpenPocketBaseRealtimeConnectionState::Stopped;
}

EOpenPocketBaseRealtimeConnectionState UOpenPocketBaseSubscription::GetConnectionState() const
{
    return ConnectionState;
}

void UOpenPocketBaseSubscription::BeginDestroy()
{
    Unsubscribe();
    Super::BeginDestroy();
}

void UOpenPocketBaseSubscription::HandleNativeEvent(
    const FOpenPocketBaseRealtimeEvent& Event)
{
    if (ConnectionState != EOpenPocketBaseRealtimeConnectionState::Stopped)
    {
        OnEvent.Broadcast(Event);
    }
}

void UOpenPocketBaseSubscription::HandleNativeState(
    const EOpenPocketBaseRealtimeConnectionState State)
{
    if (ConnectionState == State)
    {
        return;
    }
    ConnectionState = State;
    OnConnectionStateChanged.Broadcast(State);
    if (State == EOpenPocketBaseRealtimeConnectionState::Stopped)
    {
        NativeHandle.Reset();
        if (UOpenPocketBaseClient* Owner = OwnerClient.Get())
        {
            Owner->ReleaseSubscription(this);
        }
        OwnerClient.Reset();
    }
}

void UOpenPocketBaseSubscription::HandleNativeError(const FOpenPocketBaseError& Error)
{
    if (ConnectionState != EOpenPocketBaseRealtimeConnectionState::Stopped)
    {
        OnError.Broadcast(Error);
    }
}

void UOpenPocketBaseSubscription::HandleNativeResyncRequired()
{
    if (ConnectionState != EOpenPocketBaseRealtimeConnectionState::Stopped)
    {
        OnRealtimeResyncRequired.Broadcast();
    }
}
