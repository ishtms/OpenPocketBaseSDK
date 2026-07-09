#include "OpenPocketBaseProjectSettings.h"

#include "HAL/PlatformMisc.h"

namespace
{
bool FailSettings(FOpenPocketBaseError& OutError, const TCHAR* Message)
{
    OutError = FOpenPocketBaseError();
    OutError.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
    OutError.ServerMessage = Message;
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
        return FailSettings(OutError, TEXT("At least one project profile is required."));
    }

    TSet<FName> ProfileNames;
    const FOpenPocketBaseProjectProfile* ResolvedProfile = nullptr;
    const FName RequestedProfile = Profile.IsNone() ? DefaultProfile : Profile;
    if (RequestedProfile.IsNone())
    {
        return FailSettings(OutError, TEXT("A default project profile is required."));
    }

    for (const FOpenPocketBaseProjectProfile& Candidate : Profiles)
    {
        if (Candidate.Name.IsNone())
        {
            return FailSettings(OutError, TEXT("Every project profile requires a name."));
        }
        if (ProfileNames.Contains(Candidate.Name))
        {
            return FailSettings(OutError, TEXT("Project profile names must be unique."));
        }
        ProfileNames.Add(Candidate.Name);

        FOpenPocketBaseClientConfig CandidateConfig;
        CandidateConfig.BaseUrl = Candidate.BaseUrl;
        CandidateConfig.ProfileName = Candidate.Name.ToString();
        FString NormalizedCandidateUrl;
        if (!CandidateConfig.TryGetNormalizedBaseUrl(NormalizedCandidateUrl, OutError))
        {
            OutError.ServerMessage = TEXT("A project profile contains an invalid base URL or name.");
            return false;
        }
        if (ContainsHeaderControlCharacter(Candidate.AcceptLanguage))
        {
            return FailSettings(OutError, TEXT("A project profile contains an invalid language value."));
        }
        if (!FMath::IsFinite(Candidate.AuthRefreshLeadTimeSeconds) ||
            Candidate.AuthRefreshLeadTimeSeconds < 0.0 ||
            Candidate.AuthRefreshLeadTimeSeconds > 3600.0)
        {
            return FailSettings(OutError, TEXT("A project profile contains an invalid auth refresh lead time."));
        }

        if (Candidate.Name == RequestedProfile)
        {
            ResolvedProfile = &Candidate;
        }
    }

    if (ResolvedProfile == nullptr)
    {
        return FailSettings(OutError, TEXT("The requested project profile does not exist."));
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
        OutError.ServerMessage = TEXT("The project profile origin override is invalid.");
        return false;
    }

    OutConfig.BaseUrl = MoveTemp(NormalizedBaseUrl);
    OutError = FOpenPocketBaseError();
    return true;
}
