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

Direct viewport rendering/input still has to be connected inside the native Physics Asset Editor viewport client, but the plugin state now supports selecting a convex point and moving it through the standard editor transform gizmo once that viewport client forwards widget deltas.

## Mirror troubleshooting notes

Mirror apply now duplicates source body setups for newly created target bodies instead of constructing blank body setups first, invalidates target body physics data after mirrored geometry is assigned, and drops invalid mirrored convex elements with fewer than four vertices or NaN coordinates. These guards are intended to avoid debugger breakpoints/asserts during mirror while still creating valid mirrored bodies.

## Native PhAT convex editing direction

The temporary custom Slate point viewport has been removed from the Vehicle PhAT Tools panel. Convex creation/editing should be driven by the standard native Physics Asset Editor viewport, where the Skeletal Mesh and physics bodies are visible and camera controls already work. The remaining UI keeps text/list-based fallback controls, while `FVehiclePhATNativeConvexTool` provides the state and helper API that a PhAT viewport/client integration should call for vertex snapping, hover, create/move/delete, and apply.

## Native convex tool state scaffold

A shared `FVehiclePhATNativeConvexTool` state object now tracks create/edit mode, active PhysicsAsset/body, convex index, hover index, and editable convex points. This is the bridge point for the next implementation step: wiring native Physics Asset Editor viewport hit-testing to `AddPoint`, `MoveHoveredPoint`, `DeleteHoveredPoint`, and `Apply` so the PhAT viewport can drive the convex workflow directly.

## Native PhAT transform-gizmo bridge

`FVehiclePhATNativeConvexTool` now tracks a selected convex vertex separately from the hovered vertex. The native PhAT viewport client can call `SelectNearestPointToRay` or `SelectHoveredPoint` on click, return `GetSelectedPointTransform` as the widget location, and call `ApplySelectedPointDelta` from the viewport transform-gizmo delta handler. This keeps vertex movement in the standard PhAT viewport instead of opening a separate viewport.

## Native PhAT visible vertex markers

Until the Physics Asset Editor viewport client is directly extended, the Create/Edit Convex tools now create small temporary sphere markers at the convex point positions inside the selected body. These marker spheres are visible in the normal PhAT viewport with the Skeletal Mesh and physics bodies, can be selected/moved with PhAT's existing transform gizmo, and are pulled back into the convex point cloud on Apply. The markers are intentionally kept after Apply/Rebuild so PhAT does not keep a dangling selection to a just-deleted primitive; **Clear Viewport Markers** safely shrinks marker spheres instead of removing array elements, which avoids PhAT selection index assertions.

The convex dialogs also poll the marker spheres while open, but they debounce updates: the target `FKConvexElem` is updated only after marker positions stop changing for a short delay, not on every tick while the transform gizmo is moving. In create mode the first settled marker movement creates a live convex element, then later settled marker movements update that same element instead of adding duplicates.

## Skeletal mesh vertex snapping scaffold

`FVehiclePhATNativeConvexTool` can now snap candidate convex points to the nearest vertex in the preview Skeletal Mesh render data. The native PhAT viewport bridge should call `AddPointSnappedToMesh` / `MoveHoveredPointSnappedToMesh` after converting the viewport hit location into PhysicsAsset local space.

## Native PhAT ray interaction scaffold

`FVehiclePhATNativeConvexTool` also exposes ray-based helpers for the future PhAT viewport client hook: `FindNearestPreviewMeshVertexToRay`, `UpdateHoverFromRay`, `AddPointFromRay`, and `MoveHoveredPointFromRay`. The native viewport integration should pass its mouse ray in PhysicsAsset/SkeletalMesh local space so these helpers can snap directly to the nearest preview mesh vertex.

## Native PhAT viewport action mapping

`FVehiclePhATNativeConvexTool::HandleViewportRayAction` now centralizes the intended native viewport behavior: hover updates yellow selection state, primary press moves the hovered point or creates a snapped point, primary drag moves the hovered point, and secondary press deletes the hovered point. A future PhAT viewport client hook should only need to convert mouse input into local-space rays and call this method.

## Native PhAT viewport render data scaffold

`FVehiclePhATNativeConvexTool::BuildViewportRenderData` exports point and closed-segment render data for a future native PhAT viewport overlay. Hovered vertices are exported as yellow/larger points, non-hovered vertices as blue points, and segments close the convex loop when enough points exist.

## Native PhAT viewport draw hook

Use `FVehiclePhATNativeConvexTool::BuildViewportRenderData` from a Physics Asset Editor viewport draw path and draw the returned points/segments with whatever draw API is available in that viewport module. This avoids including private or version-specific viewport draw headers from the plugin public API.
