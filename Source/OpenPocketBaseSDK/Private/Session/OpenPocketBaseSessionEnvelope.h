#pragma once

#include "Containers/Array.h"
#include "OpenPocketBaseError.h"
#include "OpenPocketBaseRecord.h"

namespace OpenPocketBase::SessionEnvelope
{
constexpr int32 SchemaVersion = 1;
constexpr int32 MaxEnvelopeBytes = 64 * 1024;

enum class EReadResult : uint8
{
    Valid,
    Corrupt,
    PolicyRejected
};

bool Serialize(
    const FString& Origin,
    const FString& Profile,
    const FString& AuthCollection,
    const FString& Token,
    const FOpenPocketBaseRecord& Record,
    TArray<uint8>& OutBytes,
    FOpenPocketBaseError& OutError);

EReadResult Deserialize(
    TConstArrayView<uint8> Bytes,
    const FString& ExpectedOrigin,
    const FString& ExpectedProfile,
    FString& OutAuthCollection,
    FString& OutToken,
    FOpenPocketBaseRecord& OutRecord);
}
