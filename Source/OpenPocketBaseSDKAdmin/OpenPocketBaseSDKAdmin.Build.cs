using UnrealBuildTool;

public class OpenPocketBaseSDKAdmin : ModuleRules
{
    public OpenPocketBaseSDKAdmin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Json",
                "JsonUtilities",
                "OpenPocketBaseSDK"
            });

        bool shippingEnabled = false;
        foreach (string definition in Target.GlobalDefinitions)
        {
            if (definition == "OPENPOCKETBASESDK_ADMIN_SHIPPING_ENABLED=1")
            {
                shippingEnabled = true;
                break;
            }
        }
        PublicDefinitions.Add(
            "OPENPOCKETBASESDK_ADMIN_SHIPPING_ENABLED=" + (shippingEnabled ? "1" : "0"));
    }
}
