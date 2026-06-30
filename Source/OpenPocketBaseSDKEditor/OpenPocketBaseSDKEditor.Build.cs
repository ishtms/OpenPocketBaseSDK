using UnrealBuildTool;

public class OpenPocketBaseSDKEditor : ModuleRules
{
    public OpenPocketBaseSDKEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "OpenPocketBaseSDK", "UnrealEd" });
    }
}
