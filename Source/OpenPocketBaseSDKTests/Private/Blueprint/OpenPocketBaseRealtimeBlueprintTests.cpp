#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "OpenPocketBaseBlueprintClient.h"
#include "OpenPocketBaseRealtimeLibrary.h"
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
    UClass* RealtimeLibrary = FindObject<UClass>(
        nullptr,
        TEXT("/Script/OpenPocketBaseSDK.OpenPocketBaseRealtimeLibrary"));
    if (!TestNotNull(TEXT("The Blueprint realtime library exists"), RealtimeLibrary))
    {
        return false;
    }

    const UFunction* SubscribeRecords =
        RealtimeLibrary->FindFunctionByName(TEXT("SubscribeToRecords"));
    TestNotNull(TEXT("Subscribe to Records exists"), SubscribeRecords);
    if (SubscribeRecords != nullptr)
    {
        TestEqual(
            TEXT("Subscribe to Records expands into success and failure paths"),
            SubscribeRecords->GetMetaData(TEXT("ExpandBoolAsExecs")),
            FString(TEXT("ReturnValue")));
        const FStructProperty* CollectionProperty =
            FindFProperty<FStructProperty>(SubscribeRecords, TEXT("Collection"));
        TestNotNull(TEXT("Subscribe to Records accepts a collection value"), CollectionProperty);
        if (CollectionProperty != nullptr)
        {
            TestEqual(
                TEXT("The subscription collection pin uses the shared collection type"),
                CollectionProperty->Struct->GetFName(),
                FName(TEXT("OpenPocketBaseCollection")));
        }
    }
    TestNotNull(
        TEXT("Subscribe to Record exists"),
        RealtimeLibrary->FindFunctionByName(TEXT("SubscribeToRecord")));
    TestNotNull(
        TEXT("Advanced topic subscriptions remain available"),
        RealtimeLibrary->FindFunctionByName(TEXT("DynamicSubscribeToTopic")));
    TestNull(
        TEXT("Raw topic subscriptions are explicitly dynamic"),
        RealtimeLibrary->FindFunctionByName(TEXT("SubscribeToTopic")));
    TestNotNull(
        TEXT("Unsubscribe All Realtime exists"),
        RealtimeLibrary->FindFunctionByName(TEXT("UnsubscribeAllRealtime")));
    TestNull(
        TEXT("The Blueprint client no longer exposes collection names for record subscriptions"),
        UOpenPocketBaseClient::StaticClass()->FindFunctionByName(TEXT("SubscribeToRecords")));
    TestNull(
        TEXT("The Blueprint client no longer exposes duplicate client and collection pins"),
        UOpenPocketBaseClient::StaticClass()->FindFunctionByName(TEXT("SubscribeToRecord")));
    TestNull(
        TEXT("Realtime entry nodes live in one Blueprint library"),
        UOpenPocketBaseClient::StaticClass()->FindFunctionByName(TEXT("SubscribeToTopic")));
    TestNull(
        TEXT("Realtime teardown uses the same Blueprint library"),
        UOpenPocketBaseClient::StaticClass()->FindFunctionByName(TEXT("UnsubscribeAllRealtime")));
    TestNotNull(TEXT("A subscription exposes Unsubscribe"),
        UOpenPocketBaseSubscription::StaticClass()->FindFunctionByName(TEXT("Unsubscribe")));

    UOpenPocketBaseSubscription* InvalidSubscription = nullptr;
    FOpenPocketBaseError InvalidError;
    TestFalse(
        TEXT("An invalid collection fails before starting realtime"),
        UOpenPocketBaseRealtimeLibrary::SubscribeToRecords(
            {},
            {},
            InvalidSubscription,
            InvalidError));
    TestNull(
        TEXT("An invalid collection does not create a subscription"),
        InvalidSubscription);
    TestEqual(
        TEXT("An invalid collection returns an actionable error"),
        InvalidError.Kind,
        EOpenPocketBaseErrorKind::InvalidArgument);

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

    FOpenPocketBaseCollectionRef MessagesRef;
    MessagesRef.SchemaId = FGuid(1, 2, 3, 4);
    MessagesRef.CollectionId = TEXT("messages_id");
    MessagesRef.Name = TEXT("messages");
    MessagesRef.Type = EOpenPocketBaseCollectionType::Base;
    const FOpenPocketBaseCollection Collection = Client->Collection(MessagesRef);

    FOpenPocketBaseAnyFieldRef ForeignField;
    ForeignField.SchemaId = MessagesRef.SchemaId;
    ForeignField.CollectionId = TEXT("other_id");
    ForeignField.FieldId = TEXT("title_id");
    ForeignField.Name = TEXT("title");
    ForeignField.Type = EOpenPocketBaseFieldType::Text;
    FOpenPocketBaseRealtimeOptions MismatchedOptions;
    MismatchedOptions.Fields = {OpenPocketBase::Query::Select(ForeignField)};
    UOpenPocketBaseSubscription* MismatchedSubscription = nullptr;
    TestFalse(
        TEXT("Realtime rejects fields from another collection"),
        UOpenPocketBaseRealtimeLibrary::SubscribeToRecords(
            Collection,
            MismatchedOptions,
            MismatchedSubscription,
            Error));
    TestNull(TEXT("A mismatched field does not create a subscription"), MismatchedSubscription);
    TestEqual(TEXT("A mismatched field does not start the transport"), Transport->RequestCount, 0);

    UOpenPocketBaseSubscription* Subscription = nullptr;
    TestTrue(
        TEXT("Subscribe to Records starts from a collection value"),
        UOpenPocketBaseRealtimeLibrary::SubscribeToRecords(
            Collection,
            {},
            Subscription,
            Error));
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
