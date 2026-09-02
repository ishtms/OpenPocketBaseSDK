#include "HAL/Platform.h"

#if PLATFORM_MAC

#include "OAuth/OpenPocketBaseMacOAuthBrowser.h"

#include "CoreGlobals.h"
#include "Mac/MacSystemIncludes.h"

namespace
{
bool IsBoundedHttpsUrl(const FString& Url)
{
    return !Url.IsEmpty() && Url.Len() <= 8192 &&
        Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase) &&
        !Url.Contains(TEXT("\\"));
}

class FMacOAuthBrowser final : public IOpenPocketBaseOAuthBrowser
{
public:
    virtual bool IsAvailable(FString& OutReason) const override
    {
        OutReason = TEXT("The native macOS external browser bridge is available.");
        return true;
    }

    virtual bool IsPlatformFlowValidated(FString& OutReason) const override
    {
        OutReason = TEXT("The macOS browser bridge exists, but the complete packaged assisted OAuth flow is not validated yet.");
        return false;
    }

    virtual bool OpenExternalAuthorizationUrl(
        const FString& Url,
        FOpenPocketBaseError& OutError) override
    {
        if (!IsInGameThread())
        {
            OutError.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            OutError.Message =
                TEXT("The macOS OAuth browser must be opened from Unreal's game thread. Start the OAuth action from gameplay or Blueprint execution instead of a worker thread.");
            return false;
        }
        if (!IsBoundedHttpsUrl(Url))
        {
            OutError.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            OutError.Message =
                TEXT("The OAuth authorization URL must use HTTPS, contain no backslashes, and be no longer than 8192 characters. Use the URL returned by Start OAuth2.");
            return false;
        }

        NSString* NativeUrl = [NSString stringWithCharacters:
            reinterpret_cast<const unichar*>(*Url) length:Url.Len()];
        NSURL* ParsedUrl = [NSURL URLWithString:NativeUrl];
        if (ParsedUrl == nil || ![[NSWorkspace sharedWorkspace] openURL:ParsedUrl])
        {
            OutError.Kind = EOpenPocketBaseErrorKind::Transport;
            OutError.Message = TEXT("macOS could not open the OAuth authorization URL in the default browser. Check that a default browser is configured, then retry the login.");
            return false;
        }

        OutError = FOpenPocketBaseError();
        return true;
    }
};
}

TSharedRef<IOpenPocketBaseOAuthBrowser, ESPMode::ThreadSafe>
CreateOpenPocketBaseMacOAuthBrowser()
{
    return MakeShared<FMacOAuthBrowser, ESPMode::ThreadSafe>();
}

#endif
