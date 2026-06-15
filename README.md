# PhysicsAssetBodyTool / VehiclePhATTools

This repository contains the `VehiclePhATTools` Unreal Engine editor plugin under `Plugins/VehiclePhATTools`.

## Important: Generate Visual Studio Project Files is not a build

`Generate Visual Studio Project Files` only regenerates IDE metadata (`.sln`, `.vcxproj`, project-file metadata under the project `Intermediate` tree, etc.). It does **not** compile the plugin module, so it normally does **not** create `Plugins/VehiclePhATTools/Binaries`.

Depending on the Unreal Engine version and whether the plugin is enabled, it also may not create a plugin-local `Plugins/VehiclePhATTools/Intermediate` folder during project-file generation. Plugin-local `Intermediate` and `Binaries` are created when UnrealBuildTool actually builds the plugin/editor target.

## Checklist when adding the plugin to your own project

1. Put the plugin at this exact project-plugin path:

   ```text
   YourProject/Plugins/VehiclePhATTools/VehiclePhATTools.uplugin
   ```

2. Make sure the plugin is enabled. This plugin now has `EnabledByDefault: true`, but the safest option is to also add it explicitly to your `.uproject`:

   ```json
   "Plugins": [
     {
       "Name": "VehiclePhATTools",
       "Enabled": true,
       "TargetAllowList": ["Editor"]
     }
   ]
   ```

3. Regenerate project files if you want Visual Studio project entries.
4. Build an editor target, for example `YourProjectEditor Win64 Development`, from Visual Studio / Rider or UnrealBuildTool.
5. Only after the editor target/plugin is built should you expect generated plugin artifacts such as:

   ```text
   YourProject/Plugins/VehiclePhATTools/Binaries/Win64/UnrealEditor-VehiclePhATTools.dll
   YourProject/Plugins/VehiclePhATTools/Intermediate/Build/...
   ```

## Why `Binaries` and `Intermediate` may not appear

UnrealBuildTool creates `Binaries` and `Intermediate` for a plugin only when that plugin is part of the target being built. If those folders do not appear after a build, the usual causes are:

1. **Only project files were generated.**
   `GenerateProjectFiles` does not compile the plugin and usually does not create plugin `Binaries`.
2. **The plugin was not enabled in a `.uproject`.**
   If the plugin is disabled, UBT can skip the module entirely.
3. **The plugin was copied outside the project or engine plugin tree.**
   For a project plugin, the expected path is `YourProject/Plugins/VehiclePhATTools/VehiclePhATTools.uplugin`.
4. **The target was not an editor target.**
   `VehiclePhATTools` is an editor module and is built for editor targets only.
5. **Hot reload / Live Coding did not need to emit a plugin binary.**
   Do a clean editor build if you need deterministic plugin output folders.
6. **The build failed before UBT emitted artifacts.**
   Check `Saved/Logs`, Visual Studio output, or the UBT log for the first compile error.

This repository includes `PhysicsAssetBodyTool.uproject`, which explicitly enables `VehiclePhATTools` for editor builds so UnrealBuildTool has a project target that includes the plugin.

## Build examples for UE 5.5

### Build the plugin as a packaged plugin

From the Unreal Engine root, run a command similar to:

```bat
Engine\Build\BatchFiles\RunUAT.bat BuildPlugin ^
  -Plugin="C:\Path\To\PhysicsAssetBodyTool\Plugins\VehiclePhATTools\VehiclePhATTools.uplugin" ^
  -Package="C:\Path\To\VehiclePhATTools_Packaged" ^
  -TargetPlatforms=Win64
```

`BuildPlugin` writes the final output to the `-Package` directory, not necessarily back into the source plugin directory.

### Build inside a host project

After generating project files, build the host editor target from your IDE or command line. For example, from the Unreal Engine root:

```bat
Engine\Build\BatchFiles\Build.bat YourProjectEditor Win64 Development -Project="C:\Path\To\YourProject\YourProject.uproject" -WaitMutex
```

Generated `Binaries` and `Intermediate` directories are build artifacts and are intentionally not committed to source control.
