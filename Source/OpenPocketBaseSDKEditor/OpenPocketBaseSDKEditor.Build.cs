using UnrealBuildTool;

public class OpenPocketBaseSDKEditor : ModuleRules
{
    public OpenPocketBaseSDKEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "OpenPocketBaseSDK" });
        PrivateDependencyModuleNames.Add("UnrealEd");
    }
}
