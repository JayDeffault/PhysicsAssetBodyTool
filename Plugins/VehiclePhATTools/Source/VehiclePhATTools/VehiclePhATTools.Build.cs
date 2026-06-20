using UnrealBuildTool;

public class VehiclePhATTools : ModuleRules
{
    public VehiclePhATTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "PhysicsCore",
            "Json"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "InputCore",
            "PropertyEditor",
            "AssetTools",
            "AppFramework",
            "EditorFramework",
            "EditorStyle"
        });
    }
}
