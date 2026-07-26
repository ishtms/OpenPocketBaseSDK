#include "Transport/OpenPocketBaseHttpTransport.h"

#include "Compatibility/OpenPocketBaseUECompat.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

#include <atomic>

namespace
{
class FHttpCallbackState final
{
public:
    FHttpCallbackState(
        FOpenPocketBaseHttpChunkCallback InOnChunk,
        FOpenPocketBaseHttpCompleteCallback InOnComplete)
        : OnChunk(MoveTemp(InOnChunk))
        , OnComplete(MoveTemp(InOnComplete))
    {
    }

    void Receive(void* Data, int64& Length)
    {
        if (Length > 0)
        {
            ObserveResponseActivity();
        }
        if (OnChunk && Length > 0)
        {
            OnChunk(MakeArrayView(static_cast<const uint8*>(Data), Length));
        }
    }

    void ObserveProgress(const uint64 BytesSent, const uint64 BytesReceived)
    {
        const uint64 PreviousSent = LastBytesSent.exchange(BytesSent, std::memory_order_acq_rel);
        const uint64 PreviousReceived = LastBytesReceived.exchange(BytesReceived, std::memory_order_acq_rel);
        if (BytesSent > PreviousSent || BytesReceived > PreviousReceived)
        {
            if (BytesReceived > PreviousReceived)
            {
                bResponseStarted.store(true, std::memory_order_release);
            }
            LastActivitySeconds.store(FPlatformTime::Seconds(), std::memory_order_release);
        }
    }

    void ObserveResponseActivity(const int32 StatusCode = 0)
    {
        bResponseStarted.store(true, std::memory_order_release);
        if (StatusCode > 0)
        {
            HttpStatus.store(StatusCode, std::memory_order_release);
        }
        LastActivitySeconds.store(FPlatformTime::Seconds(), std::memory_order_release);
    }

    bool DidActivityTimeout(const double TimeoutSeconds) const
    {
        return TimeoutSeconds > 0.0 && bResponseStarted.load(std::memory_order_acquire) &&
            FPlatformTime::Seconds() - LastActivitySeconds.load(std::memory_order_acquire) + 0.05 >= TimeoutSeconds;
    }

    int32 GetHttpStatus() const
    {
        return HttpStatus.load(std::memory_order_acquire);
    }

    void Complete(FOpenPocketBaseHttpResponse&& Response)
    {
        bool bExpected = false;
        if (!bCompleted.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
        {
            return;
        }

        if (OnComplete)
        {
            FOpenPocketBaseHttpCompleteCallback Callback = MoveTemp(OnComplete);
            Callback(MoveTemp(Response));
        }
    }

private:
    std::atomic<bool> bCompleted = false;
    std::atomic<bool> bResponseStarted = false;
    std::atomic<uint64> LastBytesSent = 0;
    std::atomic<uint64> LastBytesReceived = 0;
    std::atomic<double> LastActivitySeconds = FPlatformTime::Seconds();
    std::atomic<int32> HttpStatus = 0;
    FOpenPocketBaseHttpChunkCallback OnChunk;
    FOpenPocketBaseHttpCompleteCallback OnComplete;
};

class FOpenPocketBaseHttpTransport final : public IOpenPocketBaseTransport
{
public:
    virtual bool IsIncrementalResponseStreamingAvailable(FString& OutReason) const override
    {
#if PLATFORM_MAC && PLATFORM_CPU_ARM_FAMILY
        OutReason = TEXT("Incremental Unreal HTTP streaming is packaged-proven on Mac ARM64.");
        return true;
#else
        OutReason = TEXT("Incremental Unreal HTTP streaming has not been packaged-proven on this target.");
        return false;
#endif
    }

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        const TSharedRef<FHttpCallbackState, ESPMode::ThreadSafe> Callbacks =
            MakeShared<FHttpCallbackState, ESPMode::ThreadSafe>(
                MoveTemp(OnChunk),
                MoveTemp(OnComplete));

        const FHttpRequestRef HttpRequest = FHttpModule::Get().CreateRequest();
        HttpRequest->SetURL(Request.Url);
        HttpRequest->SetVerb(Request.Method);
        HttpRequest->SetTimeout(static_cast<float>(Request.TotalTimeoutSeconds));
        HttpRequest->SetActivityTimeout(static_cast<float>(Request.ActivityTimeoutSeconds));
        HttpRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);

        for (const TPair<FString, FString>& Header : Request.Headers)
        {
            HttpRequest->SetHeader(Header.Key, Header.Value);
        }

        if (Request.BodyStream.IsValid())
        {
            const bool bValidStream = Request.Body.IsEmpty() && Request.BodyLength >= 0 &&
                Request.BodyStream->Tell() == 0 && Request.BodyStream->TotalSize() == Request.BodyLength;
            if (!bValidStream || !HttpRequest->SetContentFromStream(Request.BodyStream.ToSharedRef()))
            {
                FOpenPocketBaseHttpResponse Result;
                Result.RequestId = Request.RequestId;
                Result.EffectiveUrl = Request.Url;
                Result.ErrorMessage = TEXT("Unreal HTTP could not attach the request body stream. Confirm the stream is open and readable before sending the request.");
                Callbacks->Complete(MoveTemp(Result));
                return {};
            }
            HttpRequest->SetHeader(TEXT("Content-Length"), LexToString(Request.BodyLength));
        }
        else if (!Request.Body.IsEmpty())
        {
            HttpRequest->SetContent(MoveTemp(Request.Body));
        }

        if (Request.bStreamResponse)
        {
            HttpRequest->SetResponseBodyReceiveStreamDelegateV2(
                FHttpRequestStreamDelegateV2::CreateLambda(
                    [Callbacks](void* Data, int64& Length)
                    {
                        Callbacks->Receive(Data, Length);
                    }));
        }

        HttpRequest->OnStatusCodeReceived().BindLambda(
            [Callbacks](FHttpRequestPtr, const int32 StatusCode)
            {
                Callbacks->ObserveResponseActivity(StatusCode);
            });
        HttpRequest->OnHeaderReceived().BindLambda(
            [Callbacks](FHttpRequestPtr, const FString&, const FString&)
            {
                Callbacks->ObserveResponseActivity();
            });
        HttpRequest->OnRequestProgress64().BindLambda(
            [Callbacks](FHttpRequestPtr, const uint64 BytesSent, const uint64 BytesReceived)
            {
                Callbacks->ObserveProgress(BytesSent, BytesReceived);
            });

        const FString RequestId = Request.RequestId;
        const FString RequestUrl = Request.Url;
        const double TotalTimeoutSeconds = Request.TotalTimeoutSeconds;
        const double ActivityTimeoutSeconds = Request.ActivityTimeoutSeconds;
        HttpRequest->OnProcessRequestComplete().BindLambda(
            [Callbacks, RequestId, TotalTimeoutSeconds, ActivityTimeoutSeconds](
                FHttpRequestPtr CompletedRequest,
                FHttpResponsePtr Response,
                const bool bSucceeded)
            {
                FOpenPocketBaseHttpResponse Result;
                Result.RequestId = RequestId;
                Result.EffectiveUrl = UE::OpenPocketBase::Compatibility::GetEffectiveUrl(
                    CompletedRequest,
                    Response);
                Result.bTransportSucceeded = bSucceeded && Response.IsValid();
                const EHttpFailureReason FailureReason = CompletedRequest.IsValid()
                    ? CompletedRequest->GetFailureReason()
                    : Response.IsValid()
                        ? Response->GetFailureReason()
                        : EHttpFailureReason::Other;
                if (FailureReason == EHttpFailureReason::TimedOut)
                {
                    Result.bTimedOut = true;
                    Result.TimeoutSource = EOpenPocketBaseHttpTimeoutSource::Total;
                    Result.TimeoutSeconds = TotalTimeoutSeconds;
                }
                else if (FailureReason == EHttpFailureReason::ConnectionError &&
                    Callbacks->DidActivityTimeout(ActivityTimeoutSeconds))
                {
                    Result.bTimedOut = true;
                    Result.TimeoutSource = EOpenPocketBaseHttpTimeoutSource::Activity;
                    Result.TimeoutSeconds = ActivityTimeoutSeconds;
                }

                if (Response.IsValid())
                {
                    Result.HttpStatus = Response->GetResponseCode();
                    if (!Result.bTransportSucceeded)
                    {
                        Result.ErrorMessage = LexToString(FailureReason);
                    }

                    for (const FString& HeaderLine : Response->GetAllHeaders())
                    {
                        FString Name;
                        FString Value;
                        if (HeaderLine.Split(TEXT(":"), &Name, &Value))
                        {
                            Result.Headers.Add(Name.TrimStartAndEnd(), Value.TrimStartAndEnd());
                        }
                    }

                    Result.Body = Response->TakeContent();
                }
                else
                {
                    Result.HttpStatus = Callbacks->GetHttpStatus();
                    Result.ErrorMessage = Result.bTimedOut
                        ? LexToString(FailureReason)
                        : TEXT("Unreal HTTP did not return a response. Check the server URL, confirm PocketBase is running, and verify network access.");
                }

                Callbacks->Complete(MoveTemp(Result));
            });

        if (!HttpRequest->ProcessRequest())
        {
            FOpenPocketBaseHttpResponse Result;
            Result.RequestId = RequestId;
            Result.EffectiveUrl = RequestUrl;
            Result.ErrorMessage = TEXT("Unreal HTTP could not start the request. Check the URL, request headers, body stream, and platform HTTP support.");
            Callbacks->Complete(MoveTemp(Result));
        }

        return FOpenPocketBaseTransportHandle(
            [WeakRequest = TWeakPtr<IHttpRequest, ESPMode::ThreadSafe>(HttpRequest)]()
            {
                if (const FHttpRequestPtr RequestToCancel = WeakRequest.Pin())
                {
                    RequestToCancel->CancelRequest();
                }
            });
    }
};
}

TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> CreateOpenPocketBaseHttpTransport()
{
    return MakeShared<FOpenPocketBaseHttpTransport, ESPMode::ThreadSafe>();
}
