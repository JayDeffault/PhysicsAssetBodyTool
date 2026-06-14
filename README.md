# Physics Asset Body Tool

Editor-only Unreal Engine 5.5 plugin that adds a dockable Slate window for direct `UPhysicsAsset` body and constraint authoring. The implementation edits standard physics asset data (`SkeletalBodySetups`, `AggGeom` primitives, and `ConstraintSetup`) and uses transactional editor operations for undo/redo.

## Installation

The recommended installation method is **per-project installation**: copy `Plugins/PhysicsAssetBodyTool` into your project's `Plugins` directory, regenerate project files, build the project with Unreal Engine 5.5, enable the plugin, then open it from `Window > Physics Asset Body Tool`.

Engine-wide installation is also possible by copying the plugin into the UE 5.5 engine `Engine/Plugins` tree, but project installation is easier to version and safer for teams.

See [`INSTALL.md`](INSTALL.md) for detailed Russian installation instructions.
