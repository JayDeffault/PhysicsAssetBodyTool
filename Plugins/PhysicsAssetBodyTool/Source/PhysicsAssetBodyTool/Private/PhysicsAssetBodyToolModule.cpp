#include "PhysicsAssetBodyToolModule.h"
#include "SPhysicsAssetBodyTool.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "PhysicsAssetBodyTool"
static const FName PhysicsAssetBodyToolTabName("PhysicsAssetBodyTool");

void FPhysicsAssetBodyToolModule::StartupModule()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PhysicsAssetBodyToolTabName, FOnSpawnTab::CreateRaw(this, &FPhysicsAssetBodyToolModule::SpawnToolTab))
        .SetDisplayName(LOCTEXT("TabTitle", "Physics Asset Body Tool"))
        .SetTooltipText(LOCTEXT("Tooltip", "Open the Physics Asset Body Tool editor."))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory())
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
    ToolMenusHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPhysicsAssetBodyToolModule::RegisterMenus));
}

void FPhysicsAssetBodyToolModule::ShutdownModule()
{
    if (UToolMenus::IsToolMenusAvailable()) UToolMenus::UnRegisterStartupCallback(ToolMenusHandle);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PhysicsAssetBodyToolTabName);
}

void FPhysicsAssetBodyToolModule::RegisterMenus()
{
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
    FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
    Section.AddMenuEntry("PhysicsAssetBodyTool", LOCTEXT("MenuLabel", "Physics Asset Body Tool"), LOCTEXT("MenuTooltip", "Open Physics Asset Body Tool."), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &FPhysicsAssetBodyToolModule::OpenToolWindow)));
}

void FPhysicsAssetBodyToolModule::OpenToolWindow()
{
    FGlobalTabmanager::Get()->TryInvokeTab(PhysicsAssetBodyToolTabName);
}

TSharedRef<SDockTab> FPhysicsAssetBodyToolModule::SpawnToolTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SPhysicsAssetBodyTool)];
}

IMPLEMENT_MODULE(FPhysicsAssetBodyToolModule, PhysicsAssetBodyTool)
#undef LOCTEXT_NAMESPACE
