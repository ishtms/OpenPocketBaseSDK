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
        if (!IsInGameThread() || !IsBoundedHttpsUrl(Url))
        {
            OutError.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
            OutError.ServerMessage =
                TEXT("A bounded HTTPS OAuth URL must be opened from the game thread.");
            return false;
        }

        NSString* NativeUrl = [NSString stringWithCharacters:
            reinterpret_cast<const unichar*>(*Url) length:Url.Len()];
        NSURL* ParsedUrl = [NSURL URLWithString:NativeUrl];
        if (ParsedUrl == nil || ![[NSWorkspace sharedWorkspace] openURL:ParsedUrl])
        {
            OutError.Kind = EOpenPocketBaseErrorKind::Transport;
            OutError.ServerMessage = TEXT("The external OAuth browser could not be opened.");
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
