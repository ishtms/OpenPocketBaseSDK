#pragma once

#include "Containers/ArrayView.h"
#include "CoreTypes.h"

namespace OpenPocketBase::Crypto
{
void OPENPOCKETBASESDK_API Sha256(TArrayView<const uint8> Data, uint8 OutDigest[32]);
}
