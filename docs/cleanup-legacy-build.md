# Legacy build artifact cleanup

Run this only from a project checkout after reviewing the dry-run output. Every candidate is shown with an absolute path and reason.

```powershell
# 1. Preview recognized stale cache and staging artifacts; changes nothing.
.\scripts\cleanup-legacy-build.ps1

# 2. If the manifest no longer defines CMake/Ninja, preview their intact move
#    into a unique project-local quarantine directory.
.\scripts\cleanup-legacy-build.ps1 -QuarantineLegacyTools

# 3. After reviewing both source and destination paths, explicitly opt in.
.\scripts\cleanup-legacy-build.ps1 -Apply
.\scripts\cleanup-legacy-build.ps1 -Apply -QuarantineLegacyTools

# Standard PowerShell WhatIf behavior is available for either operation.
.\scripts\cleanup-legacy-build.ps1 -Apply -QuarantineLegacyTools -WhatIf
```

The script requires `tools/versions.json` and a Git checkout root. It may remove only stale project-local `.setup/downloads` files matching recognized CMake/Ninja archive naming, and `.setup/staging` directories matching the installer's `tool-version-GUID` convention. When a tool is absent from the current manifest, its version cannot be assumed active; a recognized archive or stage is treated as stale. Tracked paths, unknown names, and paths containing reparse points are skipped. Failures to verify project boundaries or Git tracking stop the operation.

Legacy `.tools/cmake` and `.tools/ninja` directories are never recursively deleted. If their corresponding tool entries are absent from the manifest, `-QuarantineLegacyTools` previews an intact move into a unique directory under `.setup/legacy-quarantine`; only combining it with `-Apply` performs the move. Unknown and user-added contents are retained in that backup. Directories whose tool is still listed in the manifest are preserved. The script never removes `CMakeUserPresets.json`, build caches, arbitrary `out/` directories, or system/Visual Studio installations; VS2026 toolchains are outside its candidate paths.

Run the synthetic fixture suite in disposable temporary repositories:

```powershell
.\scripts\test-cleanup-legacy-build-fixtures.ps1
```

Build cache cleanup remains outside this script. For the existing project `build/` backup migration, see [`tools/clean-build.ps1`](../tools/clean-build.ps1): it moves an untracked `build/` directory to a timestamped `build-old-*` backup and does not delete the contents. That command has no preview mode; review its source and target paths before running it.
