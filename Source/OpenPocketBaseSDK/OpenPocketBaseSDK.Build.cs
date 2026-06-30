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
                "Engine",
                "HTTP",
                "Json",
                "JsonUtilities"
            });

        PrivateDependencyModuleNames.Add("Projects");
    }
}
