#include "SecureStorage/OpenPocketBaseSecureStore.h"

#if PLATFORM_MAC || PLATFORM_IOS
TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe>
CreateOpenPocketBaseAppleKeychainSecureStore();
#endif

namespace
{
class FUnavailableOpenPocketBaseSecureStore final : public IOpenPocketBaseSecureStore
{
public:
    virtual bool IsAvailable(FString& OutReason) const override
    {
        OutReason = TEXT("Secure session storage is unavailable on this platform or build.");
        return false;
    }

    virtual bool Save(
        const FString& Key,
        TConstArrayView<uint8> Value,
        FOpenPocketBaseError& OutError) override
    {
        return Fail(OutError);
    }

    virtual bool Load(
        const FString& Key,
        TArray<uint8>& OutValue,
        bool& bOutFound,
        FOpenPocketBaseError& OutError) override
    {
        OutValue.Reset();
        bOutFound = false;
        return Fail(OutError);
    }

    virtual bool Delete(const FString& Key, FOpenPocketBaseError& OutError) override
    {
        return Fail(OutError);
    }

private:
    static bool Fail(FOpenPocketBaseError& OutError)
    {
        OutError = FOpenPocketBaseError();
        OutError.Kind = EOpenPocketBaseErrorKind::SecureStorage;
        OutError.ServerMessage = TEXT("Secure session storage is unavailable on this platform or build.");
        return false;
    }
};
}

TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe> CreateOpenPocketBaseSecureStore()
{
#if PLATFORM_MAC || PLATFORM_IOS
    return CreateOpenPocketBaseAppleKeychainSecureStore();
#else
    return MakeShared<FUnavailableOpenPocketBaseSecureStore, ESPMode::ThreadSafe>();
#endif
}
