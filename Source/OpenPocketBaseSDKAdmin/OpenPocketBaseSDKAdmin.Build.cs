using UnrealBuildTool;

public class OpenPocketBaseSDKAdmin : ModuleRules
{
    public OpenPocketBaseSDKAdmin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "OpenPocketBaseSDK" });
    }
}
