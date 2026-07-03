using UnrealBuildTool;

public class OpenPocketBaseSDKTestsEditorTarget : TargetRules
{
    public OpenPocketBaseSDKTestsEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("OpenPocketBaseSDKPackageHost");
    }
}
