#include "OpenPocketBaseBlueprintClient.h"

#include "UObject/Package.h"

UOpenPocketBaseClient* UOpenPocketBaseClient::Create(
    UObject* Outer,
    const FOpenPocketBaseClientConfig& Config,
    FOpenPocketBaseError& OutError)
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        FOpenPocketBaseClient::Create(Config, OutError);
    if (!Client.IsValid())
    {
        return nullptr;
    }

    UOpenPocketBaseClient* Wrapper = NewObject<UOpenPocketBaseClient>(
        Outer != nullptr ? Outer : GetTransientPackage());
    Wrapper->NativeClient = Client;
    return Wrapper;
}

UOpenPocketBaseClient* UOpenPocketBaseClient::Create(
    UObject* Outer,
    const FOpenPocketBaseClientConfig& Config,
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
    FOpenPocketBaseError& OutError)
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        FOpenPocketBaseClient::Create(Config, MoveTemp(Transport), OutError);
    if (!Client.IsValid())
    {
        return nullptr;
    }

    UOpenPocketBaseClient* Wrapper = NewObject<UOpenPocketBaseClient>(
        Outer != nullptr ? Outer : GetTransientPackage());
    Wrapper->NativeClient = Client;
    return Wrapper;
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

bool UOpenPocketBaseClient::IsAuthenticated() const
{
    return NativeClient.IsValid() && NativeClient->IsAuthenticated();
}

bool UOpenPocketBaseClient::GetCurrentAuthRecord(FOpenPocketBaseRecord& OutRecord) const
{
    return NativeClient.IsValid() && NativeClient->GetCurrentAuthRecord(OutRecord);
}

void UOpenPocketBaseClient::Shutdown()
{
    if (NativeClient.IsValid())
    {
        NativeClient->Shutdown();
    }
}

void UOpenPocketBaseClient::BeginDestroy()
{
    Shutdown();
    NativeClient.Reset();
    Super::BeginDestroy();
}
