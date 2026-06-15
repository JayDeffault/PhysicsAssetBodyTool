# PhysicsAssetBodyTool / VehiclePhATTools

This repository contains the `VehiclePhATTools` Unreal Engine editor plugin under `Plugins/VehiclePhATTools`.

## Why `Binaries` and `Intermediate` may not appear

UnrealBuildTool creates `Binaries` and `Intermediate` for a plugin only when that plugin is part of the target being built. If those folders do not appear after a build, the usual causes are:

1. **The plugin was not enabled in a `.uproject`.**
   The plugin descriptor has `EnabledByDefault: false`, so a project must explicitly enable it.
2. **Only project files were generated.**
   `GenerateProjectFiles` does not compile the plugin and may not create plugin `Binaries`.
3. **The plugin was copied outside the project or engine plugin tree.**
   For a project plugin, the expected path is `YourProject/Plugins/VehiclePhATTools/VehiclePhATTools.uplugin`.
4. **The target was not an editor target.**
   `VehiclePhATTools` is an editor module and is built for editor targets only.
5. **Hot reload / Live Coding did not need to emit a plugin binary.**
   Do a clean editor build if you need deterministic plugin output folders.

This repository now includes `PhysicsAssetBodyTool.uproject`, which explicitly enables `VehiclePhATTools` for editor builds so UnrealBuildTool has a project target that includes the plugin.

## Build example for UE 5.5

From the Unreal Engine root, run a command similar to:

```bash
Engine/Build/BatchFiles/RunUAT.bat BuildPlugin ^
  -Plugin="C:/Path/To/PhysicsAssetBodyTool/Plugins/VehiclePhATTools/VehiclePhATTools.uplugin" ^
  -Package="C:/Path/To/VehiclePhATTools_Packaged" ^
  -TargetPlatforms=Win64
```

Or build the host project editor target from your IDE / UnrealBuildTool after opening or generating project files for `PhysicsAssetBodyTool.uproject`.

After a successful plugin build, generated files should appear in one of these places depending on build method:

- `Plugins/VehiclePhATTools/Binaries/...`
- `Plugins/VehiclePhATTools/Intermediate/...`
- the packaged output directory passed to `BuildPlugin`
- the host project's top-level `Binaries` / `Intermediate` directories

Generated `Binaries` and `Intermediate` directories are intentionally not committed to source control.
