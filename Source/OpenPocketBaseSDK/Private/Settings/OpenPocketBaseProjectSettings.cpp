#include "OpenPocketBaseProjectSettings.h"

#include "HAL/PlatformMisc.h"

namespace
{
bool FailSettings(FOpenPocketBaseError& OutError, const FString& Message)
{
    OutError = FOpenPocketBaseError();
    OutError.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
    OutError.Message = Message;
    return false;
}

bool ContainsHeaderControlCharacter(const FString& Value)
{
    return Value.Contains(TEXT("\r")) || Value.Contains(TEXT("\n"));
}
}

FName UOpenPocketBaseProjectSettings::GetCategoryName() const
{
    return TEXT("Plugins");
}

const TCHAR* UOpenPocketBaseProjectSettings::GetDevelopmentOverrideEnvironmentVariableName()
{
    return TEXT("OPENPOCKETBASE_DEVELOPMENT_BASE_URL");
}

bool UOpenPocketBaseProjectSettings::TryResolveProfile(
    FName Profile,
    FOpenPocketBaseClientConfig& OutConfig,
    FOpenPocketBaseError& OutError) const
{
    FString DevelopmentOverride;
#if !UE_BUILD_SHIPPING
    if (bAllowDevelopmentEnvironmentOverride)
    {
        DevelopmentOverride = FPlatformMisc::GetEnvironmentVariable(
            GetDevelopmentOverrideEnvironmentVariableName());
    }
#endif

    return TryResolveProfileInternal(
        Profile,
        DevelopmentOverride.IsEmpty() ? nullptr : &DevelopmentOverride,
        OutConfig,
        OutError);
}

bool UOpenPocketBaseProjectSettings::TryResolveProfileWithDevelopmentOverride(
    FName Profile,
    const FString& OverrideBaseUrl,
    FOpenPocketBaseClientConfig& OutConfig,
    FOpenPocketBaseError& OutError) const
{
    return TryResolveProfileInternal(Profile, &OverrideBaseUrl, OutConfig, OutError);
}

bool UOpenPocketBaseProjectSettings::TryResolveProfileInternal(
    FName Profile,
    const FString* OverrideBaseUrl,
    FOpenPocketBaseClientConfig& OutConfig,
    FOpenPocketBaseError& OutError) const
{
    OutConfig = FOpenPocketBaseClientConfig();
    OutError = FOpenPocketBaseError();

    if (Profiles.IsEmpty())
    {
        return FailSettings(OutError, TEXT("No PocketBase project profile exists. Add a profile in Project Settings under Open Pocket Base before creating a client."));
    }

    TSet<FName> ProfileNames;
    const FOpenPocketBaseProjectProfile* ResolvedProfile = nullptr;
    const FName RequestedProfile = Profile.IsNone() ? DefaultProfile : Profile;
    if (RequestedProfile.IsNone())
    {
        return FailSettings(OutError, TEXT("Default Profile is None. Choose one of the configured PocketBase project profiles as the default."));
    }

    for (const FOpenPocketBaseProjectProfile& Candidate : Profiles)
    {
        if (Candidate.Name.IsNone())
        {
            return FailSettings(OutError, TEXT("A project profile has no Name. Give every profile a unique Name before creating a client."));
        }
        if (ProfileNames.Contains(Candidate.Name))
        {
            return FailSettings(
                OutError,
                FString::Printf(
                    TEXT("Project profile '%s' appears more than once. Profile names must be unique."),
                    *Candidate.Name.ToString()));
        }
        ProfileNames.Add(Candidate.Name);

        FOpenPocketBaseClientConfig CandidateConfig;
        CandidateConfig.BaseUrl = Candidate.BaseUrl;
        CandidateConfig.ProfileName = Candidate.Name.ToString();
        FString NormalizedCandidateUrl;
        if (!CandidateConfig.TryGetNormalizedBaseUrl(NormalizedCandidateUrl, OutError))
        {
            OutError.Message = FString::Printf(
                TEXT("Project profile '%s' is invalid. %s"),
                *Candidate.Name.ToString(),
                *OutError.Message);
            return false;
        }
        if (ContainsHeaderControlCharacter(Candidate.AcceptLanguage))
        {
            return FailSettings(
                OutError,
                FString::Printf(
                    TEXT("Project profile '%s' has an invalid Accept Language value. Remove line breaks from the value."),
                    *Candidate.Name.ToString()));
        }
        if (!FMath::IsFinite(Candidate.AuthRefreshLeadTimeSeconds) ||
            Candidate.AuthRefreshLeadTimeSeconds < 0.0 ||
            Candidate.AuthRefreshLeadTimeSeconds > 3600.0)
        {
            return FailSettings(
                OutError,
                FString::Printf(
                    TEXT("Project profile '%s' has Auth Refresh Lead Time Seconds set to %g. Use a finite value from 0 to 3600 seconds."),
                    *Candidate.Name.ToString(),
                    Candidate.AuthRefreshLeadTimeSeconds));
        }

        if (Candidate.Name == RequestedProfile)
        {
            ResolvedProfile = &Candidate;
        }
    }

    if (ResolvedProfile == nullptr)
    {
        return FailSettings(
            OutError,
            FString::Printf(
                TEXT("Project profile '%s' does not exist. Add it in Project Settings or choose an existing profile."),
                *RequestedProfile.ToString()));
    }

    OutConfig.BaseUrl = OverrideBaseUrl == nullptr
        ? ResolvedProfile->BaseUrl
        : *OverrideBaseUrl;
    OutConfig.ProfileName = ResolvedProfile->Name.ToString();
    OutConfig.AcceptLanguage = ResolvedProfile->AcceptLanguage;
    OutConfig.DefaultHeaders.Reset();
    OutConfig.SessionPersistence = ResolvedProfile->SessionPersistence;
    OutConfig.bProactiveAuthRefresh = ResolvedProfile->bProactiveAuthRefresh;
    OutConfig.AuthRefreshLeadTimeSeconds = ResolvedProfile->AuthRefreshLeadTimeSeconds;
    OutConfig.bRetryEligibleReadsAfterAuthRefresh =
        ResolvedProfile->bRetryEligibleReadsAfterAuthRefresh;
    OutConfig.bEnableAssistedOAuth = ResolvedProfile->bEnableAssistedOAuth;

    FString NormalizedBaseUrl;
    if (!OutConfig.TryGetNormalizedBaseUrl(NormalizedBaseUrl, OutError))
    {
        OutConfig = FOpenPocketBaseClientConfig();
        OutError.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
        OutError.Message = FString::Printf(
            TEXT("Base URL override for project profile '%s' is invalid. %s"),
            *ResolvedProfile->Name.ToString(),
            *OutError.Message);
        return false;
    }

    OutConfig.BaseUrl = MoveTemp(NormalizedBaseUrl);
    OutError = FOpenPocketBaseError();
    return true;
}
