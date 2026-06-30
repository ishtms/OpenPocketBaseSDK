using UnrealBuildTool;

public class OpenPocketBaseSDKTests : ModuleRules
{
    public OpenPocketBaseSDKTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Json",
                "JsonUtilities",
                "OpenPocketBaseSDK"
            });
    }
}
