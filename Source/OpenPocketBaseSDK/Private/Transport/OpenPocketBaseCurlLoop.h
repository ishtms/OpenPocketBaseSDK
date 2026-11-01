// Copyright 2026 Ishtmeet Singh.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace OpenPocketBase::Http
{
struct FCurlResult
{
    bool bSucceeded = false;
    bool bTimedOut = false;
    bool bActivityTimedOut = false;
    long Status = 0;
    std::string EffectiveUrl;
    std::string Error;
    std::vector<std::string> Headers;
};

struct FCurlRequest
{
    std::string Url;
    std::string Method;
    std::string Proxy;
    std::string NoProxy;
    std::vector<std::string> Headers;
    std::uint64_t BodyLength = 0;
    double TotalTimeoutSeconds = 30;
    double ActivityTimeoutSeconds = 15;
    double ConnectTimeoutSeconds = 30;
    std::function<std::size_t(std::uint8_t*, std::size_t)> ReadBody;
    std::function<bool(std::uint64_t)> SeekBody;
    std::function<bool(const std::uint8_t*, std::size_t)> Receive;
    std::function<bool(void*)> ConfigureSslContext;
    std::function<void(FCurlResult&&)> Complete;
};

class FCurlLoop final
{
public:
    explicit FCurlLoop(long MaxConnectionsPerHost = 16);
    ~FCurlLoop();
    FCurlLoop(const FCurlLoop&) = delete;
    FCurlLoop& operator=(const FCurlLoop&) = delete;

    std::function<void()> Send(FCurlRequest Request);
    void Shutdown();

private:
    struct FState;
    std::shared_ptr<FState> State;
    std::thread Worker;
};
}
