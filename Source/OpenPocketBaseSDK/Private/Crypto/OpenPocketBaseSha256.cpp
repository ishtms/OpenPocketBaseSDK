// Copyright 2026 Ishtmeet Singh.

#include "Crypto/OpenPocketBaseSha256.h"

#include "HAL/UnrealMemory.h"

namespace
{
constexpr uint32 RoundConstants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

uint32 RotateRight(const uint32 Value, const uint32 Bits)
{
    return (Value >> Bits) | (Value << (32 - Bits));
}

void ProcessBlock(const uint8* Block, uint32 State[8])
{
    uint32 Schedule[64] = {};
    for (int32 Index = 0; Index < 16; ++Index)
    {
        const int32 Offset = Index * 4;
        Schedule[Index] =
            (static_cast<uint32>(Block[Offset]) << 24) |
            (static_cast<uint32>(Block[Offset + 1]) << 16) |
            (static_cast<uint32>(Block[Offset + 2]) << 8) |
            static_cast<uint32>(Block[Offset + 3]);
    }
    for (int32 Index = 16; Index < 64; ++Index)
    {
        const uint32 First = Schedule[Index - 15];
        const uint32 Second = Schedule[Index - 2];
        const uint32 Sigma0 = RotateRight(First, 7) ^ RotateRight(First, 18) ^ (First >> 3);
        const uint32 Sigma1 = RotateRight(Second, 17) ^ RotateRight(Second, 19) ^ (Second >> 10);
        Schedule[Index] = Schedule[Index - 16] + Sigma0 +
            Schedule[Index - 7] + Sigma1;
    }

    uint32 A = State[0];
    uint32 B = State[1];
    uint32 C = State[2];
    uint32 D = State[3];
    uint32 E = State[4];
    uint32 F = State[5];
    uint32 G = State[6];
    uint32 H = State[7];
    for (int32 Index = 0; Index < 64; ++Index)
    {
        const uint32 UpperSigma1 = RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25);
        const uint32 Choice = (E & F) ^ (~E & G);
        const uint32 First = H + UpperSigma1 + Choice + RoundConstants[Index] + Schedule[Index];
        const uint32 UpperSigma0 = RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22);
        const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
        const uint32 Second = UpperSigma0 + Majority;

        H = G;
        G = F;
        F = E;
        E = D + First;
        D = C;
        C = B;
        B = A;
        A = First + Second;
    }

    State[0] += A;
    State[1] += B;
    State[2] += C;
    State[3] += D;
    State[4] += E;
    State[5] += F;
    State[6] += G;
    State[7] += H;
}
}

void OpenPocketBase::Crypto::Sha256(
    const TArrayView<const uint8> Data,
    uint8 OutDigest[32])
{
    uint32 State[8] = {
        0x6a09e667,
        0xbb67ae85,
        0x3c6ef372,
        0xa54ff53a,
        0x510e527f,
        0x9b05688c,
        0x1f83d9ab,
        0x5be0cd19,
    };

    const int32 FullBlockBytes = Data.Num() - (Data.Num() % 64);
    for (int32 Offset = 0; Offset < FullBlockBytes; Offset += 64)
    {
        ProcessBlock(Data.GetData() + Offset, State);
    }

    uint8 FinalBlocks[128] = {};
    const int32 RemainingBytes = Data.Num() - FullBlockBytes;
    if (RemainingBytes > 0)
    {
        FMemory::Memcpy(FinalBlocks, Data.GetData() + FullBlockBytes, RemainingBytes);
    }
    FinalBlocks[RemainingBytes] = 0x80;
    const int32 FinalBytes = RemainingBytes < 56 ? 64 : 128;
    const uint64 BitLength = static_cast<uint64>(Data.Num()) * 8;
    for (int32 Index = 0; Index < 8; ++Index)
    {
        FinalBlocks[FinalBytes - 1 - Index] =
            static_cast<uint8>(BitLength >> (Index * 8));
    }
    ProcessBlock(FinalBlocks, State);
    if (FinalBytes == 128)
    {
        ProcessBlock(FinalBlocks + 64, State);
    }

    for (int32 Index = 0; Index < 8; ++Index)
    {
        OutDigest[Index * 4] = static_cast<uint8>(State[Index] >> 24);
        OutDigest[Index * 4 + 1] = static_cast<uint8>(State[Index] >> 16);
        OutDigest[Index * 4 + 2] = static_cast<uint8>(State[Index] >> 8);
        OutDigest[Index * 4 + 3] = static_cast<uint8>(State[Index]);
    }
}
