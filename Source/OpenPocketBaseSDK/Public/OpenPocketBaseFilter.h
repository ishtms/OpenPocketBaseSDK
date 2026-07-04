#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseError.h"

#include "OpenPocketBaseFilter.generated.h"

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseFilterParams
{
    GENERATED_BODY()

    bool AddString(const FString& Name, const FString& Value);
    bool AddNumber(const FString& Name, double Value);
    bool AddBoolean(const FString& Name, bool bValue);
    bool AddDate(const FString& Name, const FDateTime& Value);
    bool AddNull(const FString& Name);
    bool AddStringArray(const FString& Name, const TArray<FString>& Value);
    bool AddNumberArray(const FString& Name, const TArray<double>& Value);
    bool AddBooleanArray(const FString& Name, const TArray<bool>& Value);
    int32 Num() const;
    void Reset();

private:
    bool AddEncoded(const FString& Name, FString EncodedValue);

    TMap<FString, FString> EncodedValues;

    friend class FOpenPocketBaseFilter;
};

class OPENPOCKETBASESDK_API FOpenPocketBaseFilter final
{
public:
    static bool TryBind(
        const FString& Expression,
        const FOpenPocketBaseFilterParams& Params,
        FString& OutFilter,
        FOpenPocketBaseError& OutError);
};
