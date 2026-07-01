#include "OpenPocketBaseDate.h"

namespace OpenPocketBase::Date
{
bool TryParse(const FString& Value, FDateTime& OutDateTime)
{
    if (!Value.EndsWith(TEXT("Z"), ESearchCase::CaseSensitive))
    {
        return false;
    }

    FString IsoValue = Value;
    if (IsoValue.Len() > 10 && IsoValue[10] == TEXT(' '))
    {
        IsoValue[10] = TEXT('T');
    }
    return FDateTime::ParseIso8601(*IsoValue, OutDateTime);
}

FString Format(const FDateTime& Value)
{
    FString Result = Value.ToIso8601();
    if (Result.Len() > 10 && Result[10] == TEXT('T'))
    {
        Result[10] = TEXT(' ');
    }
    return Result;
}
}
