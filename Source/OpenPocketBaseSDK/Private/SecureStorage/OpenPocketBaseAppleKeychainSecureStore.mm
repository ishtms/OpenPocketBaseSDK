#include "SecureStorage/OpenPocketBaseSecureStore.h"

#if PLATFORM_MAC || PLATFORM_IOS

#define FVector AppleFrameworkFVector
#import <Foundation/Foundation.h>
#import <Security/Security.h>
#undef FVector

namespace
{
constexpr int32 MaxSecureValueBytes = 64 * 1024;

FOpenPocketBaseError MakeKeychainError(const TCHAR* Operation, const OSStatus Status)
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::SecureStorage;
    Error.ServerMessage = FString::Printf(
        TEXT("Apple Keychain could not %s the session value (status %d)."),
        Operation,
        static_cast<int32>(Status));
    return Error;
}

NSString* MakeString(const FString& Value)
{
    return [NSString stringWithUTF8String:TCHAR_TO_UTF8(*Value)];
}

NSDictionary* MakeQuery(const FString& Key)
{
    return @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: @"OpenPocketBaseSDK.Session",
        (__bridge id)kSecAttrAccount: MakeString(Key),
    };
}

class FOpenPocketBaseAppleKeychainSecureStore final : public IOpenPocketBaseSecureStore
{
public:
    virtual bool IsAvailable(FString& OutReason) const override
    {
        OutReason.Reset();
        return true;
    }

    virtual bool Save(
        const FString& Key,
        const TConstArrayView<uint8> Value,
        FOpenPocketBaseError& OutError) override
    {
        @autoreleasepool
        {
            if (Key.IsEmpty() || Key.Len() > 256 || Value.IsEmpty() ||
                Value.Num() > MaxSecureValueBytes)
            {
                OutError = MakeKeychainError(TEXT("save"), errSecParam);
                return false;
            }

            NSData* Data = [NSData dataWithBytes:Value.GetData() length:Value.Num()];
            NSDictionary* Query = MakeQuery(Key);
            NSDictionary* Update = @{
                (__bridge id)kSecValueData: Data,
                (__bridge id)kSecAttrAccessible:
                    (__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
            };
            OSStatus Status = SecItemUpdate(
                (__bridge CFDictionaryRef)Query,
                (__bridge CFDictionaryRef)Update);
            if (Status == errSecItemNotFound)
            {
                NSMutableDictionary* AddQuery =
                    [NSMutableDictionary dictionaryWithDictionary:Query];
                [AddQuery addEntriesFromDictionary:Update];
                Status = SecItemAdd((__bridge CFDictionaryRef)AddQuery, nullptr);
            }

            if (Status != errSecSuccess)
            {
                OutError = MakeKeychainError(TEXT("save"), Status);
                return false;
            }
            OutError = FOpenPocketBaseError();
            return true;
        }
    }

    virtual bool Load(
        const FString& Key,
        TArray<uint8>& OutValue,
        bool& bOutFound,
        FOpenPocketBaseError& OutError) override
    {
        @autoreleasepool
        {
            OutValue.Reset();
            bOutFound = false;
            NSMutableDictionary* Query =
                [NSMutableDictionary dictionaryWithDictionary:MakeQuery(Key)];
            Query[(__bridge id)kSecReturnData] = @YES;
            Query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
            CFTypeRef Result = nullptr;
            const OSStatus Status = SecItemCopyMatching(
                (__bridge CFDictionaryRef)Query,
                &Result);
            if (Status == errSecItemNotFound)
            {
                OutError = FOpenPocketBaseError();
                return true;
            }
            if (Status != errSecSuccess || Result == nullptr ||
                CFGetTypeID(Result) != CFDataGetTypeID())
            {
                if (Result != nullptr)
                {
                    CFRelease(Result);
                }
                OutError = MakeKeychainError(TEXT("load"), Status);
                return false;
            }

            const CFDataRef Data = static_cast<CFDataRef>(Result);
            const CFIndex Length = CFDataGetLength(Data);
            if (Length <= 0 || Length > MaxSecureValueBytes)
            {
                CFRelease(Result);
                OutError = MakeKeychainError(TEXT("load"), errSecDecode);
                return false;
            }
            OutValue.Append(CFDataGetBytePtr(Data), static_cast<int32>(Length));
            CFRelease(Result);
            bOutFound = true;
            OutError = FOpenPocketBaseError();
            return true;
        }
    }

    virtual bool Delete(const FString& Key, FOpenPocketBaseError& OutError) override
    {
        @autoreleasepool
        {
            const OSStatus Status = SecItemDelete((__bridge CFDictionaryRef)MakeQuery(Key));
            if (Status != errSecSuccess && Status != errSecItemNotFound)
            {
                OutError = MakeKeychainError(TEXT("delete"), Status);
                return false;
            }
            OutError = FOpenPocketBaseError();
            return true;
        }
    }
};
}

TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe>
CreateOpenPocketBaseAppleKeychainSecureStore()
{
    return MakeShared<FOpenPocketBaseAppleKeychainSecureStore, ESPMode::ThreadSafe>();
}

#endif
