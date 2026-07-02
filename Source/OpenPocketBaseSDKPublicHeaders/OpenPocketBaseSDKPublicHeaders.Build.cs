using UnrealBuildTool;

public class OpenPocketBaseSDKPublicHeaders : ModuleRules
{
    public OpenPocketBaseSDKPublicHeaders(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;
        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "OpenPocketBaseSDK"
            });
    }
}
