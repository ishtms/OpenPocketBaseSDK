#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "OpenPocketBaseBlueprintClient.h"
#include "OpenPocketBaseSubscription.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include <atomic>

namespace
{
class FBlueprintRealtimeTransport final : public IOpenPocketBaseTransport
{
public:
    virtual bool IsIncrementalResponseStreamingAvailable(FString& OutReason) const override
    {
        OutReason = TEXT("The Blueprint realtime transport supports streaming.");
        return true;
    }

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        ++RequestCount;
        return FOpenPocketBaseTransportHandle([Cancelled = Cancelled]()
        {
            Cancelled->store(true, std::memory_order_release);
        });
    }

    int32 RequestCount = 0;
    TSharedRef<std::atomic<bool>, ESPMode::ThreadSafe> Cancelled =
        MakeShared<std::atomic<bool>, ESPMode::ThreadSafe>(false);
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRealtimeBlueprintTest,
    "OpenPocketBase.Blueprint.Realtime.RetainsDedicatedSubscriptionAndTearsItDown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRealtimeBlueprintTest::RunTest(const FString& Parameters)
{
    const FArrayProperty* RetainedSubscriptions = FindFProperty<FArrayProperty>(
        UOpenPocketBaseClient::StaticClass(),
        TEXT("ActiveSubscriptions"));
    TestNotNull(TEXT("The Blueprint client reflects its subscription retention array"),
        RetainedSubscriptions);
    if (RetainedSubscriptions != nullptr)
    {
        TestTrue(TEXT("The retention array is transient"),
            RetainedSubscriptions->HasAnyPropertyFlags(CPF_Transient));
    }
    TestNotNull(TEXT("Subscribe to Records is a standard Blueprint function"),
        UOpenPocketBaseClient::StaticClass()->FindFunctionByName(TEXT("SubscribeToRecords")));
    TestNotNull(TEXT("A subscription exposes Unsubscribe"),
        UOpenPocketBaseSubscription::StaticClass()->FindFunctionByName(TEXT("Unsubscribe")));

    const TSharedRef<FBlueprintRealtimeTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FBlueprintRealtimeTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    FOpenPocketBaseError Error;
    UOpenPocketBaseClient* Client = UOpenPocketBaseClient::Create(
        GetTransientPackage(), Config, Transport, Error);
    if (!TestNotNull(TEXT("The Blueprint client is created"), Client))
    {
        return false;
    }
    Client->AddToRoot();

    UOpenPocketBaseSubscription* Subscription = Client->SubscribeToRecords(
        TEXT("messages"), {}, Error);
    TestNotNull(TEXT("Subscribe to Records returns a dedicated object"), Subscription);
    TestFalse(TEXT("The subscription call has no local error"), Error.IsSet());
    TestEqual(TEXT("The object shares one native stream"), Transport->RequestCount, 1);
    if (Subscription != nullptr)
    {
        TestTrue(TEXT("The returned subscription is active"), Subscription->IsActive());
    }

    Client->Shutdown();
    if (Subscription != nullptr)
    {
        TestFalse(TEXT("Client teardown stops the retained subscription"), Subscription->IsActive());
        TestEqual(
            TEXT("Client teardown exposes the stopped lifecycle"),
            Subscription->GetConnectionState(),
            EOpenPocketBaseRealtimeConnectionState::Stopped);
    }
    TestTrue(TEXT("Client teardown cancels the shared stream"),
        Transport->Cancelled->load(std::memory_order_acquire));
    Client->RemoveFromRoot();
    return true;
}

#endif
