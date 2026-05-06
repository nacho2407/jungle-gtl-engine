# Architecture Refactor Audit

Updated: 2026-05-06

This document tracks structural risks before broad refactoring. The project must be reviewed as four separate execution domains:

- Engine: reusable runtime core, rendering, assets, world, object, input primitives.
- Editor: editor app, ImGui panels, editor viewport, selection, packaging, project settings.
- PIE: editor-hosted game world and viewport mode. It shares the editor process but must obey runtime rules where possible.
- GameClient: standalone runtime executable built with `WITH_EDITOR=0`.

Do not start broad refactoring from this document without explicit approval. Fixes should be batched, side effects checked per domain, and legacy paths removed rather than duplicated.

## Current Structural Signal

Large files/classes:

- `NipsEngine/Source/Editor/UI/EditorMainPanel.cpp`: 4158 lines.
- `NipsEngine/Source/Engine/Core/ResourceManager.cpp`: 2441 lines.
- `NipsEngine/Source/Editor/EditorEngine.cpp`: 2319 lines.
- `NipsEngine/Source/Editor/Viewport/EditorViewportClient.cpp`: 2257 lines.
- `NipsEngine/Source/Editor/UI/EditorPropertyWidget.cpp`: 2251 lines.
- `NipsEngine/Source/Engine/Spatial/BVH.cpp`: 2004 lines.
- `NipsEngine/Source/Editor/UI/EditorContentBrowserWidget.cpp`: 1600 lines.
- `NipsEngine/Source/Editor/Packaging/GamePackager.cpp`: 1448 lines.
- `NipsEngine/Source/Engine/GameFramework/PrimitiveActors.cpp`: 1436 lines.
- `NipsEngine/Source/Engine/Render/Renderer/Renderer.cpp`: 1416 lines.
- `NipsEngine/Source/Engine/Render/Scene/RenderCollector.cpp`: 1121 lines.

Lambda/helper-heavy files:

- `ScriptManagerMathBindings.cpp`, `LuaUIAPI.cpp`, `LuaInputAPI.cpp`, `LuaWorldAPI.cpp`, `LuaAudioAPI.cpp`: many lambda bindings. This is mostly binding glue, lower priority unless script API behavior is being changed.
- `EditorPropertyWidget.cpp`, `EditorContentBrowserWidget.cpp`, `EditorMainPanel.cpp`, `EditorSceneWidget.cpp`: UI logic uses local lambdas heavily. Higher refactor value because UI state, undo, selection, and asset mutation are mixed.
- `Renderer.cpp`, `InputRouter.cpp`, `EditorViewportClient.cpp`: local helper logic affects runtime behavior. Prefer extracting small named functions/classes when touching.

## Critical Risks

### 1. Engine has Editor dependencies

Observed:

- `Engine/Runtime/EngineLoop.cpp` creates `UEditorEngine`, `UObjViewerEngine`, or `UGameEngine` through preprocessor branches.
- `Engine/Core/ResourceManager.cpp` reads `Editor/Settings/EditorSettings.h` under `WITH_EDITOR` for LOD behavior.
- `Engine/Input/Controller/EditorController/*` lives under `Engine` but includes `Editor/Selection/SelectionManager.h` and `Editor/Settings/EditorSettings.h`.
- `Engine/Render/Renderer/Renderer.cpp` includes `Editor/Viewport/FSceneViewport.h` for editor ID picking.
- `Engine/Slate/SViewport.h` includes `Editor/Viewport/FSceneViewport.h`.
- `Engine/Render/Renderer/RenderFlow/OpaqueRenderPass.cpp` and `VSMConversionRenderPass.cpp` read editor shadow settings.
- `Engine/Render/Mesh/MeshManager.*` is effectively `FEditorMeshLibrary`, used by engine mesh buffers.

Risk:

- GameClient can compile only because some editor files are excluded and some includes are preprocessor guarded. The boundary is policy-based, not architecture-based.
- Runtime rendering and asset systems can accidentally depend on editor settings or viewport types.
- Future GameClient/Release changes may break from an Editor-only include or setting.

Recommended batch:

1. Move editor controllers from `Engine/Input/Controller/EditorController` to `Editor/Input`.
2. Replace `FSceneViewport` dependency in `Renderer` with an engine-level viewport render resource/readback interface.
3. Move editor-only mesh/gizmo mesh library out of generic engine mesh manager or rename/split it clearly.
4. Replace editor setting reads in engine render passes with render settings passed through `FRenderBus` or renderer config.

### 2. Input ownership policy is still centralized but not fully authoritative

Observed:

- `InputSystem` samples raw OS state and stores GUI capture flags.
- `InputRouter` resolves viewport focus/capture/hover, relative mouse, absolute clip, and ImGui blocking.
- `EditorViewportClient` still contains substantial mode-specific input handling: editor navigation, gizmo, selection, PIE possess/eject, mouse focus release, cursor lock, transform shortcuts.
- `GameEngine` also pumps RML UI and player input.
- Lua can consume input through script APIs.

Risk:

- RML, ImGui, EditorWorldController, GameInputBridge, PlayerController, and Lua all observe input through different layers.
- A fix in one path can reintroduce issues like transform shortcuts not firing, gizmo hover continuing while pointer is captured, or PIE cursor capture conflicting with editor focus.
- Current policy is mostly encoded as conditionals instead of a single visible contract.

Recommended batch:

1. Write one input ownership contract first: `OS -> InputSystem -> UI Capture -> Viewport Router -> Domain Controller -> Gameplay/Lua`.
2. Add explicit input domains: `EditorUI`, `EditorViewport`, `PIEViewport`, `RuntimeUI`, `Game`.
3. Make LMB/RMB capture suppress picking, gizmo hover tint, and passive selection feedback consistently.
4. Keep `InputSystem` as sampler only. Move ownership decisions into one router/policy layer.

### 3. Editor, PIE, and Runtime UI responsibilities are mixed

Observed:

- `EditorEngine.cpp` and `GameEngine.cpp` both contain RML key mapping, form-control checks, runtime UI input pumping style code.
- `EditorMainPanel.cpp` owns normal editor UI, viewport layout, PIE viewport drawing, RML-over-viewport ordering, packaging UI, editor debug UI, and scene/project settings UI.
- PIE is implemented inside editor viewport/client/panel state rather than a small PIE session object.

Risk:

- Draw order bugs between RML, viewport, and ImGui can reappear.
- PIE input and runtime UI behavior can diverge from GameClient.
- Editor UI changes can accidentally change PIE behavior.

Recommended batch:

1. Extract runtime RML input/render adapter shared by `UEditorEngine` PIE and `UGameEngine`.
2. Extract `FPIESession` or similar owner for active PIE world, player controller, viewport index, possessed/editor-control mode, and runtime UI context.
3. Split `EditorMainPanel` into smaller surfaces: main dock/layout, viewport area, toolbar, packaging panel, debug panel, PIE overlay.

### 4. ResourceManager is too broad

Observed:

- `ResourceManager.cpp` owns texture load, shader load/reload/cache, material load/import, OBJ/MTL material import, static mesh load, cooked binary paths, auto material names, default materials, editor/runtime LOD policy.

Risk:

- Asset loading fixes can affect material naming, shader cache, static mesh binary fallback, editor defaults, and runtime packaging.
- It is hard to reason about OBJ -> BIN -> Runtime and material reference policy in isolation.

Recommended batch:

1. Split by responsibility without changing behavior:
   - `TextureResourceCache`
   - `ShaderResourceCache`
   - `MaterialResourceCache`
   - `StaticMeshResourceCache`
   - `ImportedMaterialService`
2. Move editor-only LOD policy out of the resource manager and pass it as load/build options.
3. Keep public `FResourceManager` facade temporarily only if needed, then remove legacy facade call sites batch by batch.

## High Risks

### EditorViewportClient is a mode controller, selection service, input adapter, camera owner, and PIE bridge

Risk:

- Changes to picking, gizmo, transform shortcuts, camera, or PIE focus all land in the same class.
- Recent bugs around gizmo disappearing, selection fallback, transform mode, and cursor lock all touch this class.

Recommended batch:

- Keep `FEditorViewportClient` as coordinator.
- Move picking to `FEditorPickingService`.
- Move transform/gizmo input to `FEditorTransformInteraction`.
- Move PIE possession/focus to `FPIESession`; keep `FGameInputBridge` limited to gameplay input forwarding.
- Move camera navigation to `FEditorViewportNavigationController`.

### EditorPropertyWidget and EditorMainPanel are too stateful

Risk:

- Details fallback/component selection bugs are likely tied to UI state, selected actor/component state, and reflection/property editing being mixed.
- Packaging, scene settings, project settings, runtime preview, view mode menus, and debug controls all live in one panel file.

Recommended batch:

- First extract pure property model/builders from `EditorPropertyWidget`.
- Then split draw-only UI from mutation/undo code.
- For `EditorMainPanel`, extract panels by user-facing surface, not by helper function.

### Render pipeline mixes editor-only passes into engine-level pipeline

Observed:

- Engine render pipeline contains `EditorRenderPass`, `SelectionMask`, `PostProcessOutline`, ID picking resources/constants, and editor debug view concepts.

Risk:

- Runtime renderer can accumulate editor-only resources and shaders.
- GameClient may carry code and resource cost for editor features even if unused.

Recommended batch:

- Keep editor visualization passes in an editor render extension or editor pipeline.
- Keep engine render passes generic: scene color, depth, lighting, translucency, post-process.
- Editor can request extra passes through a clearly editor-only pipeline object.

### Scene serialization includes editor camera state in Engine

Observed:

- `SceneSaveManager` lives in Engine but knows `FEditorCameraState`.

Risk:

- Runtime scene serialization and editor scene persistence are coupled.
- Dirty scene detection and last-scene restore can remain confusing.

Recommended batch:

- Split `WorldSnapshotSerializer` from `EditorSceneDocumentSerializer`.
- Keep editor camera/layout metadata in an editor section.
- Runtime load should ignore editor metadata cleanly.

## GameClient Specific Notes

Observed:

- GameClient configurations set `WITH_EDITOR=0`, `IS_GAME_CLIENT=1`, and exclude most `Source/Editor/*`.
- GameClient still compiles broad Engine systems that contain editor naming or editor-oriented classes.
- `UGameEngine` owns runtime RML UI and player input directly.

Risk:

- GameClient build health depends on many `#if WITH_EDITOR` guards being correct.
- Runtime UI behavior is not guaranteed to match PIE because the code path is duplicated.

Recommended batch:

- Add a regular GameClient Debug build check after any Engine/Renderer/Input refactor.
- Keep `Source\Editor` out of GameClient include path, and prevent Engine files from including Editor headers unguarded.
- Share runtime UI adapter between PIE and GameClient.

## PIE Specific Notes

Observed:

- PIE is an editor mode, not a separate runtime layer.
- Editor viewport, editor main panel, runtime UI, player controller, and input focus all participate in PIE.

Risk:

- PIE can behave differently from GameClient while appearing to be runtime.
- Editor UI draw order and input capture can override runtime behavior.

Recommended batch:

- Define PIE as `Editor-hosted Game world + Runtime input/UI adapter + Editor shell`.
- Keep editor shell features outside the runtime adapter.
- Make the active PIE viewport/session explicit.

## Lower Priority / Deferred

- Script API binding files are lambda-heavy, but that is expected for binding registration. Refactor only when changing the script API surface.
- Math headers and utility headers use many inline/static helpers. Do not churn them unless correctness or compile-time issues show up.
- BVH is large, but isolated enough. Refactor after behavior tests exist.
- Primitive actor factory file is broad but mostly actor definitions. Split when adding more actor classes.

## Suggested Refactor Order

1. No-code policy doc for module boundaries and input ownership.
2. Engine/editor dependency cleanup that does not change behavior:
   - Renderer no longer includes `FSceneViewport`.
   - Editor controllers move out of Engine.
   - Editor settings no longer read directly from engine render passes.
3. Extract PIE session and runtime UI adapter shared by PIE/GameClient.
4. Split `EditorViewportClient` by picking, transform interaction, navigation, and PIE focus.
5. Split `ResourceManager` by asset type and build/load policy.
6. Split large editor UI panels after behavior-critical systems are stable.

## Approval Gate

Before starting any refactor from this list, confirm:

- Target domain: Engine, Editor, PIE, GameClient, or cross-domain.
- Batch size and expected touched files.
- Required validation: Editor Debug build, GameClient Debug build, scene load/save smoke, PIE smoke, packaging smoke.
- Whether behavior changes are allowed or only structure-preserving edits.
