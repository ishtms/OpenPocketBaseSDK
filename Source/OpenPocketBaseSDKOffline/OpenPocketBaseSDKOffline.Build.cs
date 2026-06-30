using UnrealBuildTool;

public class OpenPocketBaseSDKOffline : ModuleRules
{
    public OpenPocketBaseSDKOffline(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "OpenPocketBaseSDK" });
    }
}
