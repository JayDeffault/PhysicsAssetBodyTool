using UnrealBuildTool;

public class PhysicsAssetBodyTool : ModuleRules
{
    public PhysicsAssetBodyTool(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Slate", "SlateCore", "InputCore", "PhysicsCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "EditorFramework", "UnrealEd", "ToolMenus", "PropertyEditor", "PhysicsUtilities", "RenderCore", "RHI", "Projects", "ContentBrowser", "AdvancedPreviewScene" });
    }
}
