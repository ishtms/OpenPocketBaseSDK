#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseClientLibrary.h"
#include "OpenPocketBaseSubsystem.h"
#include "Transport/OpenPocketBaseTransport.h"

namespace
{
TArray<uint8> OwnershipToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

class FOwnershipAuthTransport final : public IOpenPocketBaseTransport
{
public:
    explicit FOwnershipAuthTransport(FString InRecordId)
        : RecordId(MoveTemp(InRecordId))
    {
    }

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = 200;
        Response.Body = OwnershipToUtf8(FString::Printf(
            TEXT("{\"token\":\"header.payload.signature\",\"record\":{"
                 "\"id\":\"%s\",\"collectionId\":\"users_id\",\"collectionName\":\"users\","
                 "\"created\":\"2026-08-22 10:00:00.000Z\",\"updated\":\"2026-08-22 10:00:00.000Z\"}}"),
            *RecordId));
        OnComplete(MoveTemp(Response));
        return {};
    }

private:
    FString RecordId;
};

struct FOwnershipState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> FirstClient;
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> SecondClient;
    int32 CompletionCount = 0;
};

class FVerifyOwnershipIsolation final : public IAutomationLatentCommand
{
public:
    FVerifyOwnershipIsolation(
        const TSharedRef<FOwnershipState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (State->CompletionCount != 2)
        {
            return false;
        }

        FOpenPocketBaseRecord FirstRecord;
        FOpenPocketBaseRecord SecondRecord;
        Test->TestTrue(TEXT("The first auth store is populated"), State->FirstClient->GetCurrentAuthRecord(FirstRecord));
        Test->TestTrue(TEXT("The second auth store is populated"), State->SecondClient->GetCurrentAuthRecord(SecondRecord));
        Test->TestEqual(TEXT("The first client retains its own user"), FirstRecord.Id, FString(TEXT("first-user")));
        Test->TestEqual(TEXT("The second client retains its own user"), SecondRecord.Id, FString(TEXT("second-user")));
        Test->TestNotEqual(TEXT("The origins remain isolated"), State->FirstClient->GetBaseUrl(), State->SecondClient->GetBaseUrl());
        State->FirstClient->Shutdown();
        State->SecondClient->Shutdown();
        return true;
    }

private:
    TSharedRef<FOwnershipState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseNativeOwnershipIsolationTest,
    "OpenPocketBase.Client.Ownership.NativeClientsRemainIsolated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseNativeOwnershipIsolationTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FOwnershipState, ESPMode::ThreadSafe> State =
        MakeShared<FOwnershipState, ESPMode::ThreadSafe>();
    const TSharedRef<FOwnershipAuthTransport, ESPMode::ThreadSafe> FirstTransport =
        MakeShared<FOwnershipAuthTransport, ESPMode::ThreadSafe>(TEXT("first-user"));
    const TSharedRef<FOwnershipAuthTransport, ESPMode::ThreadSafe> SecondTransport =
        MakeShared<FOwnershipAuthTransport, ESPMode::ThreadSafe>(TEXT("second-user"));

    FOpenPocketBaseClientConfig FirstConfig;
    FirstConfig.BaseUrl = TEXT("https://first.example.com");
    FOpenPocketBaseClientConfig SecondConfig;
    SecondConfig.BaseUrl = TEXT("https://second.example.com");
    FOpenPocketBaseError Error;
    State->FirstClient = FOpenPocketBaseClient::Create(FirstConfig, FirstTransport, Error);
    State->SecondClient = FOpenPocketBaseClient::Create(SecondConfig, SecondTransport, Error);
    if (!TestNotNull(TEXT("The first client is created"), State->FirstClient.Get()) ||
        !TestNotNull(TEXT("The second client is created"), State->SecondClient.Get()))
    {
        return false;
    }

    State->FirstClient->Collection(TEXT("users")).AuthWithPassword(
        TEXT("first@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result)
        {
            ++State->CompletionCount;
        });
    State->SecondClient->Collection(TEXT("users")).AuthWithPassword(
        TEXT("second@example.com"),
        TEXT("private-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& Result)
        {
            ++State->CompletionCount;
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyOwnershipIsolation(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseGameInstanceOwnershipTest,
    "OpenPocketBase.Blueprint.Client.GameInstanceOwnsNamedClients",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseGameInstanceOwnershipTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone(TEXT("OpenPocketBaseOwnershipTest"));

    FOpenPocketBaseClientConfig DefaultConfig;
    DefaultConfig.BaseUrl = TEXT("https://default.example.com");
    FOpenPocketBaseClientConfig NamedConfig;
    NamedConfig.BaseUrl = TEXT("https://named.example.com");
    FOpenPocketBaseError Error;
    UOpenPocketBaseClient* DefaultClient = nullptr;
    UOpenPocketBaseClient* NamedClient = nullptr;
    TestTrue(
        TEXT("The default client initializes directly from the Game Instance"),
        UOpenPocketBaseClientLibrary::InitializePocketBaseWithConfig(
            GameInstance,
            DefaultConfig,
            DefaultClient,
            Error));
    TestTrue(
        TEXT("A named client initializes through the advanced entry point"),
        UOpenPocketBaseClientLibrary::CreateNamedPocketBaseClient(
            GameInstance,
            TEXT("secondary"),
            NamedConfig,
            NamedClient,
            Error));

    TestNotNull(TEXT("The default client is created"), DefaultClient);
    TestNotNull(TEXT("The named client is created"), NamedClient);
    TestEqual(
        TEXT("The default client is returned without a name"),
        UOpenPocketBaseClientLibrary::GetPocketBaseClient(GameInstance),
        DefaultClient);
    TestEqual(
        TEXT("The named client is returned by identity"),
        UOpenPocketBaseClientLibrary::GetNamedPocketBaseClient(GameInstance, TEXT("secondary")),
        NamedClient);

    const FOpenPocketBaseCollection Collection = DefaultClient->Collection(TEXT(" sdk_tasks "));
    TestTrue(TEXT("A ready client creates a valid collection value"), Collection.IsValid());
    TestEqual(TEXT("Collection names are trimmed"), Collection.Name, FString(TEXT("sdk_tasks")));
    TestEqual(TEXT("Collection values retain their client"), Collection.Client.Get(), DefaultClient);

    UOpenPocketBaseClient* ReusedClient = nullptr;
    TestTrue(
        TEXT("Repeated initialization for the same server is safe"),
        UOpenPocketBaseClientLibrary::InitializePocketBase(
            GameInstance,
            TEXT("https://default.example.com/"),
            ReusedClient,
            Error));
    TestEqual(TEXT("Repeated initialization returns the existing client"), ReusedClient, DefaultClient);

    GameInstance->Shutdown();
    TestFalse(TEXT("Game Instance teardown shuts down the default client"), DefaultClient->IsReady());
    TestFalse(TEXT("Game Instance teardown shuts down the named client"), NamedClient->IsReady());
    GameInstance->RemoveFromRoot();
    return true;
}

#endif
