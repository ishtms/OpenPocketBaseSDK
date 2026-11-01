// Copyright 2026 Ishtmeet Singh.

#include "Transport/OpenPocketBaseHttpTransport.h"

#include "Transport/OpenPocketBaseCurlLoop.h"
#include "Transport/OpenPocketBaseResponseBuffer.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "SslModule.h"
#include "Interfaces/ISslCertificateManager.h"
#include "Containers/StringConv.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathUtility.h"

THIRD_PARTY_INCLUDES_START
#include <openssl/ssl.h>
THIRD_PARTY_INCLUDES_END
#include <atomic>
#include <memory>
#include <mutex>

namespace
{
std::string ToUtf8(const FString& Value)
{
    const FTCHARToUTF8 Utf8(*Value, Value.Len());
    return std::string(Utf8.Get(), Utf8.Length());
}

FString FromUtf8(const std::string& Value)
{
    const FUTF8ToTCHAR Converted(Value.data(), static_cast<int32>(Value.size()));
    return FString(Converted.Length(), Converted.Get());
}

struct FTlsPolicy
{
    FString Domain;
    const ISslCertificateManager* Certificates;
};

void FreeTlsPolicy(void*, void* Value, CRYPTO_EX_DATA*, int, long, void*)
{
    delete static_cast<FTlsPolicy*>(Value);
}

int TlsPolicyIndex()
{
    static const int Index = SSL_CTX_get_ex_new_index(0, nullptr, nullptr, nullptr, &FreeTlsPolicy);
    return Index;
}

int VerifyCertificate(int bVerified, X509_STORE_CTX* Store)
{
    if (!bVerified) return 0;
    SSL* Connection = static_cast<SSL*>(X509_STORE_CTX_get_ex_data(Store, SSL_get_ex_data_X509_STORE_CTX_idx()));
    SSL_CTX* Context = Connection ? SSL_get_SSL_CTX(Connection) : nullptr;
    const int Index = TlsPolicyIndex();
    const FTlsPolicy* Policy = Context && Index >= 0
        ? static_cast<FTlsPolicy*>(SSL_CTX_get_ex_data(Context, Index)) : nullptr;
    return Policy && Policy->Certificates->VerifySslCertificates(Store, Policy->Domain) ? 1 : 0;
}

bool ConfigureTls(void* RawContext, const FString& Domain, const ISslCertificateManager& Certificates)
{
    SSL_CTX* Context = static_cast<SSL_CTX*>(RawContext);
    const int Index = TlsPolicyIndex();
    if (Index < 0) return false;
    Certificates.AddCertificatesToSslContext(Context);
    if (!SSL_CTX_get_ex_data(Context, Index))
    {
        FTlsPolicy* Policy = new FTlsPolicy{Domain, &Certificates};
        if (SSL_CTX_set_ex_data(Context, Index, Policy) != 1)
        {
            delete Policy;
            return false;
        }
    }
    SSL_CTX_set_verify(Context, SSL_VERIFY_PEER, &VerifyCertificate);
    return true;
}

std::mutex LoopMutex;
std::shared_ptr<OpenPocketBase::Http::FCurlLoop> SharedLoop;
bool bTransportShutdown = false;

std::shared_ptr<OpenPocketBase::Http::FCurlLoop> GetLoop()
{
    std::lock_guard<std::mutex> Lock(LoopMutex);
    if (bTransportShutdown) return {};
    if (!SharedLoop)
    {
        SharedLoop = std::make_shared<OpenPocketBase::Http::FCurlLoop>(
            FHttpModule::Get().GetHttpMaxConnectionsPerServer());
    }
    return SharedLoop;
}

struct FRequestData
{
    FRequestData(FOpenPocketBaseHttpRequest InRequest, FOpenPocketBaseHttpChunkCallback InChunk,
        FOpenPocketBaseHttpCompleteCallback InComplete)
        : Request(MoveTemp(InRequest)), OnChunk(MoveTemp(InChunk)), OnComplete(MoveTemp(InComplete)),
          Buffer(Request.MaxResponseBytes) {}

    bool Receive(const uint8* Data, const std::size_t Length)
    {
        if (Length > MAX_int32) return false;
        const TArrayView<const uint8> Chunk(Data, static_cast<int32>(Length));
        if (Request.bStreamResponse)
        {
            if (OnChunk) OnChunk(Chunk);
        }
        else if (!Buffer.Append(Chunk)) return false;
        return !Request.ShouldContinueResponse || Request.ShouldContinueResponse();
    }

    void Complete(FOpenPocketBaseHttpResponse&& Response)
    {
        if (bCompleted.exchange(true)) return;
        if (Buffer.HasExceededLimit())
        {
            Response.bTransportSucceeded = false;
            Response.bTimedOut = false;
            Response.bResponseSizeLimitExceeded = true;
            Response.ResponseBytes = Buffer.GetReceivedBytes();
            Response.ResponseLimitBytes = Request.MaxResponseBytes;
        }
        else if (!Request.bStreamResponse) Response.Body = Buffer.TakeBytes();
        auto Callback = MoveTemp(OnComplete);
        if (Callback) Callback(MoveTemp(Response));
    }

    FOpenPocketBaseHttpRequest Request;
    FOpenPocketBaseHttpChunkCallback OnChunk;
    FOpenPocketBaseHttpCompleteCallback OnComplete;
    FOpenPocketBaseResponseBuffer Buffer;
    int64 UploadPosition = 0;
    std::atomic<bool> bCompleted = false;
};

class FOpenPocketBaseHttpTransport final : public IOpenPocketBaseTransport
{
public:
    virtual bool IsIncrementalResponseStreamingAvailable(FString& OutReason) const override
    {
        OutReason = TEXT("Incremental response delivery is available through the SDK's libcurl transport.");
        return true;
    }

    virtual FOpenPocketBaseTransportHandle Send(FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk, FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        auto Data = std::make_shared<FRequestData>(MoveTemp(Request), MoveTemp(OnChunk), MoveTemp(OnComplete));
        const auto Fail = [Data](const TCHAR* Message)
        {
            FOpenPocketBaseHttpResponse Response;
            Response.RequestId = Data->Request.RequestId;
            Response.EffectiveUrl = Data->Request.Url;
            Response.ErrorMessage = Message;
            Data->Complete(MoveTemp(Response));
        };
        FHttpModule& Http = FHttpModule::Get();
        if (!Http.IsHttpEnabled() || !Http.GetHttpManager().IsDomainAllowed(Data->Request.Url))
        {
            Fail(TEXT("The request is disabled by the project's HTTP settings or allowed-domain policy."));
            return {};
        }
        if (!FMath::IsFinite(Data->Request.TotalTimeoutSeconds) || Data->Request.TotalTimeoutSeconds < 0 ||
            !FMath::IsFinite(Data->Request.ActivityTimeoutSeconds) || Data->Request.ActivityTimeoutSeconds < 0 ||
            (!Data->Request.bStreamResponse && (Data->Request.MaxResponseBytes < 1 || Data->Request.MaxResponseBytes > MAX_int32)))
        {
            Fail(TEXT("The HTTP request contains an invalid timeout or response limit."));
            return {};
        }
        if (Data->Request.BodyStream.IsValid() &&
            (!Data->Request.Body.IsEmpty() || Data->Request.BodyLength < 0 ||
             Data->Request.BodyStream->Tell() != 0 || Data->Request.BodyStream->TotalSize() != Data->Request.BodyLength))
        {
            Fail(TEXT("The request body stream must be readable from its start and match the declared body length."));
            return {};
        }

        for (const TPair<FString, FString>& Header : Http.GetDefaultHeaders())
        {
            bool bPresent = false;
            for (const TPair<FString, FString>& Existing : Data->Request.Headers)
            {
                bPresent |= Existing.Key.Equals(Header.Key, ESearchCase::IgnoreCase);
            }
            if (!bPresent) Data->Request.Headers.Add(Header.Key, Header.Value);
        }
        bool bHasUserAgent = false;
        for (const TPair<FString, FString>& Header : Data->Request.Headers)
        {
            bHasUserAgent |= Header.Key.Equals(TEXT("User-Agent"), ESearchCase::IgnoreCase);
        }
        if (!bHasUserAgent) Data->Request.Headers.Add(TEXT("User-Agent"), FPlatformHttp::GetDefaultUserAgent());

        const auto Loop = GetLoop();
        if (!Loop)
        {
            Fail(TEXT("The PocketBase HTTP transport has shut down."));
            return {};
        }
        OpenPocketBase::Http::FCurlRequest Native;
        Native.Url = ToUtf8(Data->Request.Url);
        Native.Method = ToUtf8(Data->Request.Method.ToUpper());
        Native.Proxy = ToUtf8(Http.GetProxyAddress());
        Native.NoProxy = ToUtf8(Http.GetHttpNoProxy());
        Native.TotalTimeoutSeconds = Data->Request.TotalTimeoutSeconds;
        Native.ActivityTimeoutSeconds = Data->Request.ActivityTimeoutSeconds;
        Native.ConnectTimeoutSeconds = Http.GetHttpConnectionTimeout();
        Native.BodyLength = Data->Request.BodyStream.IsValid()
            ? Data->Request.BodyLength : Data->Request.Body.Num();
        for (const TPair<FString, FString>& Header : Data->Request.Headers)
        {
            Native.Headers.push_back(ToUtf8(Header.Key + TEXT(": ") + Header.Value));
        }
        Native.ReadBody = [Data](std::uint8_t* Output, const std::size_t Capacity) -> std::size_t
        {
            const int64 Total = Data->Request.BodyStream.IsValid()
                ? Data->Request.BodyLength : Data->Request.Body.Num();
            const int64 Length = FMath::Min<int64>(Total - Data->UploadPosition, static_cast<int64>(Capacity));
            if (Length <= 0) return 0;
            if (Data->Request.BodyStream.IsValid())
            {
                Data->Request.BodyStream->Serialize(Output, Length);
                if (Data->Request.BodyStream->IsError()) return Capacity + 1;
            }
            else FMemory::Memcpy(Output, Data->Request.Body.GetData() + Data->UploadPosition, Length);
            Data->UploadPosition += Length;
            return static_cast<std::size_t>(Length);
        };
        Native.SeekBody = [Data](const std::uint64_t Position)
        {
            const int64 Total = Data->Request.BodyStream.IsValid()
                ? Data->Request.BodyLength : Data->Request.Body.Num();
            if (Position > static_cast<std::uint64_t>(Total)) return false;
            if (Data->Request.BodyStream.IsValid())
            {
                Data->Request.BodyStream->Seek(static_cast<int64>(Position));
                if (Data->Request.BodyStream->IsError()) return false;
            }
            Data->UploadPosition = static_cast<int64>(Position);
            return true;
        };
        Native.Receive = [Data](const std::uint8_t* Bytes, std::size_t Length) { return Data->Receive(Bytes, Length); };
        const FString Domain = FPlatformHttp::GetUrlDomain(Data->Request.Url);
        const ISslCertificateManager* Certificates = &FSslModule::Get().GetCertificateManager();
        Native.ConfigureSslContext = [Domain, Certificates](void* Context) { return ConfigureTls(Context, Domain, *Certificates); };
        Native.Complete = [Data](OpenPocketBase::Http::FCurlResult&& Result)
        {
            FOpenPocketBaseHttpResponse Response;
            Response.RequestId = Data->Request.RequestId;
            Response.EffectiveUrl = FromUtf8(Result.EffectiveUrl);
            Response.bTransportSucceeded = Result.bSucceeded;
            Response.bTimedOut = Result.bTimedOut;
            Response.HttpStatus = static_cast<int32>(Result.Status);
            Response.ErrorMessage = FromUtf8(Result.Error);
            if (Result.bTimedOut)
            {
                Response.TimeoutSource = Result.bActivityTimedOut
                    ? EOpenPocketBaseHttpTimeoutSource::Activity : EOpenPocketBaseHttpTimeoutSource::Total;
                Response.TimeoutSeconds = Result.bActivityTimedOut
                    ? Data->Request.ActivityTimeoutSeconds : Data->Request.TotalTimeoutSeconds;
            }
            for (const std::string& Header : Result.Headers)
            {
                FString Name;
                FString Value;
                if (FromUtf8(Header).Split(TEXT(":"), &Name, &Value))
                {
                    Response.Headers.Add(Name.TrimStartAndEnd(), Value.TrimStartAndEnd());
                }
            }
            Data->Complete(MoveTemp(Response));
        };
        auto Cancel = Loop->Send(std::move(Native));
        return FOpenPocketBaseTransportHandle([Cancel = std::move(Cancel)]() { Cancel(); });
    }
};
}

TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> CreateOpenPocketBaseHttpTransport()
{
    return MakeShared<FOpenPocketBaseHttpTransport, ESPMode::ThreadSafe>();
}

void ShutdownOpenPocketBaseHttpTransports()
{
    std::shared_ptr<OpenPocketBase::Http::FCurlLoop> Loop;
    {
        std::lock_guard<std::mutex> Lock(LoopMutex);
        bTransportShutdown = true;
        Loop = std::move(SharedLoop);
    }
    if (Loop)
    {
        Loop->Shutdown();
        // OpenSSL keeps ex-data callbacks globally, even for contexts with no SDK data.
        const int Index = TlsPolicyIndex();
        if (Index >= 0) CRYPTO_free_ex_index(CRYPTO_EX_INDEX_SSL_CTX, Index);
    }
}
