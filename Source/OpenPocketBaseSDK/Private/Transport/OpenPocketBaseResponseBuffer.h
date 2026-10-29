// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"

class FOpenPocketBaseResponseBuffer
{
public:
    explicit FOpenPocketBaseResponseBuffer(const int64 InLimit) : Limit(InLimit) {}

    bool Append(const TArrayView<const uint8> Chunk)
    {
        if (bExceeded)
        {
            return false;
        }
        ReceivedBytes += Chunk.Num();
        if (Limit < 1 || Chunk.Num() > Limit || Bytes.Num() > Limit - Chunk.Num())
        {
            bExceeded = true;
            Bytes.Empty();
            return false;
        }
        Bytes.Append(Chunk.GetData(), Chunk.Num());
        return true;
    }

    bool HasExceededLimit() const { return bExceeded; }
    int64 GetReceivedBytes() const { return ReceivedBytes; }
    TArray<uint8> TakeBytes() { return MoveTemp(Bytes); }

private:
    int64 Limit;
    int64 ReceivedBytes = 0;
    bool bExceeded = false;
    TArray<uint8> Bytes;
};
