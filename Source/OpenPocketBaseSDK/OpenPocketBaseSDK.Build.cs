// Copyright 2026 Ishtmeet Singh.

using UnrealBuildTool;

public class OpenPocketBaseSDK : ModuleRules
{
    public OpenPocketBaseSDK(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "DeveloperSettings",
                "Engine",
                "HTTP",
                "Json",
                "JsonUtilities"
            });

        PrivateDependencyModuleNames.Add("Projects");
        if (Target.Platform == UnrealTargetPlatform.Win64 && Target.WindowsPlatform.bUseXCurl)
        {
            throw new BuildException("OpenPocketBaseSDK requires libcurl. Disable bUseXCurl for this target.");
        }
        PrivateDependencyModuleNames.Add("SSL");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl", "OpenSSL");

        if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicFrameworks.Add("Security");
        }

        if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicFrameworks.Add("AppKit");
        }
    }
}
