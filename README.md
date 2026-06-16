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

## MVP 3 status

Current MVP 3 coverage includes:

- mirrored primitive and convex body data through `FVehiclePhATMirrorUtils`;
- mirror preview that reports source/target body availability and shape counts, including convex counts;
- constraint presets for Door 60, Door 70, Bonnet 65, Boot 70, and Custom;
- explicit `Update Existing` and `Flip Axis` controls in the Create Vehicle Constraint dialog.

The remaining work after MVP 3 is the interactive convex creation/editing workflow from selected skeletal mesh vertices.

## Constraint frame notes

MVP 3 constraint creation now places the generated constraint frames at the selected child bone relative to the selected parent bone using the preview skeletal mesh reference skeleton. This avoids creating a new constraint with both reference frame positions at local `(0, 0, 0)`, which can cause the joint to appear at the skeletal mesh origin when simulation starts.

For door-style presets, the parent reference frame is biased by the selected angular limit around yaw so a 60/70 degree hinge behaves like an outward range instead of a symmetric inward/outward range. Bonnet/Boot presets use a vertical hinge bias. All newly created or updated vehicle constraints have `Disable Collision` enabled by default, with a checkbox in the dialog if you need to turn it off.

## MVP 4 status

Current MVP 4 coverage includes a safe C++ Slate convex creation dialog. It lets you choose a target body bone, seed an editable local-space point cloud from the current body shape, paste or edit one `X Y Z` point per line, preview the parsed point count, and apply the points as a new `FKConvexElem` through `FVehiclePhATConvexUtils::AddConvexFromPoints` with Undo/Redo support.

The full viewport vertex picker and automatic skeletal-mesh skin-weight vertex selection are still planned follow-up work.

## MVP 5 status

Current MVP 5 coverage includes a safe convex edit workflow. The Convex Edit Tool loads an existing `FKConvexElem` by body bone and convex index, extracts its `VertexData` into an editable local-space point cloud, previews the parsed point count, and rebuilds/replaces the selected convex element through `FVehiclePhATConvexUtils::ReplaceConvexFromPoints` with Undo/Redo support.

Direct viewport vertex dragging and snap-to-skeletal-mesh-vertex editing remain future work.

## Mirror troubleshooting notes

Mirror apply now duplicates source body setups for newly created target bodies instead of constructing blank body setups first, invalidates target body physics data after mirrored geometry is assigned, and drops invalid mirrored convex elements with fewer than four vertices or NaN coordinates. These guards are intended to avoid debugger breakpoints/asserts during mirror while still creating valid mirrored bodies.
