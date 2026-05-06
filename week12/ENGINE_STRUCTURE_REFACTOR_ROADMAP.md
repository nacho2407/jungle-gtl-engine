# Engine Structure Refactor Roadmap

Updated: 2026-05-07

This document is the working roadmap for the NipsEngine structure refactor.

The goal is not to split files for its own sake. The goal is that a new maintainer can read the code and quickly understand:

- what owns each responsibility
- which APIs are stable and public
- which code is Editor-only
- which code must run in GameClient
- how PIE differs from both Editor and GameClient
- where to add a feature without causing hidden side effects

All batches must preserve current behavior unless a behavior change is explicitly approved.

## Non-Negotiable Rules

1. Preserve existing behavior.
2. Do not make broad rewrites without a small batch boundary.
3. Do not leave duplicate legacy APIs after moving responsibility.
4. Keep external APIs intentional and narrow.
5. Keep function signatures readable.
6. Keep class responsibilities understandable from the class name.
7. Prefer named functions/classes over policy hidden in local lambdas.
8. Do not increase Engine -> Editor dependency.
9. Verify Editor and GameClient builds after structural changes that touch Engine or Runtime paths.
10. Document the touched responsibility before moving code.

## Evaluation Criteria

### Readability

Good:

- public methods are grouped by responsibility
- method names describe policy
- large UI surfaces are split by user-facing panel
- helper functions have names when they encode real policy
- function signatures wrap cleanly when long

Bad:

- a class owns UI drawing, scene mutation, packaging, runtime UI, and input policy at the same time
- a public method exists only because an old caller used it before a subsystem was extracted
- anonymous namespace helpers become a second hidden class

### Dependency

Final desired direction:

- `Engine` does not include `Editor`
- `GameClient` does not compile or include `Editor`
- `Editor` can depend on `Engine`
- `PIE` is owned by `Editor`, but shares Runtime behavior where possible
- `Lua` and `RML` do not bypass the input policy layer
- `Renderer` runtime core does not directly depend on editor viewport types

### Learning Cost

Target:

- A feature should be understandable by reading 2-3 local files.
- Cross-domain flows should have a named bridge or session object.
- A new maintainer should not need to know every panel and every viewport mode to change one feature.

### Structural Fit

The final structure should feel UE-like where it helps:

- `Engine` is Runtime core.
- `Editor` is a tool layer over Runtime.
- `PIE` is an Editor-owned Runtime session.
- `GameClient` is standalone Runtime.
- `GameMode` owns gameplay boot policy.
- `PlayerController` owns gameplay input interpretation.
- `PlayerCameraManager` owns runtime camera output.
- `EditorViewportClient` owns editor viewport coordination, not all interactions.

## Current High-Risk Areas

### EditorMainPanel

Current state:

- The old monolithic `EditorMainPanel.cpp` has been removed.
- PIE presentation, packaging, scene commands, placement, viewport toolbar, debug UI, undo history UI, console/footer commands, input capture policy, dock layout, viewport host composition, icon loading, bootstrap, style setup, widget setup, and the main editor toolbar have been split into focused files.
- `FEditorMainPanel` still owns the shell-level widget instances and grouped UI state, but those state types now live in `EditorMainPanelState.h` while the extracted panel files are being stabilized.

Final target:

- `FEditorMainPanel` becomes an Editor shell coordinator.
- It initializes ImGui, owns the main dock host, and calls smaller panels in order.
- It does not own detailed packaging state, viewport toolbar icons, RML draw callback queues, scene dirty policy, or undo history drawing.

Planned split:

- `Editor/UI/MainPanel/EditorMainPanel`
- `Editor/UI/MainPanel/EditorDockLayout`
- `Editor/UI/MainPanel/EditorWindowState`
- `Editor/UI/Packaging/EditorPackagingPanel`
- `Editor/UI/ViewportPanel/EditorViewportPanel`
- `Editor/UI/ViewportPanel/EditorViewportToolbar`
- `Editor/UI/ViewportPanel/EditorViewportContextMenu`
- `Editor/UI/ViewportPanel/EditorViewportIconSet`
- `Editor/UI/RuntimeUIPreview/RuntimeUIPreviewRenderBridge`
- `Editor/UI/Debug/EditorDebugPanel`
- `Editor/UI/Undo/EditorUndoHistoryPanel`
- `Editor/UI/Footer/EditorFooterOverlay`

### EditorEngine

Current problem:

- `UEditorEngine` is still a coordinator for many systems.
- Some public APIs are real Editor commands.
- Some APIs are legacy wrappers left after extracting a system.

Final target:

- `UEditorEngine` owns top-level Editor subsystems.
- It exposes only deliberate cross-system entry points.
- Extracted systems expose their own API through getters, not duplicated wrappers.

Current API rule:

- Undo/Redo operations belong to `FEditorUndoSystem`.
- PIE state belongs to `FPIESession`.
- Runtime UI belongs to `FRmlUiSystem`.
- Scene document operations should move to an Editor scene document/controller.

### EditorViewportClient

Current problem:

- It coordinates editor camera, selection commands, gizmo manipulation, transform shortcuts, PIE possess/eject, cursor focus, and game input bridge state.
- Point picking and drag-box selection policy have been moved to focused services.

Final target:

- `FEditorViewportClient` remains a viewport coordinator.
- Interaction policies move into smaller services:
  - `EditorPickingService`
  - `EditorBoxSelectionService`
  - `EditorTransformInteraction`
  - `EditorViewportNavigationController`
  - `PIEViewportControlBridge`

### ResourceManager

Current state:

- `FResourceManager` is now a transition facade over focused caches/policies.
- Extracted ownership now includes:
  - `TextureResourceCache`
  - `ShaderResourceCache`
  - `MaterialResourceCache`
  - `RenderStateResourceCache`
  - `StaticMeshResourceCache`
  - `AtlasResourceCache`
  - `CurveResourceCache`
  - `AssetPathPolicy`
  - `ImportedMaterialPolicy`
  - `StaticMeshLoadService`
  - `MaterialLoadService`
  - `MaterialSerializationService`
  - `ResourceMemoryReporter`
- `FResourceManager` still coordinates a few resource initialization and discovery entry points while the focused caches/services own the extracted resource behavior.

Final target:

- Keep `FResourceManager` as a facade only during transition.
- Split actual responsibilities:
  - `AssetPathPolicy`
  - `TextureResourceCache`
  - `ShaderResourceCache`
  - `MaterialResourceCache`
  - `StaticMeshResourceCache`
  - `ImportedMaterialService`
  - `MaterialLoadService`
  - `StaticMeshLoadService`
  - `ResourceMemoryReporter`
  - `CookedAssetCopyPolicy`

### Render Pipeline

Current state:

- Engine-facing editor include violations in renderer paths are currently cleared by the architecture check.
- Shadow filter/editor setting reads were moved behind runtime-facing render state flow.
- `RenderCollector` is now mostly a world traversal, culling, and render collection coordinator. Primitive draw command building, decal command building, light data collection, and editor overlay collection have been split into focused render-scene helpers.
- `Renderer` and render passes still need a later proxy/frame/draw-command architecture pass.
- Main-merged PostProcess pass code compiled in the current Editor and GameClient debug verification. The only merge issue found there was the shader project item being classified as C++ compile input, which has been corrected.

Final target:

- Move toward:
  - `PrimitiveSceneProxy`
  - `SceneRenderState`
  - `FrameContext`
  - `RenderCollector`
  - `DrawCommandBuilder`
  - `Renderer`

Desired flow:

```text
Component -> PrimitiveSceneProxy -> SceneRenderState -> RenderCollector
          -> DrawCommandBuilder -> DrawCommandList -> Renderer -> Passes
```

Editor-only rendering:

- ID Picking, SelectionMask, Outline, Grid, Gizmo, DebugLine should start from the Editor pipeline or an Editor render extension.
- GameClient should not need editor viewport types.

## Final Target Domain Layout

```text
NipsEngine/Source
  Engine/
    Core/
    Object/
    Math/
    Platform/
    Runtime/
      Engine.h
      GameEngine.h
      EngineLoop.h
    GameFramework/
      World.h
      Level.h
      AActor.h
      GameModeBase.h
      PlayerController.h
    Camera/
      PlayerCameraManager.h
      CameraModifier.h
      ViewportCamera.h
    Component/
    Input/
      InputSystem.h
      InputTypes.h
      InputPolicyTypes.h
    Render/
      Device/
      Resource/
      Scene/
      Pipeline/
      Pass/
    Asset/
    UI/
      RmlUi/
    Serialization/
    Runtime/Script/

  Editor/
    EditorEngine.h
    Input/
    PIE/
    Viewport/
    Selection/
    Undo/
    Scene/
    UI/
    Packaging/

  Game/
    Project-specific C++ gameplay when needed

  Launch/
    LaunchModeFactory.h
    LaunchModeFactory.cpp

  Misc/
    ObjViewer/
```

## Domain Boundaries

### Engine

Allowed:

- UObject, Actor, Component
- World, Level, GameMode, PlayerController
- runtime input sampling primitives
- renderer runtime core
- runtime assets and resource caches
- Lua binding
- RML runtime UI system
- runtime serialization
- platform/window abstraction

Forbidden:

- ImGui panels
- editor selection policy
- editor viewport layout
- editor settings direct dependency
- editor-only picking tools
- packaging UI

### Editor

Allowed:

- ImGui panels
- editor viewport layout
- editor-only selection/details
- undo/redo system
- content browser
- scene document state
- packaging orchestration
- PIE session ownership

Forbidden:

- GameClient boot policy
- runtime asset cache ownership
- renderer runtime internals that should be mode-independent

### PIE

PIE is an Editor-owned Runtime session.

Allowed:

- active PIE world handle
- active viewport index
- possessed/editor-control mode
- player controller pointer
- mouse focus release state
- shell commands: Esc, F8, Shift+F1
- runtime UI viewport mapping for PIE

Policy:

- Possessed: Game input and Runtime UI input are active.
- Eject / EditorControl: Editor viewport settings are respected.
- Eject does not render/capture Runtime RML UI.

### GameClient

GameClient is standalone Runtime.

Allowed:

- GameEngine boot
- GameMode selection
- PlayerController routing
- Runtime RML UI
- Lua gameplay script
- packaged asset loading

Forbidden:

- Editor include
- ImGui UI
- editor settings
- editor selection and picking tools

## External API Policy

An external API is allowed only when the owner class truly owns the behavior.

Rules:

- If a subsystem owns a behavior, callers use that subsystem.
- Do not keep wrapper methods on `UEditorEngine` after extracting the subsystem.
- Public getters are acceptable when they expose a subsystem owner.
- Public mutable subsystem access should be reviewed. If external code should not mutate it, expose a command method or const view instead.
- Methods that exist only for one panel should move toward the panel/controller that owns that workflow.

Examples:

- Good: `EditorEngine->GetUndoSystem().Undo()`
- Bad: `EditorEngine->Undo()`
- Good: `EditorEngine->GetPIESession().GetControlMode()`
- Bad: `EditorEngine->GetPIEControlMode()`
- Good: `GEngine->GetRmlUiSystem().Render(Context, Renderer)`
- Bad: RML document state spread directly across EditorEngine and GameEngine

## Batch Plan

## Current Progress

Overall progress: about 94% for the active Engine structure refactor scope.

Current synced state:

- Engine -> Editor include boundary: current guard baseline is clean.
- GameClient Editor source exclusion guard: current guard baseline is clean.
- `EditorMainPanel` monolith removal/split: structurally complete for this scope; remaining work is stabilization or moving extracted files into standalone panel classes.
- `LaunchModeFactory` split: complete for the current EngineLoop boundary scope.
- `ResourceManager` facade split: storage/cache/policy/service extraction is mostly complete. Asset discovery registration, static mesh object creation/LOD policy, default resource initialization, static mesh OBJ/BIN load orchestration, material OBJ/MTL load orchestration, material serialization/deserialization, material memory reporting, and texture asset meta load/create policy now have named focused steps/services.
- Render proxy/collector architecture cleanup: started. Primitive draw command building, decal command building, light collection, and editor overlay collection are split from `RenderCollector`; proxy/frame cleanup remains.
- `EditorViewportClient` interaction split: structurally complete for this scope. Point picking, box selection, transform/gizmo mode policy, PIE control bridge behavior, camera focus, free-orbit/pan/dolly navigation, keyboard navigation routing, move-speed wheel policy, reset-camera policy, and viewport camera-mode policy are now split into focused services/controllers.

Deferred after this batch scope:

- Render collector/proxy architecture cleanup remains partially deferred. Proxy/frame architecture remains after the collector responsibility split.

Completed:

- Batch 1: Guard Rails and API Baseline
- Batch 2: EditorEngine API Surface, first safe pass
- Batch 3: EditorMainPanel First Split, PIE presentation pass
- Batch 4: EditorMainPanel Packaging Split
- Batch 5: EditorMainPanel Scene Command Split
- Batch 6: EditorMainPanel Placement and Viewport Context Menu Split
- Batch 7: EditorMainPanel Viewport Toolbar and Menu Split
- Batch 8: EditorMainPanel Debug and Undo History Split
- Batch 9: EditorMainPanel Console Drawer and Content Command Split
- Batch 10: EditorMainPanel Input Capture Policy Split
- Batch 11: EditorMainPanel Main Toolbar Split
- Batch 12: EditorMainPanel Layout and Material Command Split
- Batch 13: EditorMainPanel Viewport Icon Resource Split
- Batch 14: EditorMainPanel Viewport Host Split
- Batch 15: EditorMainPanel Footer Overlay Split
- Batch 16: EditorMainPanel Header State Grouping
- Batch 17: EditorMainPanel PIE Viewport State Grouping
- Batch 18: EditorMainPanel Panel Visibility State Grouping
- Batch 19: EditorMainPanel Runtime UI Draw Callback State Grouping
- Batch 20: EditorMainPanel Runtime UI Bridge File Split
- Batch 21: EditorMainPanel Style and Font Setup Split
- Batch 22: EditorMainPanel Font Merge Setup Completion
- Batch 23: EditorMainPanel Create Flow Naming
- Batch 24: EditorMainPanel Render Flow Naming
- Batch 25: EditorMainPanel Frame File Split
- Batch 26: EditorMainPanel Widget Setup File Split
- Batch 27: EditorMainPanel Bootstrap File Split
- Batch 28: EditorMainPanel Release Bootstrap Move
- Batch 29: EditorMainPanel Shell Removal and Header Inline Cleanup
- Batch 30: EditorMainPanel Header API Cleanup
- Batch 31: EditorMainPanel State Type Extraction
- Batch 32: EditorMainPanel Viewport Icon State Extraction
- Batch 33: EditorMainPanel Widget Set Grouping
- Batch 34: EditorMainPanel Viewport Toolbar Helper Extraction
- Batch 35: EditorMainPanel Viewport Menu Bar Split
- Batch 36: EditorMainPanel Viewport Context Menu Split
- Batch 37: EditorMainPanel Packaging Helper Extraction
- Batch 38: EditorMainPanel Viewport Button Drawing Split
- Batch 39: Engine-facing Editor dependency cleanup, first pass
- Batch 40: Launch Mode Factory Split
- Batch 41: GameClient Editor Source Exclusion Guard
- Batch 42: Texture Resource Cache Extraction
- Batch 43: Shader Resource Cache Extraction
- Batch 44: Material Resource Cache Storage Extraction
- Batch 45: Render State Resource Cache Extraction
- Batch 46: Asset Path Policy Extraction
- Batch 47: Static Mesh Resource Cache Storage Extraction
- Batch 48: Atlas Resource Cache Extraction
- Batch 49: Imported Material Policy Extraction
- Batch 50: Curve Resource Cache Extraction
- Batch 51: Asset Path Classification Policy Extraction
- Batch 52: Main Merge Build Sync and PostProcess Shader Project Item Fix
- Batch 53: ResourceManager Asset Discovery Registration Cleanup
- Batch 54: ResourceManager StaticMesh Loaded Object Creation Helper
- Batch 55: ResourceManager Default Resource Initialization Steps
- Batch 56: StaticMesh Load Service Extraction
- Batch 57: Material Load Service Extraction
- Batch 58: Resource Memory Reporter Extraction
- Batch 59: Material Serialization Service Extraction
- Batch 60: Editor Picking Service Extraction
- Batch 61: Editor Box Selection Service Extraction
- Batch 62: Editor Transform Interaction Extraction
- Batch 63: Texture Asset Meta Service Extraction
- Batch 64: Editor Viewport PIE Controller Extraction
- Batch 65: Editor Viewport Primary Selection Focus Extraction
- Batch 66: Editor Viewport Navigation Controller Completion
- Batch 67: RenderCollector Primitive and Light Split
- Batch 68: RenderCollector Decal Command Split
- Batch 69: RenderCollector Editor Overlay Split

Verified:

- `Scripts/CheckArchitecture.ps1`
- `Debug|x64` Editor build
- Batch 11 `Debug|x64` Editor build
- Batch 12 `Debug|x64` Editor build
- Batch 13 `Debug|x64` Editor build
- Batch 14 `Debug|x64` Editor build
- Batch 15 `Debug|x64` Editor build
- Batch 15 `GameClientDebug|x64` build
- Batch 16 `Debug|x64` Editor build
- Batch 17 `Debug|x64` Editor build
- Batch 18 `Debug|x64` Editor build
- Batch 19 `Debug|x64` Editor build
- Batch 20 `Debug|x64` Editor build
- Batch 20 `GameClientDebug|x64` build
- Batch 21 `Debug|x64` Editor build
- Batch 21 `GameClientDebug|x64` build
- Batch 22 `Debug|x64` Editor build
- Batch 22 `GameClientDebug|x64` build
- Batch 23 `Debug|x64` Editor build
- Batch 23 `GameClientDebug|x64` build
- Batch 24 `Debug|x64` Editor build
- Batch 24 `GameClientDebug|x64` build
- Batch 25 `Debug|x64` Editor build
- Batch 25 `GameClientDebug|x64` build
- Batch 26 `Debug|x64` Editor build
- Batch 26 `GameClientDebug|x64` build
- Batch 27 `Debug|x64` Editor build
- Batch 27 `GameClientDebug|x64` build
- Batch 28 `Debug|x64` Editor build
- Batch 28 `GameClientDebug|x64` build
- Batch 29 `Debug|x64` Editor build
- Batch 29 `GameClientDebug|x64` build
- Batch 30 `Debug|x64` Editor build
- Batch 30 `GameClientDebug|x64` build
- Batch 31 `Debug|x64` Editor build
- Batch 31 `GameClientDebug|x64` build
- Batch 32 `Debug|x64` Editor build
- Batch 32 `GameClientDebug|x64` build
- Batch 33 `Debug|x64` Editor build
- Batch 33 `GameClientDebug|x64` build
- Batch 34 `Debug|x64` Editor build
- Batch 35 `Debug|x64` Editor build
- Batch 35 `GameClientDebug|x64` build
- Batch 36 `Debug|x64` Editor build
- Batch 36 `GameClientDebug|x64` build
- Batch 37 `Debug|x64` Editor build
- Batch 37 `GameClientDebug|x64` build
- Batch 38 `Debug|x64` Editor build
- Batch 38 `GameClientDebug|x64` build
- Batch 39 `Scripts/CheckArchitecture.ps1`
- Batch 40 `Scripts/CheckArchitecture.ps1`
- Batch 41 `Scripts/CheckArchitecture.ps1`
- Batch 42 `Scripts/CheckArchitecture.ps1`
- Batch 43 `Scripts/CheckArchitecture.ps1`
- Batch 44 `Scripts/CheckArchitecture.ps1`
- Batch 45 `Scripts/CheckArchitecture.ps1`
- Batch 46 `Scripts/CheckArchitecture.ps1`
- Batch 47 `Scripts/CheckArchitecture.ps1`
- Batch 48 `Scripts/CheckArchitecture.ps1`
- Batch 49 `Scripts/CheckArchitecture.ps1`
- Batch 50 `Scripts/CheckArchitecture.ps1`
- Batch 51 `Scripts/CheckArchitecture.ps1`
- Batch 52 `Scripts/CheckArchitecture.ps1`
- Main merge sync `Debug|x64` Editor build
- Main merge sync `GameClientDebug|x64` build
- Final `Release|x64` Editor build
- Final `GameClientRelease|x64` build
- Batch 53-55 `Scripts/CheckArchitecture.ps1`
- Batch 53-54 `Debug|x64` Editor build
- Batch 53-54 `GameClientDebug|x64` build
- Batch 55 `Debug|x64` Editor build
- Batch 55 `GameClientDebug|x64` build
- Batch 56 `Scripts/CheckArchitecture.ps1`
- Batch 56 `Debug|x64` Editor build
- Batch 56 `GameClientDebug|x64` build
- Batch 57 `Scripts/CheckArchitecture.ps1`
- Batch 57 `Debug|x64` Editor build
- Batch 57 `GameClientDebug|x64` build
- Batch 58-59 `Scripts/CheckArchitecture.ps1`
- Batch 58-59 `Debug|x64` Editor build
- Batch 58-59 `GameClientDebug|x64` build
- Batch 60-61 `Scripts/CheckArchitecture.ps1`
- Batch 60-61 `Debug|x64` Editor build
- Batch 60-61 `GameClientDebug|x64` build
- Batch 62 `Scripts/CheckArchitecture.ps1`
- Batch 62 `Debug|x64` Editor build
- Batch 62 `GameClientDebug|x64` build
- Batch 63 `Scripts/CheckArchitecture.ps1`
- Batch 63 `Debug|x64` Editor build
- Batch 63 `GameClientDebug|x64` build
- Batch 64 `Scripts/CheckArchitecture.ps1`
- Batch 64 `Debug|x64` Editor build
- Batch 64 `GameClientDebug|x64` build
- Batch 65 `Scripts/CheckArchitecture.ps1`
- Batch 65 `Debug|x64` Editor build
- Batch 65 `GameClientDebug|x64` build
- Batch 66 `Scripts/CheckArchitecture.ps1`
- Batch 66 `Debug|x64` Editor build
- Batch 66 `GameClientDebug|x64` build
- Batch 67 `Scripts/CheckArchitecture.ps1`
- Batch 68-69 `Scripts/CheckArchitecture.ps1`

Current architecture check baseline:

- Engine -> Editor include violations: 0
- Legacy `UEditorEngine` undo wrappers: 0

Notes:

- Batch 2 did not change behavior.
- Batch 2 only moved small inline `UEditorEngine` implementation details from the header into `EditorEngine.cpp` and cleaned public signature layout.
- `FPIESession`, `FEditorUndoSystem`, and `FRmlUiSystem` remain exposed through owning system getters instead of duplicated legacy wrappers.
- Batch 3 did not change behavior.
- Batch 3 moved PIE viewport fullscreen, fixed-layout Runtime UI overlay, and RML draw callback implementation from `EditorMainPanel.cpp` to `EditorMainPanelPIE.cpp`.
- Batch 4 did not change behavior.
- Batch 4 moved packaging modal, packaging build-task polling, and packaging-only helper functions from `EditorMainPanel.cpp` to `EditorMainPanelPackaging.cpp`.
- Batch 5 did not change behavior.
- Batch 5 moved scene command entry points from `EditorMainPanel.cpp` to `EditorMainPanelScene.cpp`: close prompt, new scene, load scene, save scene, save-as, and last-scene restore.
- Batch 6 did not change behavior.
- Batch 6 moved Content Browser viewport drops, prefab/static mesh placement, placement-location calculation, and viewport right-click context menu handling from `EditorMainPanel.cpp` to `EditorMainPanelPlacement.cpp`.
- Batch 7 did not change behavior.
- Batch 7 moved viewport icon toolbar rendering, viewport menu bar rendering, and shared viewport button drawing from `EditorMainPanel.cpp` to `EditorMainPanelViewportToolbar.cpp`.
- Batch 7 kept the Debug panel camera-speed helpers in `EditorMainPanel.cpp` to avoid broad API movement in this pass.
- Batch 8 did not change behavior.
- Batch 8 moved Editor Debug and Undo History windows from `EditorMainPanel.cpp` to `EditorMainPanelDebug.cpp`.
- Batch 8 moved UndoSystem-facing debug UI out of the main panel body without changing the UndoSystem API.
- Batch 9 did not change behavior.
- Batch 9 moved console drawer rendering and Content Browser open/close/toggle commands from `EditorMainPanel.cpp` to `EditorMainPanelFooter.cpp`.
- Batch 9 deliberately left the footer status bar renderer in `EditorMainPanel.cpp` for a later pass because it contains existing non-ASCII UI labels that must be preserved exactly.
- Batch 10 did not change behavior.
- Batch 10 moved `FEditorMainPanel::Update`, the ImGui-to-engine input capture policy, viewport mouse focus allowance, PIE forced input focus countdown, focus-selection shortcut, and IME context synchronization to `EditorMainPanelInput.cpp`.
- Batch 11 did not change behavior.
- Batch 11 moved the top editor toolbar, PIE toolbar view-mode menu, fixed-layout toggle, and toolbar-only view-mode labels from `EditorMainPanel.cpp` to `EditorMainPanelToolbar.cpp`.
- Batch 11 brought `EditorMainPanel.cpp` below the 1000-line milestone.
- Batch 12 did not change behavior.
- Batch 12 moved dock host rendering to `EditorMainPanelLayout.cpp`, Material Editor open commands to `EditorMainPanelMaterial.cpp`, and PIE viewport focus request handling to `EditorMainPanelPIE.cpp`.
- Batch 13 did not change behavior.
- Batch 13 moved viewport toolbar icon and viewport layout icon loading/release to `EditorMainPanelViewportIcons.cpp`, keeping WIC/D3D icon resource handling out of the main panel body.
- Batch 14 did not change behavior.
- Batch 14 moved viewport host window rendering, scene color presentation, PIE runtime UI overlay placement, viewport focus outline drawing, splitter overlay placement, and viewport toolbar child placement to `EditorMainPanelViewportHost.cpp`.
- Batch 15 did not change behavior.
- Batch 15 moved footer status bar rendering, footer scene-path formatting, Hot Reload footer command, and PIE footer event logging to `EditorMainPanelFooter.cpp`.
- Batch 15 reduced `EditorMainPanel.cpp` to the shell loop: `Create`, `Release`, and `Render`.
- Batch 15 also cleaned stale `EditorMainPanel.cpp` includes after the responsibility split.
- Batch 16 did not change behavior.
- Batch 16 grouped the busiest `EditorMainPanel.h` private state into named structs: build-game modal state, console drawer state, viewport icon resources, debug grid state, and footer event state.
- Batch 16 keeps the state local to `FEditorMainPanel` for now, avoiding a wider ownership change before the extracted panels become standalone classes.
- Batch 17 did not change behavior.
- Batch 17 grouped PIE viewport presentation state into `FPIEViewportPresentationState`: fullscreen panel hiding, fixed-layout runtime UI size, forced PIE input focus frames, and saved PIE panel/layout snapshots.
- Batch 17 deliberately kept the state inside `FEditorMainPanel` because toolbar callbacks and PIE rendering still depend on the same owner during this transition.
- Batch 18 did not change behavior.
- Batch 18 grouped editor panel visibility flags into `FEditorPanelVisibilityState`, preserving the existing Toolbar bool-pointer API by pointing it at the grouped state fields.
- Batch 18 keeps PIE panel restore snapshots separate from live panel visibility so PIE restore behavior remains explicit.
- Batch 19 did not change behavior.
- Batch 19 grouped pending Runtime UI ImGui draw callbacks into `FRuntimeUIDrawCallbackState` and named their cleanup path as `ClearRuntimeUIDrawCallbacks()`.
- Batch 19 keeps Runtime UI Preview and PIE using the same callback queue, preserving their current draw ordering while making ownership clearer.
- Batch 20 did not change behavior.
- Batch 20 moved Runtime UI draw callback queue implementation from `EditorMainPanelPIE.cpp` to `EditorMainPanelRuntimeUI.cpp`.
- Batch 20 keeps PIE viewport Runtime UI and Runtime UI Preview on the same bridge, but PIE-specific presentation code is no longer mixed with the callback queue implementation.
- Batch 21 did not change behavior.
- Batch 21 moved ImGui style setup and primary editor font range setup into `EditorMainPanelStyle.cpp`, reducing `FEditorMainPanel::Create()` to a clearer initialization flow.
- Batch 22 did not change behavior.
- Batch 22 moved icon font merge and fallback font merge into `LoadEditorFonts()`, so `FEditorMainPanel::Create()` no longer owns font merge policy.
- Batch 22 also removed the stale encoded font comments from `Create()` after the policy moved into the named font setup function.
- Batch 23 did not change behavior.
- Batch 23 split `FEditorMainPanel::Create()` into named setup steps: ImGui context, project settings, ImGui backend, widget initialization, and widget callback binding.
- Batch 23 keeps the existing widget callback API intact while making the construction order easier to audit.
- Batch 24 did not change behavior.
- Batch 24 split `FEditorMainPanel::Render()` into named frame phases: ImGui frame begin/end, Content Browser shortcut sync, toolbar/dock, viewport, editor panels, console animation, and late overlays.
- Batch 24 preserves the previous render order while making the Editor shell's per-frame responsibility easier to audit before moving panels into standalone classes.
- Batch 25 did not change behavior.
- Batch 25 moved the named frame rendering phases from `EditorMainPanel.cpp` to `EditorMainPanelFrame.cpp`.
- Batch 25 keeps `EditorMainPanel.cpp` focused on creation, backend initialization, widget initialization, callback binding, and release.
- Batch 26 did not change behavior.
- Batch 26 moved editor widget initialization and widget callback binding from `EditorMainPanel.cpp` to `EditorMainPanelWidgetSetup.cpp`.
- Batch 26 keeps `Create()` as the visible construction flow while placing widget wiring in a focused Editor-only file.
- Batch 27 did not change behavior.
- Batch 27 moved ImGui context setup, project settings bootstrap, and ImGui backend initialization from `EditorMainPanel.cpp` to `EditorMainPanelBootstrap.cpp`.
- Batch 27 keeps `EditorMainPanel.cpp` as the shell entry point for `Create()` and `Release()` while reducing its direct includes.
- Batch 28 did not change behavior.
- Batch 28 moved `Release()` into `EditorMainPanelBootstrap.cpp`, pairing backend teardown with backend initialization.
- Batch 28 leaves `EditorMainPanel.cpp` as the visible `Create()` construction flow with only the main panel header included.
- Batch 29 did not change behavior.
- Batch 29 moved `Create()` into `EditorMainPanelBootstrap.cpp` and removed the now-empty `EditorMainPanel.cpp` from the project.
- Batch 29 also moved small inline public methods out of `EditorMainPanel.h`: PIE fullscreen query now lives with PIE code, and widget-selection reset now lives with widget setup code.
- Batch 30 did not change behavior.
- Batch 30 moved public widget accessor implementations out of `EditorMainPanel.h` and into `EditorMainPanelWidgetSetup.cpp`.
- Batch 30 grouped `EditorMainPanel` public and private declarations by responsibility, keeping the header focused on API shape rather than small implementation details.
- Batch 31 did not change behavior.
- Batch 31 moved `EditorMainPanel` private state structs into `EditorMainPanelState.h` and gave them `FEditorMainPanel...` names so the main header reads as shell API plus owned state.
- Batch 31 kept viewport icon resources inside `FEditorMainPanel` because they depend on the panel-owned `EViewportToolIcon` enum and should move together with the future viewport toolbar class.
- Batch 32 did not change behavior.
- Batch 32 moved the viewport toolbar icon enum and icon resource state into `EditorMainPanelState.h`, removing another UI-only implementation detail from `EditorMainPanel.h`.
- Batch 32 renamed the icon enum to `EEditorMainPanelViewportToolIcon` so the type remains explicit after leaving the class body.
- Batch 33 did not change behavior.
- Batch 33 grouped the by-value editor widget instances into `FEditorMainPanelWidgetSet`, reducing the visible member list in `EditorMainPanel.h` while keeping the existing widget ownership under `FEditorMainPanel`.
- Batch 33 updated internal `EditorMainPanel*.cpp` call sites to access widgets through `Widgets`, preserving external getter APIs for existing callers.
- Batch 34 did not change behavior.
- Batch 34 moved viewport toolbar label and camera-speed helper policy into `FEditorMainPanelViewportToolbarHelpers`, replacing an anonymous helper namespace with a named helper class.
- Batch 35 did not change behavior.
- Batch 35 moved `RenderViewportMenuBarForIndex()` into `EditorMainPanelViewportMenuBar.cpp`, reducing `EditorMainPanelViewportToolbar.cpp` from 1052 lines to 749 lines and making menu-bar responsibility easier to find.
- Batch 36 did not change behavior.
- Batch 36 moved placement viewport location policy into `FEditorMainPanelPlacementHelpers` so static mesh/prefab placement and viewport context menu share the same named helper.
- Batch 36 moved `TickViewportContextMenu()` and `RenderViewportContextMenu()` into `EditorMainPanelViewportContextMenu.cpp`, reducing `EditorMainPanelPlacement.cpp` from 731 lines to 287 lines.
- Batch 37 did not change behavior.
- Batch 37 moved packaging path, scene-list, file-dialog, and extension helper policy into `FEditorMainPanelPackagingHelpers`, replacing the anonymous helper namespace with a named Editor-only helper.
- Batch 37 reduced `EditorMainPanelPackaging.cpp` from 561 lines to 458 lines while keeping packaging UI behavior and GameClient exclusions unchanged.
- Batch 38 did not change behavior.
- Batch 38 moved shared viewport text/icon button drawing into `EditorMainPanelViewportButtons.cpp`, reducing `EditorMainPanelViewportToolbar.cpp` from 749 lines to 643 lines.
- Batch 38 keeps the existing `FEditorMainPanel` button API intact so viewport menu, toolbar, and layout controls continue to share the same drawing behavior.
- Batch 39 did not intentionally change behavior.
- Batch 39 reduced Engine -> Editor include violations from 8 to 1 by removing EditorEngine fallback from `PursuitMovementComponent`, moving editor input controllers to `Editor/Input`, moving `FSceneViewport` ownership out of `SViewport`, and passing shadow filter mode through `FRenderBus` instead of reading `FEditorSettings` inside render passes.
- Batch 39 intentionally left `Engine/Runtime/EngineLoop.cpp` launch glue as the only remaining Engine -> Editor include violation for a later launch-mode factory pass.
- Batch 40 did not intentionally change behavior.
- Batch 40 moved launch-mode engine selection into `Launch/LaunchModeFactory`, added `UEngine::CanCloseApplication()` as the runtime-facing close hook, and let `UEditorEngine` own the editor close prompt. `EngineLoop` no longer includes or casts to `UEditorEngine`.
- Batch 41 did not change runtime/editor behavior.
- Batch 41 extended `Scripts/CheckArchitecture.ps1` to verify every `Source/Editor/*` compile item in the Visual Studio project is excluded from both `GameClientDebug|x64` and `GameClientRelease|x64`.
- Batch 42 did not intentionally change texture behavior.
- Batch 42 extracted `FTextureResourceCache` from `FResourceManager`, keeping the public `GetTexture`/`LoadTexture` facade intact while moving texture object storage, write-time tracking, reload-on-change, default-white SRV lookup, and texture release into the focused cache.
- Batch 43 did not intentionally change shader behavior.
- Batch 43 extracted `FShaderResourceCache` from `FResourceManager`, keeping the public shader and compute-shader facade intact while moving shader object storage, permutation compile/load, reload, compute-shader storage, and shader release into the focused cache.
- Batch 44 did not intentionally change material behavior.
- Batch 44 extracted `FMaterialResourceCache` storage ownership from `FResourceManager`, keeping material parsing and serialization in the facade for now while moving material/material-instance maps, material name listing, slot aliases, material memory accounting, and material release into the focused cache.
- Batch 45 did not intentionally change render state behavior.
- Batch 45 extracted `FRenderStateResourceCache` from `FResourceManager`, keeping the public sampler/depth-stencil/blend/rasterizer facade intact while moving D3D state object creation, lookup, and release into the focused cache.
- Batch 46 did not intentionally change asset path behavior.
- Batch 46 extracted `FAssetPathPolicy` from `FResourceManager`, moving runtime asset existence checks and static-mesh binary path policy for cooked, sibling, cache, and writable cache paths into a focused policy class.
- Batch 47 did not intentionally change static mesh load behavior.
- Batch 47 extracted `FStaticMeshResourceCache` storage ownership from `FResourceManager`, keeping OBJ/BIN loading flow in the facade for now while moving static-mesh registry, loaded-mesh lookup/register, load-option lookup, and static-mesh release into the focused cache.
- Batch 48 extracted `FAtlasResourceCache` from `FResourceManager`, keeping the public font/particle facade intact while moving font/particle atlas loaders, resource maps, lookup fallback, GPU loading, and release into the focused cache.
- Batch 49 did not intentionally change imported material behavior.
- Batch 49 extracted `FImportedMaterialPolicy` from `FResourceManager`, moving imported material asset naming, OBJ `mtllib` resolution, OBJ material slot collection, and material slot alias key policy into a focused policy class.
- Batch 50 extracted `FCurveResourceCache` from `FResourceManager`, keeping the public curve facade intact while moving curve loader ownership, loaded-curve lookup, save registration, and curve release into the focused cache.
- Batch 51 extracted curve/material asset path classification into `FAssetPathPolicy`, removing the remaining extension-classification helper functions from `FResourceManager`.
- Batch 52 synced the roadmap against the current main-merged project state.
- Batch 52 verified the current architecture guard baseline, `Debug|x64` Editor build, and `GameClientDebug|x64` build after main merge.
- Batch 52 fixed the main-merged `Shaders\Multipass\PostProcess.hlsl` project item classification so GameClient no longer attempts to compile the HLSL file as C++.
- Batch 53 did not intentionally change asset discovery behavior.
- Batch 53 moved duplicated asset extension classification and registration from `LoadFromAssetDirectory()` and `RefreshFromAssetDirectory()` into `RegisterDiscoveredAssetFile()`, with list reset policy named as `ClearDiscoveredResourceLists()`.
- Batch 54 did not intentionally change static mesh load behavior.
- Batch 54 moved loaded static mesh UObject creation and LOD generation policy into `CreateStaticMeshFromLoadedData()`, preserving the existing log differences between binary-drop and normal OBJ/BIN load paths.
- Batch 55 did not intentionally change default resource behavior.
- Batch 55 split default resource initialization into named steps for default white texture, default material parameters, and outline material setup.
- Batch 56 did not intentionally change static mesh load behavior.
- Batch 56 moved OBJ/BIN static mesh load orchestration from `FResourceManager` into `FStaticMeshLoadService`, keeping `FResourceManager::LoadStaticMesh()` as the facade entry point.
- Batch 57 did not intentionally change material load behavior.
- Batch 57 moved OBJ/MTL material load orchestration from `FResourceManager` into `FMaterialLoadService`, keeping `FResourceManager::LoadMaterial()` as the facade entry point.
- Batch 58 did not intentionally change memory reporting behavior.
- Batch 58 moved material memory reporting through `FResourceMemoryReporter`, keeping `FResourceManager::GetMaterialMemorySize()` as the facade entry point.
- Batch 59 did not intentionally change material serialization behavior.
- Batch 59 moved `.mat`/`.matinst` JSON serialization and deserialization from `FResourceManager` into `FMaterialSerializationService`, keeping the existing public `SerializeMaterial()`, `SerializeMaterialInstance()`, and `DeserializeMaterial()` entry points.
- Batch 60 did not intentionally change editor picking behavior.
- Batch 60 moved viewport-local actor point picking from `FEditorViewportClient` into `FEditorPickingService`, preserving ID-buffer-first picking with raycast fallback.
- Batch 61 did not intentionally change editor box selection behavior.
- Batch 61 moved drag-box actor collection and projection policy from `FEditorViewportClient` into `FEditorBoxSelectionService`, keeping the viewport client as the state holder and caller.
- Batch 62 did not intentionally change transform or gizmo behavior.
- Batch 62 moved transform mode cycling, transform-mode-to-gizmo mapping, and per-frame gizmo visual sync into `FEditorTransformInteraction`, keeping `FEditorViewportClient` as the owner of the current mode and gizmo pointer.
- Batch 63 did not intentionally change texture asset meta behavior.
- Batch 63 moved texture `.meta` JSON load/create policy and asset meta type definitions into `FTextureAssetMetaService`, keeping `FResourceManager::LoadOrCreateTextureMeta()` as the facade entry point.
- Batch 64 did not intentionally change PIE viewport control behavior.
- Batch 64 moved PIE start/end, possession/eject mode switching, PIE player-controller routing, shell command dispatch, and mouse-focus release bookkeeping into `FEditorViewportPIEController`, keeping `FEditorViewportClient` as the viewport state owner and shell command handler.
- Batch 65 did not intentionally change focus-selection navigation behavior.
- Batch 65 moved primary-selection camera focus positioning into `FEditorViewportNavigationController`, keeping `FEditorViewportClient` responsible for input command routing and editor-world controller target sync.
- Batch 66 did not intentionally change editor viewport navigation behavior.
- Batch 66 moved free-orbit/pan/dolly, plain-left navigation, keyboard navigation routing, move-speed wheel adjustment, reset-camera policy, and viewport camera-mode positioning into `FEditorViewportNavigationController`, leaving `FEditorViewportClient` as the viewport state owner and coordinator.
- Batch 67 did not intentionally change render behavior.
- Batch 67 moved runtime primitive draw command generation into `FPrimitiveDrawCommandBuilder` and light/shadow request extraction into `FLightRenderCollector`, reducing `RenderCollector.cpp` from roughly 1120 lines to roughly 670 lines while keeping culling, decal collection, and editor overlay collection in place for the next Render cleanup step.
- Batch 68 did not intentionally change decal render behavior.
- Batch 68 moved decal OBB queries, target static-mesh command generation, and decal collection stats into `FDecalCommandBuilder`.
- Batch 69 did not intentionally change editor overlay render behavior.
- Batch 69 moved selection mask/outline, gizmo, grid, selected-light debug drawing, and selected-primitive bounding-volume drawing into `FEditorOverlayCollector`, reducing `RenderCollector.cpp` to roughly 250 lines.

### Batch 1: Guard Rails and API Baseline

Purpose:

- Create repeatable checks before moving code.
- Make current API/dependency violations visible.

Scope:

- Rewrite this roadmap in readable form.
- Add architecture dependency check script.
- Track current known violations without fixing all of them at once.
- Verify Editor Debug build.

Expected touched files:

- `ENGINE_STRUCTURE_REFACTOR_ROADMAP.md`
- `Scripts/CheckArchitecture.ps1`

Risk:

- Low. No runtime behavior should change.

### Batch 2: EditorEngine API Surface

Purpose:

- Remove legacy public wrappers after subsystem extraction.
- Keep only intentional EditorEngine entry points.

Candidate cleanup:

- Undo/Redo wrappers: already moved to `FEditorUndoSystem`.
- PIE wrappers: keep direct use of `FPIESession`.
- RML wrappers: keep `FRmlUiSystem` on Engine.
- Scene document commands: move later to `EditorSceneDocumentController`.

Risk:

- Low to medium. Mostly call-site changes.

### Batch 3: MainPanel First Split

Purpose:

- Reduce `EditorMainPanel` risk without changing UX.

Order:

1. Extract packaging panel.
2. Extract footer/debug/undo panels.
3. Extract viewport icon set and viewport toolbar.
4. Extract runtime UI preview bridge.

Risk:

- Medium. UI draw order and modal state must be preserved.

### Batch 4: Scene Document and Dirty State

Purpose:

- Make save/load/new/close behavior understandable.

Targets:

- `EditorSceneDocument`
- `EditorSceneDirtyTracker`
- `EditorSceneDocumentController`

Risk:

- Medium. Dirty prompts and last-scene restore must be checked carefully.

### Batch 5: Input and Viewport Interaction

Purpose:

- Clarify ownership of selection, picking, gizmo, camera navigation, and PIE shell input.

Targets:

- `EditorPickingService`
- `EditorTransformInteraction`
- `EditorViewportNavigationController`
- `PIEViewportControlBridge`

Risk:

- High. This can affect transform shortcuts, camera movement, PIE input, and cursor capture.

### Batch 6: Resource and Asset Pipeline

Purpose:

- Split ResourceManager while preserving OBJ -> BIN -> Runtime behavior.

Targets:

- `AssetPathPolicy`
- `TextureResourceCache`
- `ShaderResourceCache`
- `MaterialResourceCache`
- `StaticMeshResourceCache`
- `ImportedMaterialService`

Risk:

- High. This affects asset loading, packaging, material references, CSO cache, and GameClient.

### Batch 7: Render Proxy Architecture

Purpose:

- Move toward a cleaner render architecture.

Targets:

- `PrimitiveSceneProxy`
- `SceneRenderState`
- `FrameContext`
- `DrawCommandBuilder`

Risk:

- Very high. This should happen after UI/Input/Asset boundaries are calmer.

## Manual Verification Matrix

After relevant batches, check:

- Editor launches
- last scene loads or falls back to New Scene
- selecting Actor shows Actor details
- selecting Component shows Component details
- duplicate Actor does not share tags/components incorrectly
- transform gizmo shortcuts work: Space, QWER, 1234
- gizmo hover and picking stop during LMB/RMB capture
- PIE possessed input works
- PIE Eject shows editor helpers according to viewport settings
- PIE Esc/F8/Shift+F1 work
- Runtime RML UI renders and clicks at correct positions
- Runtime UI Preview renders
- GameClient launches packaged scene
- OBJ-free BIN runtime load works

## Current Known Dependency Violations

None in the current `Scripts/CheckArchitecture.ps1` baseline.

## Completion Definition

This refactor is considered structurally complete when:

- `Engine -> Editor include` count is zero.
- `GameClient` builds without compiling Editor source.
- The old monolithic `EditorMainPanel.cpp` is removed or remains only as a tiny shell entry point.
- `EditorViewportClient` no longer owns all interaction details directly.
- `ResourceManager` is a facade over focused caches/services.
- render collection and draw command building are separate responsibilities.
- public APIs map to real owners.
- Editor, PIE, and GameClient behavior remain intact.
