// Copyright 2026 Ishtmeet Singh.

#include "Transport/OpenPocketBaseCurlLoop.h"

#include <curl/curl.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace OpenPocketBase::Http
{
namespace
{
using FTime = std::chrono::steady_clock;

long Milliseconds(const double Seconds)
{
    return static_cast<long>((std::min)(std::ceil(Seconds * 1000.0), static_cast<double>((std::numeric_limits<long>::max)())));
}

struct FOperation
{
    explicit FOperation(FCurlRequest InRequest) : Request(std::move(InRequest)) {}
    ~FOperation()
    {
        if (Easy) curl_easy_cleanup(Easy);
        if (Headers) curl_slist_free_all(Headers);
    }

    template <typename T> void Set(CURLoption Option, T Value)
    {
        if (SetupError == CURLE_OK) SetupError = curl_easy_setopt(Easy, Option, Value);
    }

    static std::size_t Receive(char* Data, std::size_t Size, std::size_t Count, void* Context)
    {
        auto& Self = *static_cast<FOperation*>(Context);
        const std::size_t Length = Size * Count;
        Self.ObserveActivity();
        return !Self.Cancelled->load() && Self.Request.Receive &&
            Self.Request.Receive(reinterpret_cast<const std::uint8_t*>(Data), Length) ? Length : 0;
    }

    static std::size_t Header(char* Data, std::size_t Size, std::size_t Count, void* Context)
    {
        auto& Self = *static_cast<FOperation*>(Context);
        const std::size_t Length = Size * Count;
        Self.ObserveActivity();
        std::string Line(Data, Length);
        if (Line.compare(0, 5, "HTTP/") == 0) Self.ResponseHeaders.clear();
        else if (Line.find(':') != std::string::npos) Self.ResponseHeaders.push_back(std::move(Line));
        return Length;
    }

    static std::size_t Read(char* Data, std::size_t Size, std::size_t Count, void* Context)
    {
        auto& Self = *static_cast<FOperation*>(Context);
        if (Self.Cancelled->load() || !Self.Request.ReadBody) return CURL_READFUNC_ABORT;
        const std::size_t Length = Self.Request.ReadBody(reinterpret_cast<std::uint8_t*>(Data), Size * Count);
        return Length <= Size * Count ? Length : CURL_READFUNC_ABORT;
    }

    static int Seek(void* Context, curl_off_t Offset, int Origin)
    {
        auto& Self = *static_cast<FOperation*>(Context);
        return Origin == SEEK_SET && Offset >= 0 && Self.Request.SeekBody &&
            Self.Request.SeekBody(static_cast<std::uint64_t>(Offset)) ? CURL_SEEKFUNC_OK : CURL_SEEKFUNC_CANTSEEK;
    }

    static int Progress(void* Context, curl_off_t, curl_off_t Received, curl_off_t, curl_off_t Sent)
    {
        auto& Self = *static_cast<FOperation*>(Context);
        if (Received != Self.LastReceived || Sent != Self.LastSent)
        {
            Self.LastReceived = Received;
            Self.LastSent = Sent;
            Self.ObserveActivity();
        }
        return Self.Cancelled->load() ? 1 : 0;
    }

    static CURLcode ConfigureSsl(CURL*, void* SslContext, void* Context)
    {
        auto& Self = *static_cast<FOperation*>(Context);
        return Self.Request.ConfigureSslContext(SslContext) ? CURLE_OK : CURLE_SSL_CERTPROBLEM;
    }

    void ObserveActivity()
    {
        bConnected = true;
        LastActivity = FTime::now();
    }

    bool ActivityTimedOut()
    {
        if (!bConnected)
        {
            curl_off_t ConnectTime = 0;
            curl_easy_getinfo(Easy, CURLINFO_CONNECT_TIME_T, &ConnectTime);
            if (ConnectTime > 0)
            {
                bConnected = true;
                LastActivity = Started + std::chrono::microseconds(ConnectTime);
            }
        }
        return bConnected && Request.ActivityTimeoutSeconds > 0 &&
            std::chrono::duration<double>(FTime::now() - LastActivity).count() >= Request.ActivityTimeoutSeconds;
    }

    bool Setup()
    {
        if (!std::isfinite(Request.TotalTimeoutSeconds) || Request.TotalTimeoutSeconds < 0 ||
            !std::isfinite(Request.ActivityTimeoutSeconds) || Request.ActivityTimeoutSeconds < 0 ||
            !std::isfinite(Request.ConnectTimeoutSeconds) || Request.ConnectTimeoutSeconds < 0 ||
            Request.BodyLength > static_cast<std::uint64_t>((std::numeric_limits<curl_off_t>::max)()) ||
            Request.Method.empty() || Request.Method.size() > 32 ||
            !std::all_of(Request.Method.begin(), Request.Method.end(), [](unsigned char C) { return C >= 'A' && C <= 'Z'; }))
        {
            SetupError = CURLE_BAD_FUNCTION_ARGUMENT;
            return false;
        }
        Easy = curl_easy_init();
        if (!Easy)
        {
            SetupError = CURLE_OUT_OF_MEMORY;
            return false;
        }
        Started = LastActivity = FTime::now();
        Set(CURLOPT_URL, Request.Url.c_str());
        Set(CURLOPT_CUSTOMREQUEST, Request.Method.c_str());
        Set(CURLOPT_FOLLOWLOCATION, 0L);
        Set(CURLOPT_NOSIGNAL, 1L);
        Set(CURLOPT_HTTP_VERSION, static_cast<long>(CURL_HTTP_VERSION_1_1));
        Set(CURLOPT_SSL_VERIFYPEER, 1L);
        Set(CURLOPT_SSL_VERIFYHOST, 2L);
#if LIBCURL_VERSION_NUM >= 0x075500
        Set(CURLOPT_PROTOCOLS_STR, "http,https");
#else
        Set(CURLOPT_PROTOCOLS, static_cast<long>(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
        Set(CURLOPT_TIMEOUT_MS, Milliseconds(Request.TotalTimeoutSeconds));
        Set(CURLOPT_CONNECTTIMEOUT_MS, Milliseconds(Request.ConnectTimeoutSeconds));
        Set(CURLOPT_ACCEPT_ENCODING, "");
        Set(CURLOPT_WRITEFUNCTION, &FOperation::Receive);
        Set(CURLOPT_WRITEDATA, this);
        Set(CURLOPT_HEADERFUNCTION, &FOperation::Header);
        Set(CURLOPT_HEADERDATA, this);
        Set(CURLOPT_XFERINFOFUNCTION, &FOperation::Progress);
        Set(CURLOPT_XFERINFODATA, this);
        Set(CURLOPT_NOPROGRESS, 0L);
        if (!Request.Proxy.empty()) Set(CURLOPT_PROXY, Request.Proxy.c_str());
        if (!Request.NoProxy.empty()) Set(CURLOPT_NOPROXY, Request.NoProxy.c_str());
        if (Request.ConfigureSslContext)
        {
            // The caller supplies the trust store; a build-time CA path may not exist in a packaged game.
            Set(CURLOPT_CAINFO, static_cast<const char*>(nullptr));
            Set(CURLOPT_CAPATH, static_cast<const char*>(nullptr));
            Set(CURLOPT_SSL_CTX_FUNCTION, &FOperation::ConfigureSsl);
            Set(CURLOPT_SSL_CTX_DATA, this);
        }
        if (Request.Method == "HEAD") Set(CURLOPT_NOBODY, 1L);
        else if (Request.BodyLength > 0)
        {
            Set(CURLOPT_UPLOAD, 1L);
            Set(CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(Request.BodyLength));
            Set(CURLOPT_READFUNCTION, &FOperation::Read);
            Set(CURLOPT_READDATA, this);
            Set(CURLOPT_SEEKFUNCTION, &FOperation::Seek);
            Set(CURLOPT_SEEKDATA, this);
        }
        for (const std::string& HeaderLine : Request.Headers)
        {
            curl_slist* Next = curl_slist_append(Headers, HeaderLine.c_str());
            if (!Next)
            {
                SetupError = CURLE_OUT_OF_MEMORY;
                return false;
            }
            Headers = Next;
        }
        curl_slist* Next = curl_slist_append(Headers, "Expect:");
        if (!Next)
        {
            SetupError = CURLE_OUT_OF_MEMORY;
            return false;
        }
        Headers = Next;
        Set(CURLOPT_HTTPHEADER, Headers);
        return SetupError == CURLE_OK;
    }

    FCurlRequest Request;
    std::shared_ptr<std::atomic<bool>> Cancelled = std::make_shared<std::atomic<bool>>(false);
    CURL* Easy = nullptr;
    curl_slist* Headers = nullptr;
    CURLcode SetupError = CURLE_OK;
    std::vector<std::string> ResponseHeaders;
    FTime::time_point Started;
    FTime::time_point LastActivity;
    curl_off_t LastReceived = 0;
    curl_off_t LastSent = 0;
    bool bConnected = false;
};

void Complete(std::unique_ptr<FOperation> Operation, CURLcode Code, bool bActivityTimedOut = false, const char* Error = nullptr)
{
    FCurlResult Result;
    Result.bSucceeded = Code == CURLE_OK;
    Result.bTimedOut = Code == CURLE_OPERATION_TIMEDOUT;
    Result.bActivityTimedOut = bActivityTimedOut;
    Result.EffectiveUrl = Operation->Request.Url;
    Result.Headers = std::move(Operation->ResponseHeaders);
    if (Operation->Easy)
    {
        curl_easy_getinfo(Operation->Easy, CURLINFO_RESPONSE_CODE, &Result.Status);
        char* Url = nullptr;
        if (curl_easy_getinfo(Operation->Easy, CURLINFO_EFFECTIVE_URL, &Url) == CURLE_OK && Url) Result.EffectiveUrl = Url;
    }
    if (!Result.bSucceeded) Result.Error = Error ? Error : curl_easy_strerror(Code);
    auto Callback = std::move(Operation->Request.Complete);
    Operation.reset();
    if (Callback) Callback(std::move(Result));
}
}

struct FCurlLoop::FState
{
    void WakeWorker()
    {
        Wake.notify_one();
        std::lock_guard<std::mutex> Lock(Mutex);
        if (Multi) curl_multi_wakeup(Multi);
    }

    void Run(long MaxConnectionsPerHost)
    {
        const CURLcode InitResult = curl_global_init(CURL_GLOBAL_DEFAULT);
        CURLM* Loop = InitResult == CURLE_OK ? curl_multi_init() : nullptr;
        if (Loop)
        {
            curl_multi_setopt(Loop, CURLMOPT_MAX_TOTAL_CONNECTIONS, 256L);
            curl_multi_setopt(Loop, CURLMOPT_MAX_HOST_CONNECTIONS, (std::max)(0L, MaxConnectionsPerHost));
        }
        {
            std::lock_guard<std::mutex> Lock(Mutex);
            Multi = Loop;
        }
        std::unordered_map<CURL*, std::unique_ptr<FOperation>> Active;
        for (;;)
        {
            std::vector<std::unique_ptr<FOperation>> Incoming;
            bool bStopping;
            {
                std::unique_lock<std::mutex> Lock(Mutex);
                if (Active.empty()) Wake.wait(Lock, [this]() { return bStop || !Pending.empty(); });
                Incoming.swap(Pending);
                bStopping = bStop;
            }
            for (auto& Operation : Incoming)
            {
                if (bStopping || Operation->Cancelled->load()) Complete(std::move(Operation), CURLE_ABORTED_BY_CALLBACK);
                else if (!Loop) Complete(std::move(Operation), CURLE_FAILED_INIT);
                else if (!Operation->Setup())
                {
                    const CURLcode Error = Operation->SetupError;
                    Complete(std::move(Operation), Error);
                }
                else if (curl_multi_add_handle(Loop, Operation->Easy) != CURLM_OK) Complete(std::move(Operation), CURLE_FAILED_INIT);
                else
                {
                    CURL* Easy = Operation->Easy;
                    Active.emplace(Easy, std::move(Operation));
                }
            }
            if (bStopping)
            {
                for (auto& Pair : Active)
                {
                    curl_multi_remove_handle(Loop, Pair.first);
                    Complete(std::move(Pair.second), CURLE_ABORTED_BY_CALLBACK);
                }
                break;
            }
            if (Active.empty()) continue;
            int Running = 0;
            const CURLMcode PerformResult = curl_multi_perform(Loop, &Running);
            int Messages = 0;
            while (CURLMsg* Message = curl_multi_info_read(Loop, &Messages))
            {
                if (Message->msg != CURLMSG_DONE) continue;
                const auto Found = Active.find(Message->easy_handle);
                if (Found == Active.end()) continue;
                const CURLcode Code = Message->data.result;
                curl_multi_remove_handle(Loop, Found->first);
                auto Operation = std::move(Found->second);
                Active.erase(Found);
                Complete(std::move(Operation), Code);
            }
            for (auto It = Active.begin(); It != Active.end();)
            {
                const bool bCancelled = It->second->Cancelled->load();
                const bool bTimedOut = It->second->ActivityTimedOut();
                if (bCancelled || bTimedOut || PerformResult != CURLM_OK)
                {
                    curl_multi_remove_handle(Loop, It->first);
                    auto Operation = std::move(It->second);
                    It = Active.erase(It);
                    Complete(std::move(Operation), bTimedOut ? CURLE_OPERATION_TIMEDOUT : CURLE_ABORTED_BY_CALLBACK, bTimedOut);
                }
                else ++It;
            }
            if (!Active.empty())
            {
                int Descriptors = 0;
                curl_multi_poll(Loop, nullptr, 0, 25, &Descriptors);
            }
        }
        {
            std::lock_guard<std::mutex> Lock(Mutex);
            Multi = nullptr;
        }
        if (Loop) curl_multi_cleanup(Loop);
        if (InitResult == CURLE_OK) curl_global_cleanup();
    }

    std::mutex Mutex;
    std::condition_variable Wake;
    std::vector<std::unique_ptr<FOperation>> Pending;
    CURLM* Multi = nullptr;
    bool bStop = false;
};

FCurlLoop::FCurlLoop(long MaxConnectionsPerHost) : State(std::make_shared<FState>())
{
    Worker = std::thread([Shared = State, MaxConnectionsPerHost]() { Shared->Run(MaxConnectionsPerHost); });
}

FCurlLoop::~FCurlLoop()
{
    Shutdown();
    if (Worker.joinable()) Worker.detach();
}

std::function<void()> FCurlLoop::Send(FCurlRequest Request)
{
    auto Operation = std::make_unique<FOperation>(std::move(Request));
    auto Cancelled = Operation->Cancelled;
    {
        std::lock_guard<std::mutex> Lock(State->Mutex);
        if (!State->bStop) State->Pending.push_back(std::move(Operation));
    }
    if (Operation) Complete(std::move(Operation), CURLE_ABORTED_BY_CALLBACK);
    else State->WakeWorker();
    return [Cancelled, WeakState = std::weak_ptr<FState>(State)]()
    {
        Cancelled->store(true);
        if (auto Shared = WeakState.lock()) Shared->WakeWorker();
    };
}

void FCurlLoop::Shutdown()
{
    {
        std::lock_guard<std::mutex> Lock(State->Mutex);
        State->bStop = true;
    }
    State->WakeWorker();
    if (Worker.joinable() && Worker.get_id() != std::this_thread::get_id()) Worker.join();
}
}
