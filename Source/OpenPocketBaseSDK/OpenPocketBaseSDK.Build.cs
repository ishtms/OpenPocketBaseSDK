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
