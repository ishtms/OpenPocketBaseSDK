#include "Transport/OpenPocketBaseHttpTransport.h"

#include "Compatibility/OpenPocketBaseUECompat.h"
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
        if (OnChunk && Length > 0)
        {
            OnChunk(MakeArrayView(static_cast<const uint8*>(Data), Length));
        }
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

        const FString RequestId = Request.RequestId;
        const FString RequestUrl = Request.Url;
        HttpRequest->OnProcessRequestComplete().BindLambda(
            [Callbacks, RequestId](
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

                if (Response.IsValid())
                {
                    Result.HttpStatus = Response->GetResponseCode();
                    Result.bTimedOut = Response->GetFailureReason() == EHttpFailureReason::TimedOut;
                    if (!Result.bTransportSucceeded)
                    {
                        Result.ErrorMessage = LexToString(Response->GetFailureReason());
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
                    Result.ErrorMessage = TEXT("Unreal HTTP did not return a response. Check the server URL, confirm PocketBase is running, and verify network access.");
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
