#include "OpenPocketBaseAdminBlueprintClient.h"

#include "UObject/Package.h"

namespace
{
UObject* ResolveAdminOuter(const UObject* WorldContextObject)
{
    return WorldContextObject != nullptr
        ? const_cast<UObject*>(WorldContextObject)
        : GetTransientPackage();
}
}

UOpenPocketBaseAdminClient* UOpenPocketBaseAdminClient::Create(
    UObject* Outer,
    const FOpenPocketBaseClientConfig& CoreConfig,
    const FOpenPocketBaseAdminPolicy& Policy,
    FOpenPocketBaseError& OutError)
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native =
        FOpenPocketBaseAdminClient::Create(CoreConfig, Policy, OutError);
    if (!Native.IsValid())
    {
        return nullptr;
    }
    UOpenPocketBaseAdminClient* Wrapper = NewObject<UOpenPocketBaseAdminClient>(
        Outer != nullptr ? Outer : GetTransientPackage());
    Wrapper->NativeClient = MoveTemp(Native);
    return Wrapper;
}

UOpenPocketBaseAdminClient* UOpenPocketBaseAdminClient::Create(
    UObject* Outer,
    const FOpenPocketBaseClientConfig& CoreConfig,
    const FOpenPocketBaseAdminPolicy& Policy,
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
    FOpenPocketBaseError& OutError)
{
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Native =
        FOpenPocketBaseAdminClient::Create(
            CoreConfig,
            Policy,
            MoveTemp(Transport),
            OutError);
    if (!Native.IsValid())
    {
        return nullptr;
    }
    UOpenPocketBaseAdminClient* Wrapper = NewObject<UOpenPocketBaseAdminClient>(
        Outer != nullptr ? Outer : GetTransientPackage());
    Wrapper->NativeClient = MoveTemp(Native);
    return Wrapper;
}

UOpenPocketBaseAdminClient* UOpenPocketBaseAdminClient::CreateAdminClient(
    const UObject* WorldContextObject,
    FOpenPocketBaseClientConfig CoreConfig,
    FOpenPocketBaseAdminPolicy Policy,
    FOpenPocketBaseError& OutError)
{
    return Create(
        ResolveAdminOuter(WorldContextObject),
        CoreConfig,
        Policy,
        OutError);
}

TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe>
UOpenPocketBaseAdminClient::GetNativeClient() const
{
    return NativeClient;
}

bool UOpenPocketBaseAdminClient::IsReady() const
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Core =
        NativeClient.IsValid() ? NativeClient->GetCoreClient() : nullptr;
    return Core.IsValid() && !Core->IsShutdown();
}

bool UOpenPocketBaseAdminClient::IsAuthenticated() const
{
    return NativeClient.IsValid() && NativeClient->IsAuthenticated();
}

void UOpenPocketBaseAdminClient::Logout()
{
    if (NativeClient.IsValid())
    {
        NativeClient->Logout();
    }
}

void UOpenPocketBaseAdminClient::Shutdown()
{
    if (NativeClient.IsValid())
    {
        NativeClient->Shutdown();
    }
}

void UOpenPocketBaseAdminClient::BeginDestroy()
{
    Shutdown();
    NativeClient.Reset();
    Super::BeginDestroy();
}
