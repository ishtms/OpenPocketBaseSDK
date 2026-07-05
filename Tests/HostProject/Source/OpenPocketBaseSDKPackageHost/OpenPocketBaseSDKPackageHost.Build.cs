using UnrealBuildTool;

public class OpenPocketBaseSDKPackageHost : ModuleRules
{
    public OpenPocketBaseSDKPackageHost(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Json",
                "OpenPocketBaseSDK"
            });
    }
}
