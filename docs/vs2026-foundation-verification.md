# VS2026 Foundation Verification Record

## 2026-11-24 verification

- On Windows 11 x64, Visual Studio Community 2026 18.10.2 selected MSVC 19.51.36260.0 (`v145`), Windows SDK 10.0.26100.0, and bundled CMake/CTest 4.3.1-msvc1.
- `scripts/check.ps1` configured a fresh `build` tree, built Debug, passed CTest (`foundation_smoke`, 1/1), and completed the clang-format and clang-tidy checks. The existing build was preserved as `build-old-20261124-104217`.
- `setup.cmd` then exited 0 with the same build checks, pre-commit 4.3.0 installed from cached wheels, and the hook installed in a temporary Git directory. The shared worktree hook was left in place. The run log is `.setup/logs/setup-20261124-104637-6072.log`.
- The installer and legacy cleanup fixtures both passed.
- An independent copy started with no project tools, downloads, Python package cache, or build. GitHub DNS resolution interrupted the first run. After retrying in the same copy, `setup.cmd` downloaded the remaining tools and dependencies, verified pinned hashes, and exited 0. `foundation_smoke` passed (1/1). The successful first-setup log is `.setup/clean-online/.setup/logs/setup-20261124-105204-21932.log`.
- In VS Code, saving a test C file matched the pinned clang-format output, and clang-tidy automatically reported an assignment in an `if` condition. The final local build and CTest passed again after the editor configuration changes.
- GitHub Actions and setup on other team members' PCs are pending.

## 2026-11-10 verification

Baseline HEAD: `85235eedb37860ee1282a9c4bef910cce24f9563` (`chore-vs2026-build`). Results below are from this worktree on 2026-11-10. Historical results from the previous toolchain are explicitly separated in [development.md](development.md) and [environment-setup.md](environment-setup.md).

## Environment

The elevated run selected Visual Studio Community 2026 18.x at `C:\Program Files\Microsoft Visual Studio\18\Community`, MSVC 19.51.36260.0 (`cl.exe` under that instance), Windows SDK 10.0.26100.0, and the instance-bundled CMake/CTest 4.3.1-msvc1. The existing `build/CMakeCache.txt` has generator `Visual Studio 18 2026`, instance `C:/Program Files/Microsoft Visual Studio/18/Community`, platform `x64`, but an empty `CMAKE_GENERATOR_TOOLSET`; it is treated as stale relative to the explicit `v145` preset. It was not deleted or modified to migrate it.

## Checks

| Command/check | Result |
| --- | --- |
| PowerShell AST parse: `scripts/check.ps1`, `scripts/setup.ps1`, `scripts/visual-studio-2026.ps1`, `tools/clean-build.ps1` | PASS, exit 0, no parser errors |
| JSON parse: `CMakePresets.json`, `tools/versions.json`, `.vscode/settings.json`, `.vscode/extensions.json` | PASS, exit 0 |
| `git diff --check` (initial integration check) | PASS, exit 0; Git emitted only an LF-to-CRLF notice for ADR-0002 |
| VS2026 selector | PASS; found required 18.x instance, compiler >=19.50, SDK >=10.0.22000.0, CMake/CTest >=4.2 and matching 4.3.1 |
| Pinned Git/Python downloads during setup | PASS; Git 2.50.1.windows.1 and Python 3.13.7 archives matched manifest SHA-256 and executable versions |
| `scripts/test-install-tools-fixtures.ps1` | PASS, exit 0; cache reuse/recovery, hash mismatch, failed-download and resume-to-full-retry fixtures |
| `scripts/test-cleanup-legacy-build-fixtures.ps1` | PASS, exit 0; independently reviewed cleanup fixtures; no actual cleanup was performed |
| Bundled CMake `--list-presets` | PASS, exit 0; exactly the `default` Visual Studio 2026 x64 preset is listed |
| `scripts/check.ps1` VS toolchain path-type guard | PASS, exit 0; the actual selected VS2026 instance directory and absolute cl/CMake/CTest files passed; a file used as `Instance` and a directory used as `CMake` were rejected. The valid case then stopped at the expected missing pinned clang-format executable; configure/build was not reached |
| `scripts/setup.ps1` (after fixing exit-code capture in the CMake probe) | FAIL, exit 1; Git and Python installed/verified, then six curl attempts failed resolving `files.pythonhosted.org` while downloading clang-format 20.1.8; setup log: `.setup/logs/setup-20261110-104434-28048.log` |
| Bundled CMake `--preset default` using existing `build` | FAIL, exit 1; CMake rejected cached empty toolset versus requested `v145`. Cache was left untouched; build/build/CTest did not run |
| Fresh `.setup/vs2026-validation` configure with Visual Studio 18 2026, x64, v145, explicit selected instance and preset policy minimum | FAIL, exit 1 at `cmake/ValidateScintilla.cmake`; `.tools/scintilla/wscite/Scintilla.dll` is absent because setup stopped during earlier package acquisition. CMake selected the expected SDK and identified MSVC 19.51.36260.0 before the missing dependency check |
| Build and CTest | NOT RUN; configure did not complete |
| clang-format, clang-tidy, pre-commit | NOT RUN; clang-format wheel download failed, so setup did not reach those checks |
| GitHub Actions `windows-2025-vs2026` | NOT RUN; pending CI after integration |

Final `git diff --check` passed with exit 0 (Git also reports its configured LF-to-CRLF conversion notice for ADR-0002). PowerShell AST checks and JSON parses above were repeated after the final script edits and passed.

Earlier failed attempts were also captured: the first setup run (exit 1) exposed a PowerShell pipeline exit-code capture bug, corrected before the final setup run; an initial fresh configure (exit 1) omitted the preset's `CMAKE_POLICY_VERSION_MINIMUM=3.5`, and a subsequent command-line quoting error passed that value as `3`. The correctly quoted retry reached the missing Scintilla check above. A pre-elevation run had been blocked by CIM/FileTracker access; the elevated retry cleared host OS and toolchain checks, but did not complete setup or configure/build/test.

The verification commands do not modify or claim success for the stale `build` directory. To migrate it, inspect it and run `tools/clean-build.ps1` only when preserving the entire tree under `build-old-<timestamp>` is intended; the script moves, and does not delete, that directory.

The independently reviewed legacy cleanup is opt-in and defaults to preview; applying it uses `-Apply -QuarantineLegacyTools` to preserve old `.tools/` installs intact in quarantine. It does not include the CMake `build` cache. Its three files were integrated from the reviewed result and were not edited or exercised for a real cleanup in this work.

## Primary sources

- CMake [Visual Studio 18 2026 generator](https://cmake.org/cmake/help/v4.2/generator/Visual%20Studio%2018%202026.html) (introduced in CMake 4.2; supports instance selection).
- CMake [MSVC toolset mapping](https://cmake.org/cmake/help/latest/variable/MSVC_TOOLSET_VERSION.html) (v145) and [Windows target platform SDK variable](https://cmake.org/cmake/help/v4.2/variable/CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION.html).
- Microsoft [MSVC compiler versioning](https://learn.microsoft.com/en-us/cpp/overview/compiler-versions?view=msvc-160) (VS2026 MSVC 14.50 / compiler 1950).
- GitHub Actions official [runner image list](https://github.com/actions/runner-images/blob/main/README.md) and [Windows 2025 VS2026 software inventory](https://github.com/actions/runner-images/blob/main/images/windows/Windows2025-VS2026-Readme.md), which documents the `windows-2025-vs2026` image.
- Microsoft [CMake Tools extension](https://github.com/microsoft/vscode-cmake-tools), which supports selecting a Visual Studio instance through the preset vendor field and uses the selected VS CMake when no explicit path overrides it.

## Current limitations

- Full setup/build/test/lint and pre-commit remain unverified in this run because the PyPI host did not resolve and the resulting checkout lacks clang-format, clang-tidy, and the pinned Scintilla DLL.
- The prior build cache requires a non-destructive migration before the preset can configure it with explicit v145.
- CI and a clean Windows machine remain pending.
- clang-tidy is deliberately limited to current `.c` translation units, with C17 and explicit MSVC `INCLUDE`, cJSON, and Unity include paths. Visual Studio generators do not provide `compile_commands.json`; target compile definitions/options are not imported automatically. Review those arguments if the CMake targets change.

## Changed paths

`.github/workflows/ci.yml`, `.gitignore`, `.vscode/extensions.json`, `.vscode/settings.json`, `CMakeLists.txt`, `CMakePresets.json`, `README.md`, `docs/adr/0002-build-system.md`, `docs/adr/0003-compiler-integration-policy.md`, `docs/adr/0008-testing-strategy.md`, `docs/adr/0011-gui-implementation-simplification.md`, `docs/cleanup-legacy-build.md`, `docs/dev-prepare.md`, `docs/development-validation.md`, `docs/development.md`, `docs/environment-setup.md`, `docs/tooling-validation.md`, `docs/vs2026-foundation-verification.md`, `scripts/check.ps1`, `scripts/setup.ps1`, `scripts/visual-studio-2026.ps1`, `scripts/cleanup-legacy-build.ps1`, `scripts/test-cleanup-legacy-build-fixtures.ps1`, `tools/clean-build.ps1`, `tools/versions.json`.
