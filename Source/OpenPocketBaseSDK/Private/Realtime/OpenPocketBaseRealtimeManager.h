#pragma once

#include "Clock/OpenPocketBaseClock.h"
#include "OpenPocketBaseRealtime.h"
#include "Realtime/OpenPocketBaseSseParser.h"
#include "Transport/OpenPocketBaseTransport.h"

namespace OpenPocketBase::Realtime
{
class FConnectionManager final
    : public TSharedFromThis<FConnectionManager, ESPMode::ThreadSafe>
{
public:
    FConnectionManager(
        FString InBaseUrl,
        TMap<FString, FString> InDefaultHeaders,
        FString InAcceptLanguage,
        TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> InTransport,
        TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> InClock,
        TFunction<FString()> InGetAuthToken,
        TFunction<bool()> InCanReconnect);

    FOpenPocketBaseSubscriptionHandle Subscribe(
        FString Topic,
        FOpenPocketBaseRealtimeCallbacks Callbacks,
        FOpenPocketBaseRealtimeOptions Options,
        FOpenPocketBaseError& OutError);
    void UnsubscribeTopic(const FString& Topic);
    void UnsubscribeAll();
    void NotifyAuthChanged();
    void Shutdown();

private:
    struct FListener
    {
        uint64 Id = 0;
        FString Topic;
        FString WireTopic;
        EOpenPocketBaseRealtimeConnectionState State =
            EOpenPocketBaseRealtimeConnectionState::Created;
        FOpenPocketBaseRealtimeCallbacks Callbacks;
    };

    struct FQueuedCallback
    {
        TFunction<void()> Invoke;
    };

    bool IsListenerActive(uint64 ListenerId) const;
    void UnsubscribeListener(uint64 ListenerId);
    bool BuildWireTopic(
        const FString& Topic,
        const FOpenPocketBaseRealtimeOptions& Options,
        FString& OutWireTopic,
        FOpenPocketBaseError& OutError) const;
    void OpenConnection();
    void HandleChunk(uint64 Generation, TArrayView<const uint8> Chunk);
    void HandleStreamComplete(uint64 Generation, FOpenPocketBaseHttpResponse&& Response);
    void HandleSseEvent(uint64 Generation, const FSseEvent& Event);
    void PostSubscriptions(uint64 Generation);
    void HandlePostComplete(
        uint64 Generation,
        FString ClientId,
        TSet<FString> PostedSubscriptions,
        FOpenPocketBaseHttpResponse&& Response);
    void BeginReconnect(FOpenPocketBaseError Error, bool bReportGap);
    void ScheduleReconnect();
    void HandleReconnectTimer(uint64 TimerGeneration);
    void HandleStableConnection(uint64 StableGeneration);
    void HandleConnectTimeout(uint64 TimedGeneration);
    void HandleApplicationBackgrounded();
    void HandleApplicationForegrounded();
    void HandleNetworkStatusChanged(
        ENetworkConnectionStatus PreviousStatus,
        ENetworkConnectionStatus NewStatus);
    void PauseForLifecycleOrNetwork();
    void ResumeAfterLifecycleOrNetwork();
    void StopConnection(bool bNotifyStopped);
    void SetAllListenerStates(EOpenPocketBaseRealtimeConnectionState NewState);
    void QueueState(
        const FOpenPocketBaseRealtimeCallbacks& Callbacks,
        EOpenPocketBaseRealtimeConnectionState State);
    void QueueError(const FOpenPocketBaseError& Error);
    void QueueResyncRequired();
    void QueueEvent(const FString& WireTopic, FOpenPocketBaseRealtimeEvent Event);
    void EnqueueCallback(TFunction<void()> Callback, bool bRealtimeEvent = false);
    void ScheduleDrain();
    void DrainCallbacks();
    TSet<FString> GetDesiredSubscriptionsLocked() const;
    FOpenPocketBaseHttpRequest MakeRequest(const TCHAR* Method) const;

    FString BaseUrl;
    TMap<FString, FString> DefaultHeaders;
    FString AcceptLanguage;
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport;
    TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock;
    TFunction<FString()> GetAuthToken;
    TFunction<bool()> CanReconnect;

    mutable FCriticalSection Mutex;
    TMap<uint64, FListener> Listeners;
    uint64 NextListenerId = 1;
    TSet<FString> AcknowledgedSubscriptions;
    FString ClientId;
    uint64 ConnectionGeneration = 0;
    int32 ReconnectAttempt = 0;
    TOptional<double> ServerRetrySeconds;
    EOpenPocketBaseRealtimeConnectionState ConnectionState =
        EOpenPocketBaseRealtimeConnectionState::Stopped;
    TUniquePtr<FSseParser> Parser;
    FOpenPocketBaseTransportHandle StreamHandle;
    FOpenPocketBaseTransportHandle PostHandle;
    FOpenPocketBaseClockHandle ReconnectHandle;
    FOpenPocketBaseClockHandle ConnectTimeoutHandle;
    FOpenPocketBaseClockHandle StableConnectionHandle;
    bool bPostInFlight = false;
    bool bStopped = false;
    bool bShutdown = false;
    bool bBackgrounded = false;
    bool bNetworkUnavailable = false;
    FDelegateHandle BackgroundHandle;
    FDelegateHandle ForegroundHandle;
    FDelegateHandle NetworkStatusHandle;

    FCriticalSection QueueMutex;
    TArray<FQueuedCallback> QueuedCallbacks;
    bool bDrainScheduled = false;
    bool bOverflowResyncPending = false;

    static constexpr int32 MaxQueuedCallbacks = 1024;
    static constexpr int32 MaxCallbacksPerDrain = 64;
};
}
