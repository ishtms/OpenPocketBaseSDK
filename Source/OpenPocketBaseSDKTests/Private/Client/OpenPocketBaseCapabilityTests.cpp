#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "OpenPocketBaseBlueprintClient.h"
#include "OpenPocketBaseCapability.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"
#include "SecureStorage/OpenPocketBaseSecureStore.h"
#include "UObject/Package.h"

namespace
{
class FCapabilityTransport final : public IOpenPocketBaseTransport
{
public:
    explicit FCapabilityTransport(const bool bInSupportsStreaming)
        : bSupportsStreaming(bInSupportsStreaming)
    {
    }

    virtual bool IsIncrementalResponseStreamingAvailable(FString& OutReason) const override
    {
        OutReason = bSupportsStreaming
            ? TEXT("The configured test transport supports incremental response streaming.")
            : TEXT("The configured test transport cannot stream response chunks.");
        return bSupportsStreaming;
    }

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        ++RequestCount;
        return {};
    }

    int32 RequestCount = 0;

private:
    bool bSupportsStreaming = false;
};

class FUnavailableCapabilitySecureStore final : public IOpenPocketBaseSecureStore
{
public:
    virtual bool IsAvailable(FString& OutReason) const override
    {
        OutReason = TEXT("Secure storage is unavailable in the deterministic test store.");
        return false;
    }

    virtual bool Save(
        const FString& Key,
        TConstArrayView<uint8> Value,
        FOpenPocketBaseError& OutError) override
    {
        return false;
    }

    virtual bool Load(
        const FString& Key,
        TArray<uint8>& OutValue,
        bool& bOutFound,
        FOpenPocketBaseError& OutError) override
    {
        OutValue.Reset();
        bOutFound = false;
        return false;
    }

    virtual bool Delete(const FString& Key, FOpenPocketBaseError& OutError) override
    {
        return false;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseCapabilityReportTest,
    "OpenPocketBase.Client.Capabilities.ReportsSmallStableVocabulary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseCapabilityReportTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    const TSharedRef<FCapabilityTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FCapabilityTransport, ESPMode::ThreadSafe>(true);
    FOpenPocketBaseError Error;
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        CreateOpenPocketBaseTestClient(
            Config,
            Transport,
            MakeShared<FUnavailableCapabilitySecureStore, ESPMode::ThreadSafe>(),
            Error);
    if (!TestTrue(TEXT("The native client is created"), Client.IsValid()))
    {
        return false;
    }

    const FOpenPocketBaseCapabilityReport Report = Client->GetCapabilityReport();
    TestEqual(TEXT("The report contains only the six stable capabilities"), Report.Entries.Num(), 6);

    TSet<EOpenPocketBaseCapability> Seen;
    for (const FOpenPocketBaseCapabilityInfo& Entry : Report.Entries)
    {
        Seen.Add(Entry.Capability);
        TestFalse(TEXT("Every capability names its platform"), Entry.Platform.IsEmpty());
        TestFalse(TEXT("Every capability names its build configuration"),
            Entry.BuildConfiguration.IsEmpty());
        TestFalse(TEXT("Every capability has a sanitized reason"), Entry.Reason.IsEmpty());
    }
    TestEqual(TEXT("Every capability appears exactly once"), Seen.Num(), 6);

    FOpenPocketBaseCapabilityInfo Streaming;
    TestTrue(TEXT("A single capability can be queried"),
        Report.TryGet(EOpenPocketBaseCapability::HttpStreaming, Streaming));
    TestEqual(TEXT("The declared streaming transport is supported"),
        Streaming.Status, EOpenPocketBaseCapabilityStatus::Supported);
    TestEqual(TEXT("The direct native query matches the report"),
        Client->GetCapability(EOpenPocketBaseCapability::HttpStreaming).Status,
        Streaming.Status);

    TestNotNull(TEXT("Blueprint exposes the single-capability query"),
        UOpenPocketBaseClient::StaticClass()->FindFunctionByName(TEXT("GetCapability")));
    TestNotNull(TEXT("Blueprint exposes the complete report query"),
        UOpenPocketBaseClient::StaticClass()->FindFunctionByName(TEXT("GetCapabilityReport")));
    Client->Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRealtimeCapabilityGateTest,
    "OpenPocketBase.Client.Capabilities.RejectsUnavailableRealtime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRealtimeCapabilityGateTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    const TSharedRef<FCapabilityTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FCapabilityTransport, ESPMode::ThreadSafe>(false);
    FOpenPocketBaseError Error;
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        CreateOpenPocketBaseTestClient(
            Config,
            Transport,
            MakeShared<FUnavailableCapabilitySecureStore, ESPMode::ThreadSafe>(),
            Error);
    if (!TestTrue(TEXT("The native client is created"), Client.IsValid()))
    {
        return false;
    }

    const FOpenPocketBaseCapabilityInfo Streaming =
        Client->GetCapability(EOpenPocketBaseCapability::HttpStreaming);
    TestEqual(TEXT("The transport reports streaming as unsupported"),
        Streaming.Status, EOpenPocketBaseCapabilityStatus::Unsupported);

    const FOpenPocketBaseSubscriptionResult SubscriptionResult =
        Client->DynamicSubscribe(TEXT("messages/*"));
    TestFalse(TEXT("Realtime is rejected before opening a stream"), SubscriptionResult.IsSuccess());
    TestEqual(TEXT("The rejection uses the stable unsupported error"),
        SubscriptionResult.GetError().Kind, EOpenPocketBaseErrorKind::Unsupported);
    TestEqual(TEXT("The unsupported transport receives no request"), Transport->RequestCount, 0);
    TestTrue(TEXT("The public error carries the sanitized capability reason"),
        SubscriptionResult.GetError().ServerMessage.Contains(TEXT("cannot stream")));

    const FOpenPocketBaseCapabilityInfo SecurePersistence =
        Client->GetCapability(EOpenPocketBaseCapability::SecurePersistence);
    TestEqual(TEXT("The unavailable secure store is reported"),
        SecurePersistence.Status, EOpenPocketBaseCapabilityStatus::Unavailable);
    Client->Shutdown();
    return true;
}

#endif
