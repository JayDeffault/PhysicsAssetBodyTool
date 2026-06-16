#pragma once

#include "Modules/ModuleManager.h"

class FVehiclePhATToolsModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    void OpenVehiclePhATToolsTab();
    TSharedRef<class SDockTab> SpawnVehiclePhATToolsTab(const class FSpawnTabArgs& Args);
};
