#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseError.h"
#include "Templates/SharedPointer.h"

class OPENPOCKETBASESDK_API IOpenPocketBaseOAuthBrowser
{
public:
    virtual ~IOpenPocketBaseOAuthBrowser() = default;

    virtual bool IsAvailable(FString& OutReason) const = 0;
    virtual bool IsPlatformFlowValidated(FString& OutReason) const = 0;
    virtual bool OpenExternalAuthorizationUrl(
        const FString& Url,
        FOpenPocketBaseError& OutError) = 0;
};

OPENPOCKETBASESDK_API TSharedRef<IOpenPocketBaseOAuthBrowser, ESPMode::ThreadSafe>
CreateOpenPocketBaseOAuthBrowser();
