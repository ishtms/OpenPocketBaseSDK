#pragma once

#include "OpenPocketBaseClient.h"

inline TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> TakeOpenPocketBaseTestClient(
    FOpenPocketBaseClientResult Result,
    FOpenPocketBaseError& OutError)
{
    if (!Result.IsSuccess())
    {
        OutError = Result.GetError();
        return nullptr;
    }
    OutError = {};
    return Result.TakeValue();
}

inline TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> CreateOpenPocketBaseTestClient(
    const FOpenPocketBaseClientConfig& Config,
    FOpenPocketBaseError& OutError)
{
    return TakeOpenPocketBaseTestClient(FOpenPocketBaseClient::Create(Config), OutError);
}

inline TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> CreateOpenPocketBaseTestClient(
    const FOpenPocketBaseClientConfig& Config,
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
    FOpenPocketBaseError& OutError)
{
    FOpenPocketBaseClientDependencies Dependencies;
    Dependencies.Transport = MoveTemp(Transport);
    return TakeOpenPocketBaseTestClient(
        FOpenPocketBaseClient::Create(Config, MoveTemp(Dependencies)),
        OutError);
}

inline TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> CreateOpenPocketBaseTestClient(
    const FOpenPocketBaseClientConfig& Config,
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
    TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore,
    FOpenPocketBaseError& OutError)
{
    FOpenPocketBaseClientDependencies Dependencies;
    Dependencies.Transport = MoveTemp(Transport);
    Dependencies.SecureStore = MoveTemp(SecureStore);
    return TakeOpenPocketBaseTestClient(
        FOpenPocketBaseClient::Create(Config, MoveTemp(Dependencies)),
        OutError);
}

inline TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> CreateOpenPocketBaseTestClient(
    const FOpenPocketBaseClientConfig& Config,
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
    TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore,
    TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock,
    FOpenPocketBaseError& OutError)
{
    FOpenPocketBaseClientDependencies Dependencies;
    Dependencies.Transport = MoveTemp(Transport);
    Dependencies.SecureStore = MoveTemp(SecureStore);
    Dependencies.Clock = MoveTemp(Clock);
    return TakeOpenPocketBaseTestClient(
        FOpenPocketBaseClient::Create(Config, MoveTemp(Dependencies)),
        OutError);
}

inline TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> CreateOpenPocketBaseTestClient(
    const FOpenPocketBaseClientConfig& Config,
    TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> Transport,
    TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> SecureStore,
    TSharedRef<IOpenPocketBaseClock, ESPMode::ThreadSafe> Clock,
    TSharedRef<IOpenPocketBaseOAuthBrowser, ESPMode::ThreadSafe> OAuthBrowser,
    FOpenPocketBaseError& OutError)
{
    FOpenPocketBaseClientDependencies Dependencies;
    Dependencies.Transport = MoveTemp(Transport);
    Dependencies.SecureStore = MoveTemp(SecureStore);
    Dependencies.Clock = MoveTemp(Clock);
    Dependencies.OAuthBrowser = MoveTemp(OAuthBrowser);
    return TakeOpenPocketBaseTestClient(
        FOpenPocketBaseClient::Create(Config, MoveTemp(Dependencies)),
        OutError);
}
