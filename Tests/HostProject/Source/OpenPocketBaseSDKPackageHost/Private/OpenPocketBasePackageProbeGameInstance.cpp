#include "OpenPocketBasePackageProbeGameInstance.h"

#include "HAL/PlatformMisc.h"

void UOpenPocketBasePackageProbeGameInstance::Init()
{
    Super::Init();

    const FString Origin = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENPOCKETBASE_PACKAGE_TLS_ORIGIN"));
    if (Origin.IsEmpty())
    {
        return;
    }

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = Origin;
    Config.ProfileName = TEXT("packaged-tls-probe");
    FOpenPocketBaseError Error;
    Client = FOpenPocketBaseClient::Create(Config, Error);
    if (!Client.IsValid())
    {
        FinishProbe(false, Error);
        return;
    }

    FOpenPocketBaseRequestOptions Options;
    Options.TotalTimeoutSeconds = 20;
    Options.ActivityTimeoutSeconds = 10;
    Options.bRetryEligibleReads = false;
    const TWeakObjectPtr<UOpenPocketBasePackageProbeGameInstance> WeakThis(this);
    Request = Client->Collection(TEXT("tls_probe")).GetOne(
        TEXT("missing"),
        [WeakThis](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            if (Result.IsSuccess())
            {
                WeakThis->FinishProbe(true, FOpenPocketBaseError());
                return;
            }

            const FOpenPocketBaseError& RequestError = Result.GetError();
            const bool bReachedTrustedHttp = RequestError.Kind == EOpenPocketBaseErrorKind::Http ||
                RequestError.Kind == EOpenPocketBaseErrorKind::PocketBase ||
                RequestError.Kind == EOpenPocketBaseErrorKind::Serialization;
            WeakThis->FinishProbe(bReachedTrustedHttp, RequestError);
        },
        Options);
}

void UOpenPocketBasePackageProbeGameInstance::Shutdown()
{
    if (Client.IsValid())
    {
        Client->Shutdown();
        Client.Reset();
    }
    Super::Shutdown();
}

void UOpenPocketBasePackageProbeGameInstance::FinishProbe(
    const bool bReachedTrustedHttp,
    const FOpenPocketBaseError& Error)
{
    if (bReachedTrustedHttp)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("OPENPOCKETBASE_PACKAGED_TLS_SUCCESS kind=%d status=%d request=%s"),
            static_cast<int32>(Error.Kind),
            Error.HttpStatus,
            *Error.RequestId);
    }
    else
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("OPENPOCKETBASE_PACKAGED_TLS_FAILURE kind=%d status=%d request=%s"),
            static_cast<int32>(Error.Kind),
            Error.HttpStatus,
            *Error.RequestId);
    }

    if (Client.IsValid())
    {
        Client->Shutdown();
    }
    FPlatformMisc::RequestExitWithStatus(true, bReachedTrustedHttp ? 0 : 3);
}
