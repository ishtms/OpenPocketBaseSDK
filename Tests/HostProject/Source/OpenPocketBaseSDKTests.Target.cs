using UnrealBuildTool;

public class OpenPocketBaseSDKTestsTarget : TargetRules
{
    public OpenPocketBaseSDKTestsTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("OpenPocketBaseSDKPackageHost");
    }
}
