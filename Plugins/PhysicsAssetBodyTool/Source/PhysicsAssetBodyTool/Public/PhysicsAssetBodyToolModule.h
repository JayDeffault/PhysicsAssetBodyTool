#pragma once
#include "Modules/ModuleManager.h"

class FPhysicsAssetBodyToolModule final : public IModuleInterface
{
public:
    void StartupModule() override;
    void ShutdownModule() override;
private:
    void RegisterMenus();
    void OpenToolWindow();
    TSharedRef<class SDockTab> SpawnToolTab(const class FSpawnTabArgs& Args);
    FDelegateHandle ToolMenusHandle;
};
