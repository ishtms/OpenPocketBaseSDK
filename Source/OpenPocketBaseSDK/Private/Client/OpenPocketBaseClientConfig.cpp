// Copyright 2026 Ishtmeet Singh.

#include "OpenPocketBaseClientConfig.h"

namespace
{
bool ContainsWhitespace(const FString& Value)
{
    for (const TCHAR Character : Value)
    {
        if (FChar::IsWhitespace(Character)) {
            return true;
        }
    }

    return false;
}

bool IsValidPort(const FString& Value)
{
    if (Value.IsEmpty())
    {
        return false;
    }

    for (const TCHAR Character : Value)
    {
        if (!FChar::IsDigit(Character))
        {
            return false;
        }
    }

    const int64 Port = FCString::Atoi64(*Value);
    return Port >= 1 && Port <= 65535;
}

bool IsValidHostName(const FString& Value)
{
    if (Value.IsEmpty() || Value.StartsWith(TEXT(".")) || Value.EndsWith(TEXT(".")) ||
        Value.Contains(TEXT("..")))
    {
        return false;
    }

    TArray<FString> Labels;
    Value.ParseIntoArray(Labels, TEXT("."), false);
    for (const FString& Label : Labels)
    {
        if (Label.IsEmpty() || Label.StartsWith(TEXT("-")) || Label.EndsWith(TEXT("-")))
        {
            return false;
        }
        for (const TCHAR Character : Label)
        {
            if (!FChar::IsAlnum(Character) && Character != TEXT('-'))
            {
                return false;
            }
        }
    }
    return true;
}

bool IsValidAuthority(const FString& Authority)
{
    if (Authority.IsEmpty() || ContainsWhitespace(Authority) || Authority.Contains(TEXT("%")))
    {
        return false;
    }

    if (Authority.StartsWith(TEXT("[")))
    {
        int32 ClosingBracket = INDEX_NONE;
        if (!Authority.FindChar(TEXT(']'), ClosingBracket) || ClosingBracket <= 1)
        {
            return false;
        }

        const FString Address = Authority.Mid(1, ClosingBracket - 1);
        for (const TCHAR Character : Address)
        {
            if (!FChar::IsHexDigit(Character) && Character != TEXT(':') && Character != TEXT('.'))
            {
                return false;
            }
        }

        const FString Suffix = Authority.Mid(ClosingBracket + 1);
        return Suffix.IsEmpty() || (Suffix.StartsWith(TEXT(":")) && IsValidPort(Suffix.Mid(1)));
    }

    FString Host = Authority;
    int32 Colon = INDEX_NONE;
    if (Authority.FindLastChar(TEXT(':'), Colon))
    {
        if (Authority.Left(Colon).Contains(TEXT(":")) || !IsValidPort(Authority.Mid(Colon + 1)))
        {
            return false;
        }
        Host = Authority.Left(Colon);
    }
    return IsValidHostName(Host);
}

bool IsValidProfileName(const FString& Value)
{
    if (Value.Len() > 64)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('-') &&
            Character != TEXT('_') && Character != TEXT('.'))
        {
            return false;
        }
    }
    return true;
}

bool FailNormalization(FOpenPocketBaseError& OutError, const FString& Message)
{
    OutError = FOpenPocketBaseError();
    OutError.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
    OutError.Message = Message;
    return false;
}
}

bool FOpenPocketBaseClientConfig::TryGetNormalizedBaseUrl(
    FString& OutBaseUrl,
    FOpenPocketBaseError& OutError) const
{
    OutBaseUrl.Reset();
    OutError = FOpenPocketBaseError();

    if (ProfileName.Len() > 64)
    {
        return FailNormalization(
            OutError,
            FString::Printf(
                TEXT("Profile Name is %d characters. Use at most 64 characters."),
                ProfileName.Len()));
    }
    if (!IsValidProfileName(ProfileName))
    {
        return FailNormalization(OutError, TEXT("Profile Name can contain only letters, numbers, periods, hyphens, and underscores."));
    }

    FString Candidate = BaseUrl.TrimStartAndEnd();
    while (Candidate.EndsWith(TEXT("/")))
    {
        Candidate.LeftChopInline(1, EAllowShrinking::No);
    }

    const bool bUsesHttps = Candidate.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase);
    const bool bUsesHttp = Candidate.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase);
    if (!bUsesHttps && !bUsesHttp)
    {
        return FailNormalization(OutError, TEXT("Base URL must start with http:// or https://, for example http://127.0.0.1:8090."));
    }

    const int32 SchemeLength = bUsesHttps ? 8 : 7;
    FString Authority = Candidate.Mid(SchemeLength);
    if (Authority.Contains(TEXT("@")))
    {
        return FailNormalization(OutError, TEXT("Base URL must not contain a username or password. Configure authentication through the PocketBase client instead."));
    }

    if (Authority.Contains(TEXT("/")) || Authority.Contains(TEXT("\\")) ||
        Authority.Contains(TEXT("?")) || Authority.Contains(TEXT("#")))
    {
        return FailNormalization(OutError, TEXT("Base URL must contain only the server origin, for example https://pb.example.com. Remove paths such as /api, query parameters, and fragments."));
    }

    if (!IsValidAuthority(Authority))
    {
        return FailNormalization(OutError, TEXT("Base URL must contain a valid host and an optional port from 1 to 65535."));
    }

    Authority.ToLowerInline();
    if (bUsesHttps)
    {
        OutBaseUrl = FString::Printf(TEXT("https://%s"), *Authority);
    }
    else
    {
        OutBaseUrl = FString::Printf(TEXT("http://%s"), *Authority);
    }

    return true;
}
