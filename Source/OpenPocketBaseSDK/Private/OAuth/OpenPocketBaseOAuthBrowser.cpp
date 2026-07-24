#include "OAuth/OpenPocketBaseOAuthBrowser.h"

#if PLATFORM_MAC
#include "OAuth/OpenPocketBaseMacOAuthBrowser.h"
#endif

namespace
{
class FUnavailableOAuthBrowser final : public IOpenPocketBaseOAuthBrowser
{
public:
    virtual bool IsAvailable(FString& OutReason) const override
    {
        OutReason = TEXT("Assisted OAuth cannot open an external browser on this platform. Use the manual OAuth flow or provide a platform browser bridge.");
        return false;
    }

    virtual bool IsPlatformFlowValidated(FString& OutReason) const override
    {
        OutReason = TEXT("Assisted OAuth has not been validated on this target platform. Use the manual OAuth flow on this target.");
        return false;
    }

    virtual bool OpenExternalAuthorizationUrl(
        const FString& Url,
        FOpenPocketBaseError& OutError) override
    {
        OutError.Kind = EOpenPocketBaseErrorKind::Unsupported;
        OutError.Message =
            TEXT("Assisted OAuth cannot open the authorization page because this platform has no browser bridge. Use the manual OAuth flow or provide a platform browser bridge.");
        return false;
    }
};
}

TSharedRef<IOpenPocketBaseOAuthBrowser, ESPMode::ThreadSafe>
CreateOpenPocketBaseOAuthBrowser()
{
#if PLATFORM_MAC
    return CreateOpenPocketBaseMacOAuthBrowser();
#else
    return MakeShared<FUnavailableOAuthBrowser, ESPMode::ThreadSafe>();
#endif
}
