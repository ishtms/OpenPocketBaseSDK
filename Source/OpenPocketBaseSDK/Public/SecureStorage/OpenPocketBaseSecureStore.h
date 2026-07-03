#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "OpenPocketBaseError.h"
#include "Templates/SharedPointer.h"

class OPENPOCKETBASESDK_API IOpenPocketBaseSecureStore
{
public:
    virtual ~IOpenPocketBaseSecureStore() = default;

    virtual bool IsAvailable(FString& OutReason) const = 0;

    virtual bool Save(
        const FString& Key,
        TConstArrayView<uint8> Value,
        FOpenPocketBaseError& OutError) = 0;

    virtual bool Load(
        const FString& Key,
        TArray<uint8>& OutValue,
        bool& bOutFound,
        FOpenPocketBaseError& OutError) = 0;

    virtual bool Delete(const FString& Key, FOpenPocketBaseError& OutError) = 0;
};

OPENPOCKETBASESDK_API TSharedRef<IOpenPocketBaseSecureStore, ESPMode::ThreadSafe>
CreateOpenPocketBaseSecureStore();
