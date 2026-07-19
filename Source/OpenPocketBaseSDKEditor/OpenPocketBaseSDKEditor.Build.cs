using UnrealBuildTool;

public class OpenPocketBaseSDKEditor : ModuleRules
{
    public OpenPocketBaseSDKEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "OpenPocketBaseSDK" });
        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "AssetRegistry",
                "BlueprintGraph",
                "CoreUObject",
                "DataValidation",
                "Engine",
                "GraphEditor",
                "InputCore",
                "Json",
                "Kismet",
                "PropertyEditor",
                "Projects",
                "Slate",
                "SlateCore",
                "UnrealEd"
            });
    }
}
