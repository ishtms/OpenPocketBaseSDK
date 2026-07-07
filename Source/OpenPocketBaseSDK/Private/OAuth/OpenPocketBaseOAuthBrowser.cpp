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
        OutReason = TEXT("A redaction-safe external OAuth browser bridge is unavailable on this platform.");
        return false;
    }

    virtual bool IsPlatformFlowValidated(FString& OutReason) const override
    {
        OutReason = TEXT("Assisted OAuth has no validated platform flow on this target.");
        return false;
    }

    virtual bool OpenExternalAuthorizationUrl(
        const FString& Url,
        FOpenPocketBaseError& OutError) override
    {
        OutError.Kind = EOpenPocketBaseErrorKind::Unsupported;
        OutError.ServerMessage =
            TEXT("A redaction-safe external OAuth browser bridge is unavailable.");
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
