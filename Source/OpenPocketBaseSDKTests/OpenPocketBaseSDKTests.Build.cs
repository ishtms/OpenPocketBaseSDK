using UnrealBuildTool;
using System.IO;

public class OpenPocketBaseSDKTests : ModuleRules
{
    public OpenPocketBaseSDKTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "../OpenPocketBaseSDK/Private"));
        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "HTTP",
                "Json",
                "JsonUtilities",
                "OpenPocketBaseSDK",
                "OpenPocketBaseSDKAdmin"
            });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new[]
                {
                    "BlueprintGraph",
                    "KismetCompiler",
                    "OpenPocketBaseSDKEditor",
                    "UnrealEd"
                });
        }
    }
}
