// Copyright 2026 Ishtmeet Singh.

#include "Transport/OpenPocketBaseCurlLoop.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>

using namespace OpenPocketBase::Http;
using namespace std::chrono_literals;

namespace
{
void Require(bool Value, const char* Message)
{
    if (!Value) throw std::runtime_error(Message);
}

struct FTransfer
{
    std::promise<FCurlResult> Completion;
    std::future<FCurlResult> Future = Completion.get_future();
    std::promise<void> FirstChunk;
    std::future<void> First = FirstChunk.get_future();
    std::atomic<int> Chunks{0};
    std::atomic<int> Completions{0};
    std::string Body;
};

FCurlRequest Get(const std::string& Url, const std::shared_ptr<FTransfer>& Transfer)
{
    FCurlRequest Request;
    Request.Url = Url;
    Request.Method = "GET";
    Request.TotalTimeoutSeconds = 3;
    Request.ConnectTimeoutSeconds = 1;
    Request.Receive = [Transfer](const std::uint8_t* Bytes, std::size_t Length)
    {
        Transfer->Body.append(reinterpret_cast<const char*>(Bytes), Length);
        if (Transfer->Chunks.fetch_add(1) == 0) Transfer->FirstChunk.set_value();
        return true;
    };
    Request.Complete = [Transfer](FCurlResult&& Result)
    {
        ++Transfer->Completions;
        Transfer->Completion.set_value(std::move(Result));
    };
    return Request;
}

FCurlResult Wait(const std::shared_ptr<FTransfer>& Transfer)
{
    Require(Transfer->Future.wait_for(5s) == std::future_status::ready, "request did not complete");
    FCurlResult Result = Transfer->Future.get();
    Require(Transfer->Completions.load() == 1, "request completed more than once");
    return Result;
}

void SetBody(FCurlRequest& Request, const std::string& Body)
{
    auto Data = std::make_shared<std::pair<std::string, std::size_t>>(Body, 0);
    Request.Method = "POST";
    Request.Headers = {"Content-Type: application/json", "Authorization: dummy-token"};
    Request.BodyLength = Body.size();
    Request.ReadBody = [Data](std::uint8_t* Output, std::size_t Capacity)
    {
        const auto Length = std::min(Capacity, Data->first.size() - Data->second);
        std::copy_n(Data->first.data() + Data->second, Length, Output);
        Data->second += Length;
        return Length;
    };
    Request.SeekBody = [Data](std::uint64_t Position)
    {
        if (Position > Data->first.size()) return false;
        Data->second = static_cast<std::size_t>(Position);
        return true;
    };
}
}

int main(int argc, char** argv)
{
    try
    {
        Require(argc == 2, "pass the fixture URL");
        const std::string Base = argv[1];
        FCurlLoop Loop;

        auto Record = std::make_shared<FTransfer>();
        Loop.Send(Get(Base + "/record", Record));
        Require(Wait(Record).Status == 200 && Record->Body == "record", "GET response changed");

        auto Echo = std::make_shared<FTransfer>();
        auto Post = Get(Base + "/echo", Echo);
        SetBody(Post, "{\"password\":\"fake-password\"}");
        Loop.Send(std::move(Post));
        Require(Wait(Echo).bSucceeded && Echo->Body == "{\"password\":\"fake-password\"}", "POST body changed");

        for (const int Status : {301, 302, 303, 307, 308})
        for (const std::string Method : {"GET", "POST", "PATCH", "DELETE"})
        {
            auto Transfer = std::make_shared<FTransfer>();
            const std::string Url = Base + "/redirect/" + std::to_string(Status);
            auto Request = Get(Url, Transfer);
            Request.Headers = {"Authorization: dummy-token"};
            if (Method != "GET") SetBody(Request, "fake-password");
            Request.Method = Method;
            Loop.Send(std::move(Request));
            const auto Result = Wait(Transfer);
            Require(Result.Status == Status && Result.EffectiveUrl == Url, "redirect was followed");
        }

        auto Large = std::make_shared<FTransfer>();
        auto Bounded = Get(Base + "/large", Large);
        Bounded.Receive = [Large](const std::uint8_t* Bytes, std::size_t Length)
        {
            if (Length > 4096 - Large->Body.size()) return false;
            Large->Body.append(reinterpret_cast<const char*>(Bytes), Length);
            return true;
        };
        Loop.Send(std::move(Bounded));
        Require(!Wait(Large).bSucceeded && Large->Body.size() <= 4096, "receiver rejection did not stop the transfer");

        auto Sse = std::make_shared<FTransfer>();
        auto Stream = Get(Base + "/sse", Sse);
        Stream.TotalTimeoutSeconds = 0;
        Stream.ActivityTimeoutSeconds = 0;
        auto Cancel = Loop.Send(std::move(Stream));
        Require(Sse->First.wait_for(2s) == std::future_status::ready, "SSE did not stream incrementally");
        auto Concurrent = std::make_shared<FTransfer>();
        Loop.Send(Get(Base + "/record", Concurrent));
        Require(Wait(Concurrent).bSucceeded, "SSE blocked another request");
        Require(Sse->Future.wait_for(200ms) == std::future_status::timeout, "idle SSE was closed prematurely");
        Cancel();
        Require(!Wait(Sse).bSucceeded, "cancellation did not stop SSE");

        for (const bool Activity : {true, false})
        {
            auto Stalled = std::make_shared<FTransfer>();
            auto Request = Get(Base + "/stall", Stalled);
            Request.ActivityTimeoutSeconds = Activity ? 0.1 : 0;
            Request.TotalTimeoutSeconds = Activity ? 3 : 0.1;
            Loop.Send(std::move(Request));
            const auto Result = Wait(Stalled);
            Require(Result.bTimedOut && Result.bActivityTimedOut == Activity, "timeout classification changed");
        }

        std::vector<std::shared_ptr<FTransfer>> Parallel;
        for (int Index = 0; Index < 40; ++Index)
        {
            auto Transfer = std::make_shared<FTransfer>();
            Loop.Send(Get(Base + "/record", Transfer));
            Parallel.push_back(Transfer);
        }
        for (const auto& Transfer : Parallel) Require(Wait(Transfer).bSucceeded, "parallel request failed");

        auto Pending = std::make_shared<FTransfer>();
        auto Request = Get(Base + "/sse", Pending);
        Request.TotalTimeoutSeconds = 0;
        Request.ActivityTimeoutSeconds = 0;
        Loop.Send(std::move(Request));
        Require(Pending->First.wait_for(2s) == std::future_status::ready, "shutdown test stream did not start");
        Loop.Shutdown();
        Require(!Wait(Pending).bSucceeded, "shutdown left a stream active");
        auto Rejected = std::make_shared<FTransfer>();
        Loop.Send(Get(Base + "/record", Rejected));
        Require(!Wait(Rejected).bSucceeded, "a stopped transport accepted a request");
        std::cout << "PASS - redirects, uploads, response rejection, SSE, concurrency, timeouts, cancellation and shutdown\n";
        return 0;
    }
    catch (const std::exception& Error)
    {
        std::cerr << Error.what() << '\n';
        return 1;
    }
}
