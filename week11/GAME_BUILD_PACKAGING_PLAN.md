# Game Build / Packaging Plan

Date: 2026-05-02

This document tracks the plan for adding an Editor-driven game build feature.

The selected first step is **not** a separate project yet. We first add game-client configurations inside the existing `NipsEngine.vcxproj`, make `WITH_EDITOR=0` work, then later split to `NipsGame.vcxproj` if needed.

## Goal

From the Editor:

```text
File > Build Game...
```

produce a standalone game package:

```text
Builds/Windows/GameName/
  GameName.exe
  Asset/
    Scene/              cooked scene copies
    Cooked/Mesh/        cooked static mesh binary files
    Material/           referenced materials/material instances
    Texture/            referenced textures/images
    Script/             referenced Asset/Script lua files
  Shaders/
  LuaScript/            only referenced legacy LuaScript files, when needed
  Settings/
    Game.ini
    PackageManifest.txt
  Saves/
```

The package must run without Editor UI, Editor selection, Details, Content Browser, Undo/Redo, PIE viewport layout, or Editor input routing.

## Current Engine Facts

- `Release|x64` currently defines `WITH_EDITOR=1`.
- `NipsEngine.vcxproj` currently always includes `Source/Editor/*`.
- `FEngineLoop::CreateEngine()` is the engine selection point.
  - `IS_OBJ_VIEWER` creates `UObjViewerEngine`.
  - `WITH_EDITOR` creates `UEditorEngine`.
  - otherwise it creates `UEngine`.
- `UEngine` already initializes window, input system, renderer, resources, render pipeline, world tick, and render loop.
- `UEngine` does **not** yet perform game startup:
  - read `Game.ini`
  - load startup scene
  - create game world
  - spawn `AGameJamPlayerController`
  - spawn pawn from `PlayerStart`
  - set active camera
  - run game `BeginPlay`
- Editor menu entry belongs naturally in `FEditorToolbarWidget::RenderFilesMenu()`.
- Existing `ReleaseBuild.bat` packages an Editor Release build, not a true game client.

## Build Config Model

Editor Packaging should expose two build modes:

```text
Development
  maps to GameClientDebug|x64
  WITH_EDITOR=0
  IS_GAME_CLIENT=1
  _DEBUG
  Optimization disabled or low
  Debug info enabled

Shipping
  maps to GameClientRelease|x64
  WITH_EDITOR=0
  IS_GAME_CLIENT=1
  NDEBUG
  Optimization enabled
  Debug info optional
```

Recommended Visual Studio configurations:

```text
GameClientDebug|x64
GameClientRelease|x64
```

Recommended output:

```text
NipsEngine/Bin/GameClientDebug/NipsGame.exe
NipsEngine/Bin/GameClientRelease/NipsGame.exe
```

Editor may still display them as:

```text
Development
Shipping
```

## Packaging Settings

Editor Build Game UI should expose:

Required:

- Game Name
- Startup Scene
- Build Configuration: Development / Shipping
- Output Directory
- Clean Output Before Build

Useful follow-up options:

- Run After Build
- Window Title
- Width
- Height
- Fullscreen
- Include Saves

Initial `Game.ini`:

```ini
[Game]
GameName=MyGame
StartupScene=Asset/Scene/Main.Scene
WindowTitle=MyGame

[Window]
Width=1280
Height=720
Fullscreen=false

[Build]
Configuration=Development
```

## Runtime Game Engine

Add:

```text
NipsEngine/Source/Engine/Runtime/GameEngine.h
NipsEngine/Source/Engine/Runtime/GameEngine.cpp
```

`UGameEngine` responsibilities:

- read `Settings/Game.ini`
- resolve startup scene relative to packaged root
- create `EWorldType::Game` world context
- load scene through `FSceneSaveManager`
- spawn `AGameJamPlayerController`
- configure its runtime camera from window size/default FOV
- call `SpawnDefaultPawn()`
- set `World->SetActiveCamera(PlayerController->GetRuntimeCamera())`
- call world `BeginPlay()`
- tick world and render through default runtime pipeline

## Editor Packaging System

Add:

```text
NipsEngine/Source/Editor/Packaging/GamePackager.h
NipsEngine/Source/Editor/Packaging/GamePackager.cpp
```

Possible public API:

```cpp
struct FGameBuildSettings
{
    FString GameName;
    FString StartupScene;
    FString OutputDirectory;
    FString Configuration; // Development or Shipping
    int32 Width;
    int32 Height;
    bool bFullscreen;
    bool bCleanOutput;
    bool bRunAfterBuild;
};

class FGamePackager
{
public:
    bool BuildAndPackage(const FGameBuildSettings& Settings, FString& OutError);
};
```

Build Game flow:

1. Prompt save if current scene is dirty.
2. If current scene has no file path, force Save As.
3. Select or confirm Startup Scene.
4. Validate startup scene contains exactly one `APlayerStart`.
5. Run MSBuild with `GameClientDebug|x64` or `GameClientRelease|x64`.
6. Create/clean package output directory.
7. Copy game exe/PDB.
8. Cook scenes into package output.
   - Walk scene JSON strings.
   - Collect referenced runtime assets.
   - Convert `.obj` static meshes to package-local `.bin`.
   - Rewrite scene mesh paths from `.obj` to cooked `.bin`.
9. Copy dependency assets only.
   - `.mat`, `.matinst`, textures, audio, fonts, lua, prefab/meta files.
   - Material JSON is scanned recursively for texture/shader references.
   - Lua text is scanned for `Asset/`, `LuaScript/`, `Shaders/` string paths.
10. Copy `Shaders/` as a whole for now.
11. Write `Settings/Game.ini`.
12. Write `Settings/PackageManifest.txt`.
13. Create `Saves/` directory.
14. Show footer log / result popup.
15. Optionally open package folder or run game.

## Packaging Cook Status

Implemented first cook pass:

- `Asset/` is no longer copied wholesale during Packaging.
- Included scenes are written as cooked scene copies into the package.
- Scene `.obj` references are baked to `Asset/Cooked/Mesh/**/*.bin`.
- Scene mesh paths are rewritten to the cooked `.bin` paths.
- Referenced `.bin` meshes are copied as-is.
- Referenced `.mat`, `.matinst`, image, sound, font, lua, prefab, and meta files are copied on demand.
- `Materials` slot names in scenes are resolved against `Asset/Material/**/*.mat|*.matinst` when possible.
- Material/prefab JSON and Lua scripts are scanned recursively for additional asset path strings.
- `Shaders/` is still copied wholesale for stability.
- `Settings/PackageManifest.txt` records copied/cooked files and counts.

Known limitations:

- Dependency collection is string/path based, not a full typed asset graph yet.
- Lua dynamic path construction cannot be discovered unless the literal asset path appears in the script text.
- Material slot name resolution is heuristic for legacy OBJ/MTL-derived names.
- Shaders are intentionally not dependency-pruned yet.

## Engine Structure Regression Scan

These are current engine-structure issues that can break `WITH_EDITOR=0`.

Must inspect/fix before GameClient compile is considered stable:

- `Source/Engine/Runtime/EngineLoop.cpp`
  - includes `Editor/EditorEngine.h`
  - includes `Misc/ObjViewer/ObjViewerEngine.h`
  - close-request callback casts to `UEditorEngine`
  - shader watcher always runs
- `Source/Engine/GameFramework/PlayerController.h`
  - includes `Editor/Viewport/ViewportCamera.h`
  - owns `FViewportCamera RuntimeCamera`
- `Source/Engine/GameFramework/World.h`
  - stores `FViewportCamera* ActiveCamera`
- `Source/Engine/Render/Renderer/DefaultRenderPipeline.cpp`
  - includes `Editor/Viewport/ViewportCamera.h`
- Billboard/SubUV/TextRender components
  - use `FViewportCamera` for camera-facing behavior
- Runtime source files include Editor console/log UI:
  - `Asset/ObjLoader.cpp`
  - `Asset/StaticMesh.cpp`
  - `Asset/StaticMeshSimplifier.cpp`
  - `Component/DecalComponent.cpp`
  - `Core/ResourceManager.cpp`
  - `GameFramework/World.cpp`
  - `Render/SubUVBatcher.cpp`
  - `Runtime/WindowsFileWatcher.cpp`
- `Core/ResourceManager.cpp`
  - references `FEditorSettings::Get().ShowFlags.bEnableLOD`
- `Serialization/SceneSaveManager.h`
  - contains `FEditorCameraState`
  - acceptable short-term if optional, but name/boundary is misleading for runtime
- `Slate/SViewport.h`
  - includes `Editor/Viewport/FSceneViewport.h`
  - should not be part of GameClient if Slate viewport is Editor-only
- `GizmoComponent`
  - relies on editor mesh library concepts
  - should be excluded or guarded for GameClient

Expected runtime abstractions to introduce or move:

- move `FViewportCamera` to runtime, or create `FRuntimeCamera` and adapt world/render pipeline
- replace Runtime includes of `EditorConsoleWidget` with `Core/Debug.h` or runtime logging
- isolate `FEditorSettings` usage from `ResourceManager`
- make ShaderDirectoryWatcher editor-only
- guard `UEditorEngine` references in `FEngineLoop`

## Revised Batch Plan

### Batch 0: Documentation / Scope Lock

Size: small

- [x] Rewrite packaging plan around same-project GameClient config first.
- [x] Add Development/Shipping packaging model.
- [x] Add regression scan list from current engine structure.
- [x] Confirm output exe name: `NipsGame.exe`.
- [x] Confirm default output folder: `Builds/Windows/<GameName>`.

### Batch 1: Runtime Boundary Cleanup, Part 1

Size: medium-large

Goal: make Runtime stop depending on Editor UI/logging where the fix is straightforward.

- [x] Replace `EditorConsoleWidget` includes in Runtime with runtime logging.
- [x] Remove `FEditorSettings` dependency from `ResourceManager` or guard it.
- [x] Make `WindowsFileWatcher` logging runtime-safe.
- [x] Verify normal Editor Debug build still works.

Regression risk:

- asset load logs
- static mesh loading
- resource LOD behavior
- Lua/script logs if routed through runtime logging

### Batch 2: Runtime Camera Boundary

Size: large

Goal: decide and implement the camera type usable by GameClient without Editor includes.

Recommended path:

- [x] Move `FViewportCamera` from `Source/Editor/Viewport` to a runtime location.
- [x] Keep `Source/Editor/Viewport/ViewportCamera.h` as a compatibility include.
- [x] Update runtime camera users to include `Camera/ViewportCamera.h`.
- [x] Verify normal Editor Debug build still works.

Likely touched areas:

- `World.h`
- `PlayerController.h/.cpp`
- `DefaultRenderPipeline.cpp`
- `BillboardComponent`
- `SubUVComponent`
- `TextRenderComponent`
- Editor viewport camera compatibility layer if needed

Regression risk:

- editor viewport rendering
- PIE camera
- billboard facing
- SubUV facing
- text render facing
- PlayerController runtime camera

### Batch 3: Game Engine Bootstrap

Size: medium-large

- [x] Add `UGameEngine`.
- [x] Add `Game.ini` reader.
- [x] Add startup scene path resolution.
- [x] Create `EWorldType::Game` world.
- [x] Load scene with `FSceneSaveManager`.
- [x] Spawn `AGameJamPlayerController`.
- [x] Spawn pawn through PlayerStart.
- [x] Set active camera.
- [x] Call `BeginPlay()`.
- [x] Disable editor-only shader watcher in GameClient.

Regression risk:

- PIE and Game startup diverging
- PlayerStart assumptions
- scene paths relative to package root
- missing PlayerStart fallback

### Batch 4: GameClient Build Configurations

Size: large

- [x] Add `GameClientDebug|x64`.
- [x] Add `GameClientRelease|x64`.
- [x] Set `WITH_EDITOR=0`.
- [x] Set `IS_GAME_CLIENT=1`.
- [x] Set output/int dirs.
- [x] Exclude `Source/Editor/*`.
- [x] Exclude EditorController sources if needed.
- [x] Exclude Editor-only shaders from compile if needed.
- [x] Fix compile errors from missing Editor includes.

Regression risk:

- `.vcxproj` and `.filters` churn
- accidental exclusion from normal Editor configs
- Solution config mismatch

### Batch 5: Editor Build Settings UI

Size: medium

- [x] Add `Build Game...` to File menu.
- [x] Add modal/popup with:
  - Game Name
  - Startup Scene
  - Development/Shipping
  - Output Directory
  - Clean Output
  - Run After Build
- [x] Connect dirty scene save prompt.
- [x] Validate scene path exists.
- [ ] Save settings for repeated builds if useful.

Regression risk:

- menu UI conflicts
- scene dirty/save flow
- modal focus/input capture

### Batch 6: Packager Implementation

Size: medium-large

- [x] Add `FGamePackager`.
- [x] Discover MSBuild path or use configured MSBuild path.
- [x] Run correct config:
  - Development -> `GameClientDebug|x64`
  - Shipping -> `GameClientRelease|x64`
- [x] Write `Game.ini`.
- [x] Create clean package folder.
- [x] Copy exe.
- [x] Copy `Asset/`.
- [x] Copy `Shaders/`.
- [x] Copy `LuaScript/`.
- [x] Copy `Settings/Game.ini`.
- [x] Create `Saves/`.
- [x] Report stage-level failure.

Regression risk:

- blocking UI during build
- path quoting, especially spaces/Korean paths
- partial output after failed build
- stale package files if clean disabled

### Batch 7: End-to-End GameClient QA

Size: medium

- [x] Build Development package.
- [x] Build Shipping package.
- [x] Run package outside project root.
- [x] Startup scene does not fail during smoke launch.
- [x] PlayerStart spawns player.
- [x] Camera/input works.
- [x] StaticMesh/materials do not fail during smoke launch.
- [x] LuaScript folder is present in package.
- [x] Shaders load from package root during smoke launch.
- [x] Crash dumps are not produced during smoke launch.
- [x] Missing startup scene gives readable error in `Saves/Logs/Game.log`.

### Batch 8: Optional Project Split Preparation

Size: medium

This is not part of first implementation, but Batch 1-7 should make this easy later.

- [ ] Create `NipsGame.vcxproj` from working GameClient config.
- [ ] Keep Runtime source list.
- [ ] Keep Editor sources excluded.
- [ ] Add to solution.
- [ ] Update packager to build `NipsGame.vcxproj`.

## Omission Check

The plan includes:

- build configuration selection
- optimization/debug mode split
- Startup Scene selection
- Game.ini generation
- output directory selection
- package file copying
- Editor dirty scene prompt
- standalone GameClient bootstrap
- Runtime/Editor dependency cleanup
- current engine camera dependency issue
- current engine ResourceManager/EditorSettings dependency issue
- LuaScript packaging
- shader packaging
- crash dump/save directory expectation
- later project split path

Not included in first implementation:

- dependency-based asset cooking
- pak/archive format
- encrypted/package compressed assets
- dedicated server
- installer generation
- code signing
- separate `NipsRuntime.lib`

These are intentionally deferred.

## Final Direction

First implementation should be:

```text
same NipsEngine.vcxproj
  + GameClientDebug|x64
  + GameClientRelease|x64
  + WITH_EDITOR=0
  + IS_GAME_CLIENT=1
  + UGameEngine
  + Editor Build Game packaging UI
```

If that works, splitting to:

```text
NipsEditor.vcxproj
NipsGame.vcxproj
```

becomes a project-file operation instead of a combined code-boundary and project-boundary fight.

## Current Status

Active Batch: Batch 17 - Game branding icon and splash

Last Verified Build: Debug|x64, GameClientDebug|x64, and GameClientRelease|x64 passed on 2026-05-02 after Batch 17.

Last Edited Files:

- `GAME_BUILD_PACKAGING_PLAN.md`
- `NipsEngine.sln`
- `NipsEngine/NipsEngine.vcxproj`
- `NipsEngine/NipsEngine.vcxproj.filters`
- `NipsEngine/Source/Editor/Packaging/GameBuildSettings.h`
- `NipsEngine/Source/Editor/Packaging/GamePackager.h`
- `NipsEngine/Source/Editor/Packaging/GamePackager.cpp`
- `NipsEngine/Source/Editor/UI/EditorMainPanel.h`
- `NipsEngine/Source/Editor/UI/EditorMainPanel.cpp`
- `NipsEngine/Source/Engine/Runtime/GameSplashScreen.h`
- `NipsEngine/Source/Engine/Runtime/GameSplashScreen.cpp`
- `NipsEngine/Source/Engine/Runtime/WindowsApplication.cpp`
- `NipsEngine/Source/Engine/Runtime/GameBranding.rc`
- `NipsEngine/Source/Editor/UI/EditorSceneWidget.h`
- `NipsEngine/Source/Editor/UI/EditorToolbarWidget.h`
- `NipsEngine/Source/Editor/UI/EditorToolbarWidget.cpp`
- `NipsEngine/Source/Engine/Runtime/GameEngine.h`
- `NipsEngine/Source/Engine/Runtime/GameEngine.cpp`
- `NipsEngine/Source/Engine/Runtime/Engine.cpp`
- `NipsEngine/Source/Engine/Runtime/EngineLoop.cpp`
- `NipsEngine/Source/Engine/Component/Movement/PursuitMovementComponent.cpp`
- `NipsEngine/Source/Engine/Camera/ViewportCamera.h`
- `NipsEngine/Source/Engine/Camera/ViewportCamera.cpp`
- `NipsEngine/Source/Editor/UI/EditorConsoleWidget.cpp`
- `NipsEngine/Source/Editor/UI/EditorConsoleWidget.h`
- `NipsEngine/Source/Engine/Core/CoreMinimal.h`
- `NipsEngine/Source/Engine/Core/Logging/Log.h`
- `NipsEngine/Source/Engine/Core/Logging/Log.cpp`
- Runtime files that previously included `EditorConsoleWidget` only for logging
- Runtime camera users that previously included `Editor/Viewport/ViewportCamera.h`
- Removed `Source/Editor/Viewport/ViewportCamera.h/.cpp` forwarding shim

Open Risks:

- Editor input/controller code still lives under `Source/Engine/Input/Controller/EditorController`, but is excluded from GameClient configs.
- `OpaqueRenderPass` and `ResourceManager` are guarded with `WITH_EDITOR` and have now compiled under `WITH_EDITOR=0`.
- `Shader`/`Script`/`Texture` runtime logging now routes through `FLog`; Editor console receives it through a sink.
- `Source/Editor/Viewport/ViewportCamera.h/.cpp` forwarding shim has been removed.
- Game packaged input is minimal and routes directly to `AGameJamPlayerController`.
- GameClient configs still compile some render passes that are editor-oriented but runtime-safe; this can be slimmed in a later size pass.
- `GameClientDebug` intentionally uses release CRT with debug info because the available DirectXTK library is release-built.
- `FGamePackager` now runs from an Editor-side async task, but MSBuild itself is still a blocking child process inside that worker.
- MSBuild path is fixed to the current VS Community 2022 install path.
- Visible scene QA passed after the runtime render target fix.
- Visible camera/input QA passed for WASD/E/Q and mouse look.
- Missing-startup-scene behavior is now recorded in `Saves/Logs/Game.log`, but still has no visible in-window error screen.
- Game branding now writes a generated `.rc`; packaging must regenerate it before MSBuild so stale icons do not remain.

Next Step:

- Keep PIE and Standalone behavior aligned without making PIE launch the packaged GameClient exe.
- Optional: add a visible GameClient fatal/error screen for startup scene failures.

### 2026-05-02 Batch 17 - Game Branding Icon and Splash Started

Purpose:

- Let Packaging assign a game executable icon and startup splash image, similar to Unity-style branding.
- Keep the implementation package-driven so future project separation can reuse the same `Game.ini` branding section.

Implemented:

- Added `IconPath`, `SplashImagePath`, and `SplashMinSeconds` to `FGameBuildSettings`.
- Added Packaging Settings rows for:
  - Game Icon (`.ico`)
  - Splash Image (`.png`, `.jpg`, `.jpeg`, `.bmp`)
  - Splash Seconds
- Added optional branding validation before MSBuild.
- Added `Source/Engine/Runtime/GameBranding.rc`.
- Packager now regenerates `Source/Engine/Runtime/GameBranding.rc` before MSBuild:
  - selected icon -> resource id `101`
  - no icon -> empty rc file to avoid stale icon reuse
- Packager resets `GameBranding.rc` back to the empty default after MSBuild, so local absolute icon paths do not linger in the worktree.
- Packager copies selected icon/splash into package `Branding/`.
- Packager writes `[Branding]` to `Settings/Game.ini`.
- `FWindowsApplication` loads resource id `101` as window/taskbar icon when available.
- `GameBranding.rc` is excluded from Editor Debug/Release and included for GameClient configs only.
- Added `FGameSplashScreen`, a GameClient runtime splash window using GDI+.
- `FEngineLoop` shows the splash in GameClient before the main window/engine initialization and closes it after `BeginPlay`.

Validation to run:

- [x] `Debug|x64`
- [x] `GameClientDebug|x64`
- [x] `GameClientRelease|x64`
- Package with no icon/splash to verify fallback path.
- Package with `.ico` and `.png` to verify exe icon and startup splash.

## Work Log

### 2026-05-02 Batch 1 - Runtime Boundary Cleanup, Part 1 Started

Purpose:

- Reduce direct Runtime -> Editor dependencies before adding `WITH_EDITOR=0` GameClient configs.
- Keep later `NipsGame.vcxproj` split easy by preventing new Runtime dependencies on `Source/Editor`.

Planned files to inspect:

- `Source/Engine/Asset/ObjLoader.cpp`
- `Source/Engine/Asset/StaticMesh.cpp`
- `Source/Engine/Asset/StaticMeshSimplifier.cpp`
- `Source/Engine/Component/DecalComponent.cpp`
- `Source/Engine/Core/ResourceManager.cpp`
- `Source/Engine/GameFramework/World.cpp`
- `Source/Engine/Render/SubUVBatcher.cpp`
- `Source/Engine/Runtime/WindowsFileWatcher.cpp`

Success criteria:

- No simple Runtime logging-only include should depend on `Editor/UI/EditorConsoleWidget.h`.
- Existing Editor Debug x64 build still passes.

Result:

- Added runtime logging facade: `Source/Engine/Core/Logging/Log.h/.cpp`.
- `UE_LOG` now resolves through runtime `FLog` instead of `FEditorConsoleWidget`.
- Editor console registers itself as the runtime log sink in `FEditorConsoleWidget`.
- Removed Runtime `EditorConsoleWidget` includes from asset loading, resource loading, render bus, texture, shader, Lua coroutine/script, SubUV, world, decal, and file watcher paths.
- Guarded `ResourceManager` LOD generation setting with `WITH_EDITOR`; GameClient defaults to generating LODs.
- Guarded `OpaqueRenderPass` shadow filter access with `WITH_EDITOR`; GameClient defaults to `PCF`.
- Fixed hidden transitive dependency in `CoroutineScheduler` by explicitly including runtime `FString`.
- Verified: `MSBuild NipsEngine.sln /p:Configuration=Debug /p:Platform=x64` passed.

Follow-up:

- `FLog` currently uses one global sink and falls back to `OutputDebugStringA`; this is enough for GameClient bootstrap, but a later file log sink can be added for packaged builds.
- `WITH_EDITOR=0` has not been compiled yet; Batch 2 and Batch 4 are still required before that becomes meaningful.

### 2026-05-02 Batch 2 - Runtime Camera Boundary Completed

Purpose:

- Make `FViewportCamera` usable by GameClient without depending on `Source/Editor`.
- Preserve existing Editor/ObjViewer call sites while moving the implementation to Runtime.

Result:

- Added runtime camera files under `Source/Engine/Camera/ViewportCamera.h/.cpp`.
- Converted `Source/Editor/Viewport/ViewportCamera.h` into a compatibility forwarding header.
- Converted `Source/Editor/Viewport/ViewportCamera.cpp` into a shim comment file and removed it from compilation.
- Updated `NipsEngine.vcxproj` so the compiled implementation is `Source/Engine/Camera/ViewportCamera.cpp`.
- Updated Engine/runtime users to include `Camera/ViewportCamera.h` directly:
  - `PlayerController`
  - `DefaultRenderPipeline`
  - `BillboardComponent`
  - `SubUVComponent`
  - `TextRenderComponent`
  - `PursuitMovementComponent`
  - Editor controller files that currently live under `Source/Engine/Input`
- Verified there are no direct `Editor/Viewport/ViewportCamera.h` includes under `Source/Engine`.
- Verified: `MSBuild NipsEngine.sln /p:Configuration=Debug /p:Platform=x64` passed.

Follow-up:

- `FViewportCamera` is still named after viewport concepts. Renaming can wait until after GameClient builds; the dependency boundary is now the important part.
- Editor controller files still live in `Source/Engine/Input/Controller/EditorController`; they should be excluded or relocated when `GameClientDebug|x64` is added.

### 2026-05-02 Batch 3 - Game Engine Bootstrap Completed

Purpose:

- Add the runtime entry point that a packaged GameClient can use instead of `UEditorEngine`.
- Keep the implementation inside existing Runtime code so later `NipsGame.vcxproj` can reuse it.

Result:

- Added `UGameEngine` under `Source/Engine/Runtime`.
- `UGameEngine::Init` loads runtime resources through `UEngine`, then reads `Settings/Game.ini`.
- `Game.ini` currently supports:
  - `GameName`
  - `StartupScene`
  - `PlayerControllerClass`
- If `Settings/Game.ini` is missing, `UGameEngine` falls back to `Asset/Scene/Default.scene`.
- `UGameEngine` resolves StartupScene through `FPaths::ToAbsoluteString`.
- Startup scene loading uses `FSceneSaveManager::Load`.
- Loaded worlds are forced to `EWorldType::Game`.
- If startup scene loading fails, an empty `EWorldType::Game` world is created.
- `UEngine::CreateWorldContext` now also sets `UWorld::WorldType`.
- `UGameEngine::BeginPlay` spawns the configured `PlayerControllerClass` before `UWorld::BeginPlay`.
- Existing `APlayerController::BeginPlay` then spawns/possesses the default pawn through `APlayerStart` when available.
- `UGameEngine::Tick` pumps keyboard and mouse deltas into the active `APlayerController`, then ticks/renders the game world.
- `FEngineLoop::CreateEngine` now selects `UGameEngine` for `!WITH_EDITOR && !IS_OBJ_VIEWER`.
- Editor-only close prompts and shader directory watcher are guarded out for GameClient.
- Verified: `MSBuild NipsEngine.sln /p:Configuration=Debug /p:Platform=x64` passed.

Follow-up:

- Batch 4 must add real GameClient configurations so `WITH_EDITOR=0` is actually compiled.
- After Batch 4, any remaining editor-only compile breaks should be handled by excluding Editor files from GameClient or guarding specific code.

### 2026-05-02 Batch 4 - GameClient Build Configurations Completed

Purpose:

- Add actual game-client Visual Studio configurations inside the existing project.
- Prove that `WITH_EDITOR=0` can compile and link before building the packaging UI.

Result:

- Added solution configs:
  - `GameClientDebug|x64`
  - `GameClientRelease|x64`
- Added project configs with:
  - `WITH_EDITOR=0`
  - `IS_GAME_CLIENT=1`
  - `IS_OBJ_VIEWER=0`
  - output under `NipsEngine/Bin/GameClientDebug/` and `NipsEngine/Bin/GameClientRelease/`
  - target name `NipsGame.exe`
- Excluded shader `FxCompile` from GameClient configs, matching existing x64 editor configs.
- Excluded `Source/Editor/*` compile items from GameClient configs.
- Excluded `Source/Misc/ObjViewer/*` compile items from GameClient configs.
- Excluded `Source/Engine/Input/Controller/EditorController/*` compile items from GameClient configs.
- Fixed `UPursuitMovementComponent` so GameClient no longer links against `UEditorEngine`; it falls back to the owning world's active camera.
- Adjusted `GameClientDebug` to use release CRT with debug symbols because the current DirectXTK package is release-built.

Verified:

- `MSBuild NipsEngine.sln /p:Configuration=GameClientDebug /p:Platform=x64` passed.
- `MSBuild NipsEngine.sln /p:Configuration=GameClientRelease /p:Platform=x64` passed.
- `MSBuild NipsEngine.sln /p:Configuration=Debug /p:Platform=x64` passed after project changes.

Outputs:

- `NipsEngine/Bin/GameClientDebug/NipsGame.exe`
- `NipsEngine/Bin/GameClientRelease/NipsGame.exe`

Follow-up:

- Batch 5 should expose `Build Game...` in the Editor UI.
- Batch 6 should copy `NipsGame.exe`, `Asset/`, `Shaders/`, `LuaScript/`, and generated `Settings/Game.ini` into `Builds/Windows/<GameName>/`.
- Later project split is easier now: Runtime/GameClient compiles without Editor source files.

### 2026-05-02 Cleanup - Removed ViewportCamera Forwarding Shim

Purpose:

- Remove the temporary `Source/Editor/Viewport/ViewportCamera.h` forwarding header after the runtime camera move.

Result:

- Updated remaining Editor and ObjViewer includes to use `Camera/ViewportCamera.h` directly.
- Deleted `Source/Editor/Viewport/ViewportCamera.h`.
- Deleted `Source/Editor/Viewport/ViewportCamera.cpp`.
- Removed the deleted header from `NipsEngine.vcxproj` and filters.
- Verified no source file or project item references `Editor/Viewport/ViewportCamera.h`.

Verified:

- `MSBuild NipsEngine.sln /p:Configuration=Debug /p:Platform=x64` passed.
- `MSBuild NipsEngine.sln /p:Configuration=GameClientDebug /p:Platform=x64` passed.

### 2026-05-02 Batch 5 - Editor Build Settings UI Completed

Purpose:

- Add the Editor-facing entry point for standalone game builds.
- Keep the settings model reusable for a future separate game project.

Result:

- Added `FGameBuildSettings` under `Source/Editor/Packaging`.
- Added `File > Build Game...`.
- Added `Build Game` modal with:
  - Game Name
  - Startup Scene
  - scene browse button
  - Output Directory
  - Development / Shipping
  - Clean Output
  - Run After Build
- Connected dirty-scene save prompt before opening the build modal.
- Added startup scene existence validation.

Verified:

- `MSBuild NipsEngine.sln /p:Configuration=Debug /p:Platform=x64` passed.

Follow-up:

- Persisting build settings between Editor runs is still deferred.

### 2026-05-02 Batch 6 - Packager Implementation Completed

Purpose:

- Make `Build Game...` create a standalone package folder instead of only preparing settings.

Result:

- Added `FGamePackager` under `Source/Editor/Packaging`.
- `Build` button now runs MSBuild:
  - Development -> `GameClientDebug|x64`
  - Shipping -> `GameClientRelease|x64`
- Package output writes to the selected output directory.
- Clean output is supported.
- Copies:
  - `NipsGame.exe`
  - `Asset/`
  - `Shaders/`
  - `LuaScript/`
- Generates:
  - `Settings/Game.ini`
  - `Saves/`
- Reports success/failure through footer logs.

Verified:

- `MSBuild NipsEngine.sln /p:Configuration=Debug /p:Platform=x64` passed.
- `MSBuild NipsEngine.sln /p:Configuration=GameClientDebug /p:Platform=x64` passed.
- `MSBuild NipsEngine.sln /p:Configuration=GameClientRelease /p:Platform=x64` passed.

Follow-up:

- Packaging currently blocks the Editor while MSBuild/copy is running.
- End-to-end package launch QA is Batch 7.

### 2026-05-02 Batch 7 - End-to-End GameClient Smoke QA Completed

Purpose:

- Verify the package layout and confirm GameClient can launch from package roots.

Result:

- Created Shipping-style package at `Builds/Windows/NipsGame`.
- Created Development-style package at `Builds/Windows/NipsGameDev`.
- Verified package contains:
  - `NipsGame.exe`
  - `Asset/`
  - `Shaders/`
  - `LuaScript/`
  - `Settings/Game.ini`
  - `Saves/`
- Verified `Settings/Game.ini` contains `StartupScene=Asset/Scene/Default.scene`.
- Verified `Asset/Scene/Default.scene` exists in package.
- Verified package file count is non-empty and asset/shader folders are copied.
- Ran Shipping package from `Builds/Windows/NipsGame`; process stayed alive for 5 seconds.
- Copied Shipping package to `%TEMP%/NipsGameSmokePackage` and ran outside project root; process stayed alive for 5 seconds.
- Ran Development package from `Builds/Windows/NipsGameDev`; process stayed alive for 5 seconds.
- No crash dump was produced during smoke launch.

Verified:

- `MSBuild NipsEngine.sln /p:Configuration=Debug /p:Platform=x64` passed.
- `MSBuild NipsEngine.sln /p:Configuration=GameClientDebug /p:Platform=x64` passed.
- `MSBuild NipsEngine.sln /p:Configuration=GameClientRelease /p:Platform=x64` passed.

Follow-up:

- Manual visible QA is still needed for PlayerStart possession, camera view, and movement input.
- Missing-startup-scene handling should be surfaced through a package log file or visible message.

### 2026-05-02 Runtime Package Logging Added

Purpose:

- Make packaged GameClient startup failures readable without attaching a debugger.

Result:

- `FLog` now supports an optional file output path.
- `UGameEngine` initializes runtime logging to `Saves/Logs/Game.log`.
- GameClient startup writes `[GameEngine] GameClient boot started.`
- Missing startup scene now writes a readable message and falls back to an empty game world.

Verified:

- `MSBuild NipsEngine.sln /p:Configuration=Debug /p:Platform=x64` passed.
- `MSBuild NipsEngine.sln /p:Configuration=GameClientRelease /p:Platform=x64` passed.
- Packaged Shipping launch creates `Saves/Logs/Game.log`.
- `Game.log` contains `Loaded startup scene` for the default scene.
- With `StartupScene=Asset/Scene/__Missing.scene`, `Game.log` contains:
  - `Startup scene missing`
  - `Created empty game world`

### 2026-05-02 Batch 8 - GameClient Input Bootstrap and Game Source Boundary

Purpose:

- Fix packaged GameClient not clearly receiving movement/look input.
- Keep game-specific code separable for future project/module extraction.

Result:

- `UGameEngine` now explicitly keeps the game window focused/captured when the standalone GameClient is active.
- GameClient hides the cursor, locks mouse sampling to the client rect center, and enables raw mouse input for PlayerController look.
- GameClient resizes the runtime PlayerController camera when the window is resized.
- Added runtime logs for:
  - Game input capture
  - PlayerController spawn
  - Runtime camera attachment
  - Default pawn spawn
  - First key/mouse input received
- Fixed `UCameraComponent::GetForwardVector()` and `GetRightVector()` degree/radian conversion.
  - This could make game movement direction feel broken even when keys were being received.
- Moved game-specific sample controller from engine framework into `Source/Game`:
  - `Source/Game/GameJamPlayerController.h`
  - `Source/Game/GameJamPlayerController.cpp`
- Removed the direct `UGameEngine -> AGameJamPlayerController` include/dependency.
- Added `UWorld::SpawnActorByTypeName()` so runtime boot can instantiate game classes through the existing object factory.
- `Game.ini` now supports:
  - `PlayerControllerClass=AGameJamPlayerController`
- The Editor Build Game modal exposes `Player Controller` and the packager writes it into `Settings/Game.ini`.

Structure note:

- `UGameEngine` intentionally stays in `Source/Engine/Runtime`.
  - It is the editorless runtime shell: load settings, load startup scene, own the runtime world loop, and route input to the active game controller.
  - It should not contain project-specific controller logic.
- `AGameJamPlayerController` intentionally lives in `Source/Game`.
  - It is game/project policy: pawn spawn behavior, camera response, and movement mapping.
- Runtime game-class selection is now string/factory based.
  - This is a lightweight stand-in for a future project module setting.
- PIE and Standalone are intentionally separate runtime modes:
  - PIE runs inside `UEditorEngine` in the Editor process.
  - PIE duplicates the Editor world into a PIE world, keeps editor-only eject/selection/viewport behavior available, and forwards possessed input to a game `APlayerController`.
  - Standalone/GameClient runs `UGameEngine` from `NipsGame.exe`, loads `Settings/Game.ini`, and owns its own window/input/render loop.
- PIE should share game classes with Standalone, not become the packaged GameClient executable.
- This layout keeps future project separation straightforward:
  - Engine/runtime shell remains engine-owned.
  - Game-specific controller/pawn/rules can later move into a project/game module with minimal engine dependency churn.

Verified:

- `MSBuild NipsEngine.sln /m /p:Configuration=Debug /p:Platform=x64 /v:minimal` passed.
- `MSBuild NipsEngine.sln /p:Configuration=GameClientDebug /p:Platform=x64 /v:minimal` passed.
- `MSBuild NipsEngine.sln /p:Configuration=GameClientRelease /p:Platform=x64 /v:minimal` passed.
- Hidden GameClientRelease smoke launch from `NipsEngine/Bin/GameClientRelease/NipsGame.exe` wrote:
  - `Loaded startup scene`
  - `Spawned PlayerController class: AGameJamPlayerController`
  - `Runtime camera attached to game world`
  - `Spawned default pawn`
  - `Game input captured`
  - `First mouse input received`

Follow-up:

- Manual visible QA is still needed to confirm:
  - window captures input immediately on launch
  - WASD/E/Q moves the default pawn
  - mouse look updates the camera
  - the startup scene is visible from PlayerStart

### 2026-05-02 Batch 9 - Packaging Build Logs in Console

Purpose:

- Make Editor `Build Game...` failures inspectable from the in-editor Console instead of only a footer toast.
- Diagnose `NipsEngine.sln not found` during packaging.

Result:

- `FGamePackager` now emits `[Build] ...` logs through `UE_LOG`, so they appear in the Editor Console.
- MSBuild stdout/stderr is captured through a pipe and forwarded line-by-line to the Console.
- Packaging logs now include:
  - MSBuild start
  - resolved solution path
  - selected GameClient configuration
  - MSBuild output
  - package output path
  - copied exe/pdb/assets/shaders/lua steps
  - `Settings/Game.ini` write
  - final `Build Complete`
- Console renders `[Build]` lines in orange so build output is easier to scan.
- Solution discovery no longer depends only on `FPaths::RootDir().parent_path()`.
  - It walks upward from `FPaths::RootDir()`, current working directory, and the running module directory until it finds `NipsEngine.sln`.
- If the solution is still missing, the Console logs `FPaths::RootDir` and current path for quick diagnosis.

Verified:

- `MSBuild NipsEngine.sln /m /p:Configuration=Debug /p:Platform=x64 /v:minimal` passed.

### 2026-05-02 Batch 10 - Startup Scene Packaging Validation

Purpose:

- Prevent GameClient packages that launch but have no valid player spawn point.

Result:

- `FGamePackager` validates the selected `StartupScene` before running MSBuild.
- Packaging now requires exactly one `APlayerStart` in the startup scene.
- If validation fails, packaging stops before compile and emits orange Console logs:
  - validated scene path
  - Player Start count
  - `Build Failed`
  - instruction to add/save Player Start

Validation rule:

- `PlayerStartCount == 1`

Follow-up:

- `MSBuild NipsEngine.sln /m /p:Configuration=Debug /p:Platform=x64 /v:minimal` compiled `GamePackager.cpp`, then failed at link because `NipsEngine/Bin/Debug/NipsEngine.exe` was open/running.
- Close the Editor or stop `NipsEngine.exe`, then rerun Debug build for final link verification.

### 2026-05-02 Batch 11 - Empty Texture Load Noise Cleanup

Purpose:

- Remove the startup log flood from optional/empty material texture slots.

Finding:

- Many `.mat` / `.matinst` files serialize texture parameters with an empty value.
- Count found in current assets: 710 occurrences of `"Type": "Texture"` with `"Value": ""`.
- `FResourceManager::DeserializeMaterial()` passed those empty strings to `LoadTexture("")`.
- `UTexture::LoadFromFile("")` then logged `Failed to load texture:` for every optional empty texture slot.

Result:

- `FResourceManager::LoadTexture()` now treats an empty normalized path as `nullptr` without logging.
- Failed non-empty texture loads now destroy the temporary `UTexture` object instead of leaking it.
- `UTexture::LoadFromFile()` now returns false immediately for empty path or null D3D device.
- Actual missing texture paths still log normally.

Remaining meaningful warnings:

- Non-empty paths such as missing `water.png`, malformed Lumine texture path, or missing cottage texture are real asset/path issues and should remain visible.

Verified:

- `MSBuild NipsEngine.sln /m /p:Configuration=GameClientRelease /p:Platform=x64 /v:minimal` passed.

### 2026-05-02 Batch 12 - GameClient Runtime Render Target Fix

Purpose:

- Diagnose why the packaged/runtime GameClient showed only clear color even though the startup scene loaded without obvious errors.

Finding:

- Runtime logs proved the scene and camera were valid:
  - `Actors=10`
  - `VisiblePrimitives=15`
  - `OpaqueCommands=5`
  - camera at `(-3.00, 0.00, 1.60)` facing `+X`
- The failure point was not scene loading, culling, or player camera setup.
- `FDefaultRenderPipeline` rendered directly against the swapchain backbuffer target.
- The render flow now expects the same intermediate target set used by Editor viewports:
  - Scene color
  - Normal
  - Light
  - Fog
  - World position
  - FXAA
  - Depth SRV/DSV
- The backbuffer target only supplied color + depth. Later passes such as Light/Fog/FXAA therefore had missing/null intermediate RTV/SRV inputs, so the final visible output could remain clear color.

Result:

- Added a dedicated game-frame render target resource in `FRenderer`.
- Added `FRenderer::BeginGameFrame()`:
  - allocates/resizes the complete intermediate target set for the game window
  - clears and binds it through the existing viewport render path
- Added `FRenderer::CompositeCurrentSceneToBackBuffer()`:
  - draws the final scene SRV to the swapchain backbuffer with the existing fullscreen FXAA shader in copy mode
- Updated `FDefaultRenderPipeline` to:
  - set viewport size/origin in the runtime render bus
  - render into the game-frame target set
  - composite to the backbuffer before present
- Kept Editor viewport rendering unchanged.

Verified:

- `MSBuild NipsEngine.sln /m /p:Configuration=GameClientRelease /p:Platform=x64 /v:minimal` passed.
- Hidden `NipsGame.exe` smoke run wrote:
  - `[GameRender] First collect. Actors=10 VisiblePrimitives=15 ... OpaqueCommands=5`
  - `[GameRender] Game frame targets ready. Size=1904x1041 Color=1 Light=1 Fog=1 FXAA=1 DepthSRV=1`

Manual QA:

- Re-run Editor `Build Game...` so the package copies the newly built `NipsGame.exe`.
- Launch the packaged client and confirm:
  - scene is visible instead of clear color
  - WASD/E/Q movement is visible
  - mouse look updates the player camera

### 2026-05-02 Batch 14 - PIE vs Standalone Boundary Clarification

Purpose:

- Clarify that PIE is not the same thing as packaged Standalone/GameClient.
- Reduce the risk of editor PIE code drifting away from Standalone game-class selection.

Architecture decision:

- PIE remains Editor-owned:
  - `UEditorEngine` duplicates the current Editor world into a PIE world.
  - Editor viewport, PIE toolbar, eject, selection, and editor control still belong to the Editor.
  - Possessed PIE input is forwarded to a game `APlayerController`.
- Standalone remains GameClient-owned:
  - `UGameEngine` runs in `NipsGame.exe`.
  - It loads `Settings/Game.ini`, spawns the configured player controller, captures standalone input, and renders directly to the game window.
- Shared part:
  - both modes use game-side controller classes such as `AGameJamPlayerController`.

Result:

- `UEditorEngine::StartPlaySessionNow()` no longer directly includes or templates on `AGameJamPlayerController`.
- PIE now spawns the default game controller through `UWorld::SpawnActorByTypeName("AGameJamPlayerController")` and casts it to `APlayerController`.
- This keeps PIE closer to Standalone's factory-based controller path while preserving Editor-owned PIE behavior.

Verified:

- Packaged visible QA passed:
  - scene visible
  - WASD/E/Q input works
  - mouse look works
- `MSBuild NipsEngine.sln /m /p:Configuration=Debug /p:Platform=x64 /v:minimal` passed.
- `MSBuild NipsEngine.sln /p:Configuration=GameClientDebug /p:Platform=x64 /v:minimal` passed.
- `MSBuild NipsEngine.sln /m /p:Configuration=GameClientRelease /p:Platform=x64 /v:minimal` passed.

Manual QA:

- Re-test PIE:
  - Play enters possessed game input
  - F8 eject returns to PIE editor control
  - Shift+F1 only releases mouse cursor

### 2026-05-02 Batch 15 - Packaging Menu and Settings UX

Purpose:

- Rename the user-facing standalone build flow to `Packaging`.
- Make the settings modal clearer before further build-setting expansion.

Result:

- File menu item renamed:
  - `Build Game...` -> `Packaging...`
- Popup renamed:
  - `Build Game` -> `Packaging Settings`
- Packaging settings UI now groups fields into:
  - game/package identity
  - startup scene
  - player controller
  - output directory
  - configuration
  - options
  - validation
- Configuration labels now show their actual target mapping:
  - `Development (GameClientDebug)`
  - `Shipping (GameClientRelease)`
- Action button renamed:
  - `Build` -> `Package`
- Footer/status text now uses packaging terminology:
  - `Packaging game...`
  - `Packaging already in progress`
- Validation area now displays:
  - selected configuration mapping
  - output exe name
  - startup scene existence
  - empty game/controller/output errors

Verified:

- `MSBuild NipsEngine.sln /p:Configuration=GameClientDebug /p:Platform=x64 /v:minimal` passed.
- `MSBuild NipsEngine.sln /m /p:Configuration=GameClientRelease /p:Platform=x64 /v:minimal` passed.
- `MSBuild NipsEngine.sln /m /p:Configuration=Debug /p:Platform=x64 /v:minimal` compiled changed files, then link failed because `NipsEngine/Bin/Debug/NipsEngine.exe` was already running.

Next setting expansion candidates:

- package version/build number
- output folder browse button
- open output folder after packaging
- include/exclude debug symbols
- include/exclude LuaScript
- include/exclude full Asset folder vs selected StartupScene dependencies
- additional startup validation report

### 2026-05-02 Batch 16 - Build Menu and Scene Copy List

Purpose:

- Move packaging out of the File menu into a dedicated Build menu.
- Add an explicit scene copy list to the Packaging Settings panel.

Result:

- Main menu now contains:
  - `File`
  - `Edit`
  - `Build`
  - `View`
  - `Settings`
  - `Help`
- `Packaging...` now lives under `Build`.
- `File` is back to scene/file/asset folder actions only.
- Packaging Settings panel width increased.
- Added `Scenes to Copy` section:
  - startup scene is always included
  - browse/add arbitrary scene files
  - add current startup scene button
  - remove scenes from the list
  - scene count shown in validation
- `FGameBuildSettings` now carries `IncludedScenes`.
- `FGamePackager` now:
  - validates the included scene list
  - explicitly copies listed scenes into the package path
  - logs each copied scene
  - writes `[Scenes]` to `Settings/Game.ini`

Note:

- The current packager still copies the whole `Asset/` directory.
- The scene list is already useful as validation/metadata, and it is the path toward future selective scene/asset cooking.

Verified:

- `MSBuild NipsEngine.sln /m /p:Configuration=Debug /p:Platform=x64 /v:minimal` passed.
- `MSBuild NipsEngine.sln /p:Configuration=GameClientDebug /p:Platform=x64 /v:minimal` passed.
- `MSBuild NipsEngine.sln /m /p:Configuration=GameClientRelease /p:Platform=x64 /v:minimal` passed.

### 2026-05-02 Batch 13 - Packaging Polish and Async Build UX

Purpose:

- Close the remaining packaging UX item from the plan:
  - `FGamePackager` previously ran synchronously from the Build Game modal.
  - This made the Editor look frozen while MSBuild was running.

Result:

- `FEditorMainPanel` now starts packaging through a `std::async` task.
- The Build Game modal closes immediately after starting the build.
- Footer log reports:
  - `Building game...` at start
  - final success/failure message when the worker finishes
- Console still receives live `[Build]` MSBuild output while the build is running.
- Build requests are guarded while one package task is already in progress.
- `FEditorConsoleWidget` log storage is now protected by a mutex so background build logs do not race against ImGui rendering.
- `FLog` now snapshots the sink/file path under a mutex before dispatching log output.

Verified:

- `MSBuild NipsEngine.sln /m /p:Configuration=Debug /p:Platform=x64 /v:minimal` passed.
- `MSBuild NipsEngine.sln /p:Configuration=GameClientDebug /p:Platform=x64 /v:minimal` passed.
- `MSBuild NipsEngine.sln /m /p:Configuration=GameClientRelease /p:Platform=x64 /v:minimal` passed.

Manual QA:

- Open Editor `Build Game...` and confirm:
  - modal closes immediately after pressing Build
  - Console receives orange `[Build]` output during MSBuild
  - Footer displays final build result

### 2026-05-02 Batch 17 - Splash Overlay Window

Purpose:

- Replace the first splash implementation that created a separate image-sized popup before the game window.
- Make splash behave like a loading cover over the real GameClient window.

Result:

- GameClient now creates its main window first, then shows the splash overlay on top of that window.
- The splash overlay uses the target game window outer rect, including the title bar area.
- The overlay no longer paints a black background; it is a layered transparent cover that draws only the splash image.
- Fade-in/out applies to the logo image only.
- The overlay uses `SW_SHOWNOACTIVATE` / `WS_EX_NOACTIVATE` so it does not steal input focus from the game window.
- Splash seconds now defaults to 3 seconds and the Packaging UI clamps it to the supported 3-10 second range.

Manual QA:

- Package with a PNG splash image.
- Run the GameClient and confirm:
  - the game window appears first
  - splash covers the whole game window area
  - no black patch is painted behind the splash
  - the logo is centered and not stretched across the whole window
  - splash closes after at least the configured minimum seconds

Verified:

- `MSBuild NipsEngine.sln /m /p:Configuration=Debug /p:Platform=x64 /v:minimal` passed.
- `MSBuild NipsEngine.sln /p:Configuration=GameClientDebug /p:Platform=x64 /v:minimal` passed.
- `MSBuild NipsEngine.sln /m /p:Configuration=GameClientRelease /p:Platform=x64 /v:minimal` passed.
