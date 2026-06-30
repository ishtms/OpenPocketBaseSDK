#include "OpenPocketBaseClientConfig.h"

namespace
{
bool ContainsWhitespace(const FString& Value)
{
    for (const TCHAR Character : Value)
    {
        if (FChar::IsWhitespace(Character))
        {
            return true;
        }
    }

    return false;
}

bool FailNormalization(FOpenPocketBaseError& OutError, const TCHAR* Message)
{
    OutError = FOpenPocketBaseError();
    OutError.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
    OutError.ServerMessage = Message;
    return false;
}
}

bool FOpenPocketBaseClientConfig::TryGetNormalizedBaseUrl(
    FString& OutBaseUrl,
    FOpenPocketBaseError& OutError) const
{
    OutBaseUrl.Reset();
    OutError = FOpenPocketBaseError();

    FString Candidate = BaseUrl.TrimStartAndEnd();
    while (Candidate.EndsWith(TEXT("/")))
    {
        Candidate.LeftChopInline(1, EAllowShrinking::No);
    }

    const bool bUsesHttps = Candidate.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase);
    const bool bUsesHttp = Candidate.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase);
    if (!bUsesHttps && !bUsesHttp)
    {
        return FailNormalization(OutError, TEXT("Base URL must use HTTP or HTTPS."));
    }

    const int32 SchemeLength = bUsesHttps ? 8 : 7;
    FString Authority = Candidate.Mid(SchemeLength);
    if (Authority.IsEmpty() || ContainsWhitespace(Authority))
    {
        return FailNormalization(OutError, TEXT("Base URL must contain a valid host."));
    }

    if (Authority.Contains(TEXT("@")))
    {
        return FailNormalization(OutError, TEXT("Base URL must not contain credentials."));
    }

    if (Authority.Contains(TEXT("/")) || Authority.Contains(TEXT("\\")) ||
        Authority.Contains(TEXT("?")) || Authority.Contains(TEXT("#")))
    {
        return FailNormalization(OutError, TEXT("Base URL must be an origin without a path, query, or fragment."));
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
