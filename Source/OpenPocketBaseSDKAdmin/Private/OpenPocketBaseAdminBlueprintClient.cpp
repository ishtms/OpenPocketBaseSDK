// Copyright 2026 Ishtmeet Singh.

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
    FOpenPocketBaseAdminClientResult Result =
        FOpenPocketBaseAdminClient::Create(CoreConfig, Policy);
    if (!Result.IsSuccess())
    {
        OutError = Result.GetError();
        return nullptr;
    }
    UOpenPocketBaseAdminClient* Wrapper = NewObject<UOpenPocketBaseAdminClient>(
        Outer != nullptr ? Outer : GetTransientPackage());
    Wrapper->NativeClient = Result.TakeValue();
    OutError = {};
    return Wrapper;
}

UOpenPocketBaseAdminClient* UOpenPocketBaseAdminClient::Create(
    UObject* Outer,
    const FOpenPocketBaseClientConfig& CoreConfig,
    const FOpenPocketBaseAdminPolicy& Policy,
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
    FOpenPocketBaseError& OutError)
{
    FOpenPocketBaseClientDependencies Dependencies;
    Dependencies.Transport = MoveTemp(Transport);
    FOpenPocketBaseAdminClientResult Result =
        FOpenPocketBaseAdminClient::Create(CoreConfig, Policy, MoveTemp(Dependencies));
    if (!Result.IsSuccess())
    {
        OutError = Result.GetError();
        return nullptr;
    }
    UOpenPocketBaseAdminClient* Wrapper = NewObject<UOpenPocketBaseAdminClient>(
        Outer != nullptr ? Outer : GetTransientPackage());
    Wrapper->NativeClient = Result.TakeValue();
    OutError = {};
    return Wrapper;
}

bool UOpenPocketBaseAdminClient::InitializeAdminClient(
    const UObject* WorldContextObject,
    FOpenPocketBaseClientConfig CoreConfig,
    FOpenPocketBaseAdminPolicy Policy,
    UOpenPocketBaseAdminClient*& Client,
    FOpenPocketBaseError& Error)
{
    Client = Create(
        ResolveAdminOuter(WorldContextObject),
        CoreConfig,
        Policy,
        Error);
    return Client != nullptr;
}

UWorld* UOpenPocketBaseAdminClient::GetWorld() const
{
    return GetOuter() != nullptr ? GetOuter()->GetWorld() : nullptr;
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
