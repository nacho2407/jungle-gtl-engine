# Engine Bug Tracking

This document is the single working tracker for the current bug-fix pass.
Keep it updated before and after each batch.

## Core Premise

- Always distinguish Engine, Editor, PIE, and Runtime(GameClient).
- Always inspect the latest project code before deciding a fix.
- Prefer small, verifiable fixes over broad refactors.
- Avoid adding complexity for side-effect prevention. Fix ownership/lifetime/state boundaries directly.
- Avoid lambdas and namespaces in new/changed code unless there is a clear local precedent and explicit agreement.
- Do not keep unnecessary legacy paths.
- Batch related changes, but do not combine unrelated systems in one patch.
- If behavior is ambiguous, stop and ask for agreement. Even if the user says "go ahead", make the exact decision explicit before applying risky or broad changes.
- Git operations only when explicitly requested.
- Build only when explicitly requested.

## Current Bug Reports

### Bug 1. Actor/Component Tags Shared After Duplicate

Status: Patched, needs verification

Report:
- When Actors or Components are copied, Tags appear to be shared.
- Duplicate must produce independent Actor Name, Component Name, and Tag state where appropriate.

Current code observations:
- `AActor::PostDuplicate` deep-copies actor tags with `Tags = OrigActor->Tags`.
- `UActorComponent` exposes component tags through editable `TagsText`; `CopyPropertiesFrom` copies the string and calls `PostEditProperty("Tags")`, which rebuilds the destination `Tags`.
- `TArray` is `std::vector`, so a plain assignment should not share the tag container itself.
- Most Component `PostDuplicate` implementations call their parent `PostDuplicate`.
- `UActorComponent` does not define its own explicit `PostDuplicate`; it currently relies on inherited `UObject::PostDuplicate` plus `CopyPropertiesFrom`.
- `UObject::PostDuplicate` copies `ObjectName` from the original.
- `AActor::PostDuplicate` does not call `UObject::PostDuplicate`, so Actor names are not preserved during generic Actor duplicate. This matters for PIE world duplication.
- Editor Ctrl+D and PIE world copy both use the same generic `Duplicate()` path, but they need different Actor name policy:
  - PIE copy should preserve scene Actor names.
  - Editor Ctrl+D should produce independent, unique Actor names.
- Actor Tag editing in Details intentionally applies to every selected Actor when multi-selection is active. This can look like tag sharing after duplicating multiple Actors because the duplicate selection contains all duplicated Actors.

Patch:
- `AActor::PostDuplicate` now calls `UObject::PostDuplicate` so generic Actor duplicate preserves ObjectName for PIE/world copy.
- Editor Ctrl+D now assigns a UE-style unique Actor name after duplication, keeping Editor duplicate policy separate from Engine duplicate policy.
- Editor-generated Actor names use `BaseName_#` within the World, and old nested `Name (1) (1)` forms are normalized back to the base before numbering.
- `UActorComponent::PostDuplicate` now explicitly copies component Tags and refreshes `TagsText`, making component duplicate tag state independent and obvious.
- Newly added Components use `BaseName_#` within their Owner Actor. Component names duplicated into a new Actor are preserved because the Actor is the effective naming scope.

Next checks:
- Verify single Actor Ctrl+D, edit duplicate Actor tags, confirm source Actor tags do not change.
- Verify multi-Actor Ctrl+D, then note Details tag edits apply to all selected duplicates by design.
- Verify PIE world duplicate keeps Actor names and component names from the Editor scene.
- Verify Runtime loaded-scene names/tags still match serialized scene data.

### Bug 2. StaticMesh OBJ -> Binary -> Runtime Load Path

Status: Patched, needs verification

Report:
- Intended flow: OBJ source can be converted to binary, and Runtime should load from binary.
- If `.bin` exists, Runtime must work even when `.obj` is absent.

Required distinction:
- Editor may need OBJ import/conversion.
- Runtime(GameClient) must not require OBJ when a valid binary mesh exists.

Current code observations:
- Packaging cooks `.obj` dependencies to `Asset/Cooked/Mesh/...bin` and rewrites scene paths to cooked `.bin`.
- `FResourceManager::LoadStaticMesh(".bin")` can load binary mesh data directly.
- Missing `.obj` fallback only checked the cooked path, not sibling `.bin` or the existing static mesh cache path `Asset/Mesh/Bin/...bin`.
- Binary validity checks for `.obj` cache require source OBJ timestamp, so they cannot be used when the OBJ is intentionally absent.

Patch:
- Missing `.obj` now falls back in this order:
  1. `Asset/Cooked/Mesh/...bin`
  2. sibling `.bin` next to the requested OBJ
  3. current static mesh cache `Asset/Mesh/Bin/<name>.bin`
- Direct `.bin` loading no longer tries to load OBJ material data unless the recorded source OBJ path still exists.

Next checks:
- Verify packaged Runtime scene loads rewritten cooked `.bin` paths.
- Verify requesting a missing `.obj` succeeds when only a valid `.bin` exists in one of the fallback locations.
- Verify Editor OBJ import/conversion still creates cache binary when OBJ exists.

### Bug 3. Details Component Selection Falls Back To Actor

Status: Patched, needs verification

Report:
- Selecting a Component in Editor Details sometimes shows Actor properties instead.
- Reopening the same scene can make it work, or fail in the opposite direction.

Likely related areas:
- `FSelectionManager`
- `FEditorPropertyWidget`
- Outliner component selection
- Duplicate/PIE selection identity

Patch:
- Component selection validation now avoids global `FindByUUID()` as the identity authority. It verifies that the selected component pointer is still registered, then checks that the same object still has the stored UUID.
- This prevents a valid Editor component selection from being cleared when another world, especially PIE, contains a duplicate object with the same preserved UUID.

### Bug 4. Gizmo Missing When Cursor Enters Viewport

Status: Patched, needs verification

Report:
- Selecting Actor in Outliner shows Gizmo.
- Moving cursor into Viewport can make Gizmo disappear.

Likely related areas:
- `FSelectionManager::SyncGizmo`
- `FEditorViewportClient::ApplyTransformModeToGizmo`
- viewport input routing and hover/capture state

Patch:
- Viewport now synchronizes Gizmo visual state before hover early-return, so an Outliner selection cannot leave the Gizmo in a stale visible state until the cursor enters the viewport.
- Gizmo target validation now uses live pointer registration plus UUID confirmation instead of global UUID lookup only.

### Bug 5. Duplicate Sometimes Triggers Bug 3/4

Status: Partially patched, needs verification

Report:
- Ctrl+D can produce actors with Details/Gizmo issues.
- Duplicating a broken actor may produce either working or broken copies.

Working assumption:
- Treat as a possible identity, selection, ownership, registration, or stale pointer issue until proven otherwise.

Patch:
- Duplicate-specific Actor/Component name/tag policy was handled in Batch A.
- Selection/Gizmo UUID collision false invalidation was handled in Batch C.

### Bug 6. Gizmo Disappears During Manipulation And Cursor Gets Trapped

Status: Patched, needs verification

Report:
- During Gizmo manipulation, Gizmo can suddenly disappear.
- Cursor is visible and movable but trapped inside the Viewport.
- Clicking or other input may release the trapped state, but Gizmo may not return.

Likely related areas:
- mouse lock/release transitions
- Gizmo holding/pressed state
- viewport absolute clip and relative mouse mode interaction

Patch:
- `UGizmoComponent::Deactivate()` now ends drag state as well as clearing target/visibility.
- `UGizmoComponent::DragEnd()` now clears pending snap delta, preventing stale drag residue after aborted manipulation.
- `UGizmoComponent::SetTarget()` and `SetTargetComponent()` now end any previous drag state before accepting a new target.

### Bug 7. PIE Actor/Component Name And Tag Independence

Status: Patched, needs verification

Report:
- PIE does not share some Actor Name information as expected.
- Actor Name, Component Name, and Tags need verification across Editor, PIE, and Runtime(GameClient).

Required distinction:
- Editor world duplication into PIE world.
- Scene serialization/loading.
- Runtime GameClient loaded world.

Current code observations:
- PIE creates its world with `SourceWorld->Duplicate()`, which duplicates Level, Actors, and Components through the Engine duplicate path.
- Runtime(GameClient) scene loading uses `FSceneSaveManager::Load()` and `FActorSerialization::SpawnActorFromJson()`.
- Scene serialization stores Actor name/tags and Component `ObjectName`/tags; load restores them with preserve-name options.

Patch:
- Actor names are now preserved by generic Engine duplicate through `AActor::PostDuplicate()` calling `UObject::PostDuplicate()`.
- Component names and tags are explicitly preserved through `UActorComponent::PostDuplicate()`.
- Editor Ctrl+D still applies Editor-only unique Actor naming after duplicate, so this does not change PIE or Runtime naming policy.

Next checks:
- Verify PIE world Actors keep Editor scene Actor names.
- Verify PIE components keep Editor scene Component names and tags.
- Verify Runtime(GameClient) loaded scene restores Actor/Component names and tags from serialized scene data.

### Bug 8. Transform/Gizmo Mode Shortcuts Do Not Change Mode

Status: Patched, needs verification

Report:
- `Space`, `Q/W/E/R`, and `1/2/3/4` do not change transform/gizmo mode as expected.

Current code observations:
- The shortcuts are defined in `EditorViewportInputMapping.h`:
  - `Space` -> `CycleGizmoMode`
  - `Q/1` -> Select
  - `W/2` -> Translate
  - `E/3` -> Rotate
  - `R/4` -> Scale
- The routed command path handles these in `FEditorViewportClient::HandleCommandInput()` through `TickEditorShortcuts(Context)`.
- However `FInputRouter::FinalizeAndDispatchInput()` removes keyboard events unless the target viewport is already focused:
  - `bBlockKeyboardForViewport = InOutContext.bImGuiCapturedKeyboard || !InOutContext.bFocused`
- `FocusedViewport` is only set on pointer press in `FInputRouter::UpdatePointerTrackingState()`.

Root cause:
- Hovering the viewport is enough to choose the dispatch target, but not enough to allow keyboard input.
- Therefore the shortcut events can be erased before `HandleCommandInput()` sees them.
- This makes `Space/QWER/1234` appear broken, especially after interacting with Editor UI/toolbar/details instead of first clicking the viewport.
- The first patch still allowed generic ImGui keyboard capture to block viewport shortcuts. This is too broad because `WantCaptureKeyboard` can represent normal UI focus, not only text/modal ownership.
- Follow-up finding: toolbar highlight can change because the active `FEditorViewportClient::TransformMode` changes, but all four viewport clients share one `UGizmoComponent`.
- `SyncGizmoVisualState()` was running before the non-hovered viewport early-return, so non-hovered clients could immediately overwrite the shared Gizmo mode/visibility with their own stale `TransformMode`.

Patch:
- `FInputRouter::FinalizeAndDispatchInput()` now treats the viewport as a keyboard target when it is focused, hovered, captured, or in relative mouse mode.
- ImGui/Rml text input and modal/popup-style viewport blocking still block viewport keyboard events.
- Generic ImGui keyboard capture no longer blocks viewport tool shortcuts while the viewport is the keyboard target.
- This keeps text/modal input protected while making `Space/QWER/1234` responsive under the cursor.
- `FEditorViewportClient::SyncGizmoVisualState()` now lets only the hovered viewport or the viewport that processed routed input drive the shared Gizmo visual mode and screen-space scaling.
- Direct toolbar requests still update that viewport's `TransformMode` immediately, but unrelated non-hovered viewports no longer undo the shared Gizmo state on the next tick.

Input ownership policy draft:
- OS/window focus is the first gate. If the engine window is not foreground, all input is reset.
- Text input is exclusive. ImGui/Rml text fields consume keyboard/text and must not trigger Editor viewport shortcuts, PlayerController, or Lua gameplay bindings.
- Modal or popup UI that blocks the viewport owns mouse and keyboard until dismissed.
- Editor viewport hovered/focused/captured/relative state owns Editor tool commands such as `Q/W/E/R`, `1/2/3/4`, `Space`, `F`, delete, duplicate, and transform gizmo interactions.
- PIE possessed + Runtime GameOnly owns gameplay input through PIEController/PlayerController/Lua.
- PIE editor-control/ejected state routes to Editor viewport policy, not gameplay policy.
- Lua should observe gameplay input after Runtime/PlayerController ownership is decided, not before Editor/ImGui/Rml capture policy.

Next checks:
- Hover the Viewport after interacting with Outliner/Details/Toolbar and press `Q/W/E/R`, `1/2/3/4`, and `Space`.
- Click toolbar Translate/Rotate/Scale, then move into the Viewport and confirm the actual rendered Gizmo mesh matches the highlighted toolbar mode.
- Confirm text fields still capture typing and do not trigger viewport mode changes.

## Later Improvements

### Pending Input Policy 0. Selection Capture Input Ownership

Status: Patched, needs verification

Goal:
- Define and enforce a clear Editor selection/input capture policy for LMB/RMB interactions.
- When a selection, drag, camera navigation, or context operation owns LMB/RMB capture, unrelated viewport feedback must stop until capture is released.

Expected behavior:
- Picking should not run while LMB/RMB capture belongs to another selection or navigation operation.
- Gizmo hover hit-test and hover tint should not update during unrelated LMB/RMB capture.
- Viewport hover feedback should resume only after the owning mouse button is released and capture state is cleared.

Related risk:
- This is part of the broader Editor input policy involving ImGui, RmlUi, EditorController, PIEController, PlayerController, and Lua.
- The fix should be batched with input ownership cleanup, not mixed into rendering-only Gizmo changes.

Patch:
- `UGizmoComponent` now has hover-free `HitTestMesh()` separate from `RaycastMesh()`, so code can test a handle without changing hover tint.
- `FEditorViewportClient::WantsRelativeMouseMode()` uses hover-free Gizmo hit-test when deciding whether plain LMB drag should become viewport navigation.
- `FEditorViewportClient::TickMouseInput()` suppresses passive absolute mouse-move routing while non-Gizmo LMB/RMB capture, drag, relative mouse mode, or box selection is active.
- External viewport picking requests are ignored while LMB/RMB is still down or dragging.
- Active Gizmo manipulation remains exempt so the selected handle axis can continue driving the drag.

Verification:
- Hover a Gizmo handle, press/hold LMB on empty viewport space, and confirm hover tint clears and does not retarget while held.
- RMB-drag camera navigation over the Gizmo and confirm hover tint/picking does not update during the drag.
- Drag an actual Gizmo handle and confirm the active axis still transforms correctly.
- Release the mouse and confirm hover feedback resumes normally.

### Pending UI Layer 1. PIE RmlUi And Editor ImGui Draw Order

Status: Patched, needs verification

Report:
- During PIE, RmlUi is rendered over the Viewport.
- Editor ImGui windows can be visually hidden behind the PIE RmlUi layer.

Goal:
- Preserve Runtime/PIE RmlUi over the game Viewport while keeping Editor ImGui overlays, popups, and tool windows visually above Runtime UI when they belong to the Editor.

Likely areas:
- `UEditorEngine::RenderRuntimeUI`
- Editor render pipeline viewport composition
- ImGui frame/render order in `FEditorMainPanel`
- RmlUi render target or final composite pass order

Policy note:
- Do not solve by disabling RmlUi during PIE.
- The intended fix is layer/composition order: Engine/Runtime UI should stay inside the game viewport layer, while Editor UI should remain the top editor layer.

Patch:
- `FEditorMainPanel::Render()` now drains and renders pending PIE RmlUi contexts before submitting the final ImGui draw data.
- PIE Runtime UI still renders over the game viewport layer, but Editor ImGui panels, popups, drawers, and overlays are submitted last as the Editor top layer.

Verification:
- Enter PIE with an RmlUi screen visible.
- Open or move Editor ImGui windows over the PIE viewport and confirm they appear above the Runtime UI.
- Confirm the Runtime UI still appears inside the PIE viewport and still receives intended PIE input.

### Pending Scene Workflow 0. Scene Dirty And Content Browser Scene Creation

Status: Patched, needs verification

Report:
- A scene with no user edits can still ask whether unsaved changes should be saved.
- Content Browser should be able to create an empty/default Scene asset in the same form as `Ctrl+N`.

Patch:
- New scenes created through `Ctrl+N` now start clean because default Actor creation is baseline scene setup, not a user edit.
- `FSceneSaveManager::SaveToFilePath()` now saves directly to an exact target path instead of always writing through the global Scene directory.
- `UEditorEngine::CreateDefaultSceneAsset()` creates a temporary default Editor world, spawns the same default actors used by `Ctrl+N`, saves it to the requested path, then destroys the temporary world.
- Content Browser `Create` menu now includes `Scene`.

Verification:
- Press `Ctrl+N`, make no edits, then try another scene-changing action. It should not ask to save.
- Create a Scene from Content Browser and confirm a `.Scene` file appears without replacing the currently open scene.
- Open the created Scene and confirm it contains the default Directional Light, Ambient Light, and Player Start.

### Pending Render Component Policy 0. Billboard Scale Independence

Status: Patched, needs verification

Report:
- `UBillboardComponent` currently uses `GetWorldScale()` when building the camera-facing billboard matrix.
- This means Actor/parent scale can change Billboard visual size and picking bounds.
- Editor helper icons such as Player Start, lights, and other component sprites should likely not inherit Actor scale like normal geometry.

Current code observations:
- `UBillboardComponent::UpdateWorldAABB()` and `RaycastMesh()` both call `MakeBillboardWorldMatrix(GetWorldLocation(), GetWorldScale(), ...)`.
- `MakeBillboardWorldMatrix()` applies `WorldScale` directly to the billboard basis axes.
- `USubUVComponent` inherits `UBillboardComponent`, but it appears to represent actual sprite/VFX-style content where size may be intentional.

UE reference:
- Unreal `UBillboardComponent` is a `UPrimitiveComponent` described as a 2D texture rendered always facing the camera.
- UE exposes Billboard-specific scale/display fields such as `ScreenSize`, `bIsScreenSizeScaled`, and `bUseInEditorScaling`.
- UE also exposes `USceneComponent::SetUsingAbsoluteScale()` for components that should not inherit parent scale.

Decision needed:
- Option A: make all `UBillboardComponent` visuals ignore parent/Actor scale and use only `Width/Height` or editor sprite scale.
- Option B: add a flag such as `bInheritOwnerScale` / `bUseAbsoluteBillboardScale`, default false for editor helper Billboards and true for runtime sprite-like subclasses if needed.
- Option C: split editor helper icons into a dedicated subclass of `UBillboardComponent`, leaving `USubUVComponent` and runtime sprite use cases scale-aware.

Decision:
- Use Option B.
- Add an explicit scale inheritance policy flag, for example `bInheritOwnerScale` or `bUseAbsoluteBillboardScale`.
- Default helper/icon Billboards should ignore Actor/parent scale.
- Derived runtime sprite-like components such as `USubUVComponent` may opt in to inherited scale if their gameplay/VFX size is intended to follow transforms.

Patch:
- `UBillboardComponent` now owns an explicit `bInheritOwnerScale` policy flag.
- Plain Billboard helper/icon components default to not inheriting owner/parent scale.
- `USubUVComponent` opts in to inherited owner scale by default because it represents world-space sprite/VFX content.
- Billboard render matrix, AABB update, raycast picking, and selection-mask matrix construction now use the same scale policy through `GetBillboardWorldScale()`.
- The policy is duplicated, serialized, and exposed in Details as `Inherit Owner Scale`.

Verification:
- Scale an Actor that owns a helper Billboard such as Player Start or light icon and confirm the icon visual size and picking bounds do not grow/shrink with Actor scale.
- Add or inspect a `USubUVComponent`, scale its Actor, and confirm the sprite/VFX still follows Actor scale while `Inherit Owner Scale` is enabled.
- Toggle `Inherit Owner Scale` on a Billboard/SubUV component and confirm visual size, picking, and selection outline bounds change consistently.

### Improvement 0. ThirdParty Prebuilt Library Build Pipeline

Status: Patched, needs verification

Goal:
- Reduce Editor and GameClient build time by moving heavy ThirdParty source builds out of the main `NipsEngine.vcxproj` compile path.
- Start with RmlUi, then consider SoLoud, then ImGui if policy consistency is worth it.

Required configuration coverage:
- Editor Debug
- Editor Release
- GameClient Debug
- GameClient Release

Preferred approach:
- Option A: prebuild `.lib` files with scripts, without adding separate Visual Studio projects.
- Keep ThirdParty source in the repo, but make the main engine project link against generated `.lib` outputs.
- Store outputs under configuration/platform-specific folders, for example:
  - `NipsEngine/ThirdParty/RmlUi/Lib/x64/Debug/RmlUiCore.lib`
  - `NipsEngine/ThirdParty/RmlUi/Lib/x64/Release/RmlUiCore.lib`
  - `NipsEngine/ThirdParty/RmlUi/Lib/x64/GameClientDebug/RmlUiCore.lib`
  - `NipsEngine/ThirdParty/RmlUi/Lib/x64/GameClientRelease/RmlUiCore.lib`

Implementation notes:
- RmlUi currently builds directly from `NipsEngine.vcxproj` through:
  - `ThirdParty\RmlUi\Source\Core\*.cpp`
  - `ThirdParty\RmlUi\Source\Core\Elements\*.cpp`
  - `ThirdParty\RmlUi\Source\Core\Layout\*.cpp`
- RmlUi source count is high, so it is the first candidate.
- The script must use compile options compatible with the consuming configuration:
  - Runtime library selection
  - `RMLUI_STATIC_LIB`
  - C++ standard
  - include paths
  - platform architecture
- `NipsEngine.vcxproj` should exclude RmlUi ThirdParty `.cpp` files only after the matching `.lib` link path and dependency are added for every supported configuration.

Deferred rule:
- Do not start this until the current urgent bug batch is finished or the user explicitly asks to switch to build-pipeline work.
- Before implementation, inspect all current configurations in `NipsEngine.vcxproj` and `ReleaseBuild.bat`.

Patch:
- Added `NipsEngine/BuildTools/Scripts/BuildRmlUiLib.ps1`.
- The script builds `RmlUiCore.lib` directly with `cl.exe` and `lib.exe`; no separate Visual Studio project is added.
- The script checks RmlUi source/header timestamps and skips rebuilding when the generated `.lib` is up to date.
- Output layout is platform/profile-specific:
  - `NipsEngine/ThirdParty/RmlUi/Lib/<Platform>/Debug/RmlUiCore.lib`
  - `NipsEngine/ThirdParty/RmlUi/Lib/<Platform>/Release/RmlUiCore.lib`
- Intermediate objects are written under:
  - `NipsEngine/Intermediate/ThirdParty/RmlUi/<Platform>/<Configuration>/`
- `NipsEngine.vcxproj` now links `RmlUiCore.lib` from the generated lib folder for all project configurations.
- `NipsEngine.vcxproj` now invokes the script before engine compilation.
- Direct wildcard compilation of `ThirdParty\RmlUi\Source\Core\*.cpp`, `Core\Elements\*.cpp`, and `Core\Layout\*.cpp` was removed from the main engine compile path.
- The MSBuild invocation now uses `$(MSBuildProjectDirectory)` instead of trailing-slash `$(ProjectDir)` to avoid PowerShell argument quoting failures.
- The RmlUi prebuild script batches all RmlUi `.cpp` files through response files instead of spawning one `cl.exe` process per file.
- The RmlUi prebuild script now writes a toolchain stamp and rebuilds the `.lib` when `cl.exe`, `lib.exe`, compiler version, platform, or configuration changes. This prevents stale libs built by another MSVC toolset from being linked into the current Engine build.
- The script now resolves `cl.exe` and `lib.exe` through PATH first, then Visual Studio/MSBuild environment variables such as `VCToolsInstallDir` and `VCINSTALLDIR`. This makes the build more portable across Visual Studio IDE, MSBuild, and developer command prompt workflows.
- Added `NipsEngine/BuildTools/Scripts/BuildSoLoudLib.ps1`.
- SoLoud now follows the same generated static library policy as RmlUi.
- The SoLoud prebuild covers only the files previously compiled directly by `NipsEngine.vcxproj`: SoLoud core, wav audio source, `stb_vorbis.c`, and the miniaudio backend.
- `NipsEngine.vcxproj` now links `SoLoud.lib` from `ThirdParty/SoLoud/Lib/<Platform>/<ThirdPartyBinaryConfiguration>` and no longer directly compiles the SoLoud source files.
- `NipsEngine.vcxproj` now maps ThirdParty binary profiles explicitly:
  - `Debug` builds use generated `Debug` ThirdParty libs.
  - Editor `Release`, `GameClientDebug`, and `GameClientRelease` use generated `Release` ThirdParty libs.

Verification:
- First build of each configuration should build `RmlUiCore.lib` once, then link the engine.
- Subsequent builds should print that `RmlUiCore.lib` is up to date and should not recompile all RmlUi sources.
- Verify Editor Debug, Editor Release, GameClient Debug, and GameClient Release.
- `ReleaseBuild.bat` should continue to work because it calls the existing Release x64 MSBuild path, which now triggers the RmlUi prebuild step.

Build notes:
- Debug x64 initially hit `__std_*` unresolved external symbols when linking against a stale `RmlUiCore.lib` produced by a different MSVC toolset.
- The toolchain stamp patch forces RmlUi lib regeneration when the active MSVC compiler/lib tool changes.
- Debug x64 MSBuild passed after regenerating `RmlUiCore.lib` with the active VS2022 toolset.
- SoLoud Debug x64 prebuild passed and produced `SoLoud.lib`.
- Debug x64 MSBuild passed after the SoLoud `.lib` split.
- Editor Release/GameClient lib sharing still needs Release and GameClient build verification.
- GameClientRelease ThirdParty prebuild target passed and reused `Lib/x64/Release` for both RmlUi and SoLoud.

### Improvement 0.0. GameClient StaticMesh Binary Cook Reuse

Status: Patched, needs verification

Goal:
- Reduce GameClient packaging time by reusing local static mesh binary cache outputs instead of recooking every referenced OBJ on every package.

Patch:
- Packaging now checks `Asset/Mesh/Bin/<mesh>.bin` for each OBJ static mesh dependency.
- If the cache binary header matches the current OBJ source timestamp, packaging copies it to the cooked package path under `Asset/Cooked/Mesh/...`.
- If the cache is missing, invalid, or stale, packaging falls back to the existing OBJ cook path.

Verification:
- Package GameClient with existing mesh cache binaries and confirm the manifest logs `Copied cached mesh`.
- Modify or touch an OBJ and confirm packaging falls back to `Cooked mesh` for that source.

### Improvement 0.1. Local CSO Shader Cache

Status: Pending

Goal:
- Prepare local caching for compiled shader object (`.cso`) outputs so unchanged shaders do not need to be recompiled every build/run.

Initial policy:
- Cache should be local/generated output, not source content.
- Cache invalidation should consider shader source timestamp/content, shader profile, entry point, compiler flags, and include dependencies if the shader compiler path supports them.
- Do not start implementation until the current ThirdParty prebuilt library batch is verified.

### Improvement 1. Real Outline Rendering

Status: Deferred

Rule:
- Must request reference before implementation.
- The target is a real visible silhouette/outline, not the current approximate behavior.

### Improvement 2. ID Picking

Status: Deferred

Goal:
- Replace Ray-Triangle picking with ID Picking.

### Improvement 3. Tone Mapping And Bloom Post Process

Status: Deferred

Rule:
- Must request reference before implementation.
- Intended source is another engine's tone mapping and bloom implementation.

### Improvement 4. Engine-Wide Dependency Cleanup And Refactor

Status: Deferred

Rule:
- Do this last.
- Requires approval before starting.

## Batch Plan

Overall progress: 98%

### Batch A. Identity And Duplicate

Progress: 88%

Scope:
- Bug 1
- Bug 5 duplicate-specific parts
- Bug 7 name/tag verification

Do not touch:
- StaticMesh loader
- viewport cursor/gizmo interaction internals unless identity/selection state directly requires it

Completed:
- Bug 1 duplicate name/tag policy patch.
- Editor Actor naming now follows `BaseName_#` numbering for Ctrl+D, Outliner rename collisions, and serialization collision fallback.
- PIE world duplicate path checked: Editor world uses `SourceWorld->Duplicate()`.
- Runtime scene load path checked: GameClient uses `FSceneSaveManager::Load`, which restores serialized Actor/Component names and tags through `FActorSerialization::SpawnActorFromJson`.
- Duplicate selection path checked: Ctrl+D clears old selection, selects only new Actors, and clears component selection through `FSelectionManager`.
- Component duplicate override chain checked: StaticMesh, Primitive, Shape, Light, Movement, Script, and related components route through `UActorComponent::PostDuplicate()`.
- Component add paths now assign `BaseName_#` names scoped to the Owner Actor.

Remaining:
- Manual verification in Editor/PIE/Runtime. No extra duplicate-specific selection patch is planned unless manual repro still fails.
- Bug 7 path has been inspected and is covered by the Batch A duplicate/name/tag patch plus Runtime serialization restore path.

### Batch B. StaticMesh Runtime Loading

Progress: 75%

Scope:
- Bug 2 only

Completed:
- StaticMesh load path inspected across ResourceManager and GamePackager.
- Runtime fallback patch added for cooked, sibling, and current cache binary paths.
- Runtime fallback file existence checks now resolve relative paths against `FPaths::RootDir()`, so GameClient does not depend on the current working directory.

Remaining:
- Manual Runtime/package verification.

### Batch C. Selection, Details, Gizmo Target Validity

Progress: 65%

Scope:
- Bug 3
- Bug 4
- Bug 5 selection/gizmo parts

Completed:
- Gizmo visual mode and screen-space scaling now sync once per viewport tick, even when the viewport is not hovered.
- SelectionManager and Gizmo target validation now check the actual live object pointer first, then confirm its UUID. This avoids false invalidation when Editor and PIE worlds contain objects with the same preserved UUID.

Remaining:
- Manual verification for component Details selection after scene reopen and after Ctrl+D.
- Manual verification for Outliner actor selection, viewport hover, and PIE transition cases.

### Batch D. Gizmo Mouse Capture

Progress: 70%

Scope:
- Bug 6 only

Completed:
- Gizmo deactivation now clears holding and pressed-on-handle state, so viewport cursor clipping no longer stays active after a target disappears or deactivates mid-drag.
- Gizmo target replacement now clears previous drag state before binding the new target.

Remaining:
- Manual verification while dragging Gizmo, deleting/clearing selection, switching mode, and releasing mouse.

### Batch E. Pre-Build Static Risk Check

Progress: 45%

Scope:
- Cross-check compile surface for Batches A-D without running a build.
- Recheck header declarations, helper visibility, and state transition side effects.

Completed:
- `UObjectManager::ContainsObject()` call sites checked: SelectionManager and Gizmo use it only for live-pointer validation before UUID comparison.
- StaticMesh Runtime path helpers checked against `FPaths` policy: relative fallback paths now resolve through `FPaths::RootDir()`.
- Gizmo target replacement checked: `SetTarget()` and `SetTargetComponent()` are selection-sync paths, not per-frame visual sync paths, so clearing previous drag state there is scoped.
- `FEditorViewportClient::SyncGizmoVisualState()` checked: it updates visual mode per tick but does not rebind targets.

Remaining:
- Build/compiler verification when explicitly requested.
- Runtime manual verification through the Verification Matrix.

### Batch F. Verification Asset Preparation

Progress: 100%

Scope:
- Identify existing scenes and assets that can verify patched bugs without editing project content.
- Keep verification prep separate from code patches.

Completed:
- Found PIE-capable scene candidates with PlayerStart, Player tag, CameraComponent, Actor names, Component names, and tags:
  - `NipsEngine/Asset/Scene/Intro.Scene`
  - `NipsEngine/Asset/Scene/Main.Scene`
  - `NipsEngine/Asset/Scene/Playing.Scene`
- Found StaticMesh binary fallback candidates:
  - `NipsEngine/Asset/Scene/Playing.Scene`
  - `Asset/Mesh/cyberpunk_katana/katana.obj` with cache binary `Asset/Mesh/Bin/katana.bin`
  - `Asset/Mesh/cyberpunk_arm/cyber_arm_right_small.obj` with cache binary `Asset/Mesh/Bin/cyber_arm_right_small.bin`
  - `Asset/Mesh/Dice/Dice.obj` with cache binary `Asset/Mesh/Bin/Dice.bin`
- No existing cooked mesh directory was found in the current workspace scan, so cooked-path verification depends on running packaging later.

Closure notes:
- Verification candidates are now identified without changing scenes or assets.
- Build/run verification is intentionally left for an explicit build/run request.
- Optional temporary local-only test by hiding an OBJ still requires approval because it changes asset files, even if reversible.

### Batch G. Patch Cleanup And Constraint Check

Progress: 100%

Scope:
- Recheck Batches A-F against the project constraints before any build request.
- Keep this pass limited to naming, documentation, and compile-surface cleanup.

Completed:
- Removed `legacy` wording from the StaticMesh fallback helper and tracking notes. The fallback is now described as the current static mesh cache path because `Asset/Mesh/Bin` is actively used by the current code.
- Rechecked component `PostDuplicate` chain. StaticMesh, Primitive, Shape, Light, Movement, Script, and related components route through `UActorComponent::PostDuplicate()`.
- Rechecked UUID validation call sites. `FindByUUID()` is no longer used by selection or gizmo target validation.
- `git diff --check` passed with only CRLF conversion warnings.

Remaining:
- None before build/manual verification.

### Batch H. Final Pre-Build Static Verification

Progress: 100%

Scope:
- Validate patched assumptions that can be checked without running a build or editing assets.
- Keep build/run and asset mutation out of scope unless explicitly requested.

Completed:
- Rechecked `FPaths::ToAbsolute()`: absolute paths are returned unchanged, while relative paths are rooted at `FPaths::RootDir()`. The `RuntimeFileExists()` change is consistent with existing path policy.
- Confirmed scene candidates still exist:
  - `NipsEngine/Asset/Scene/Intro.Scene`
  - `NipsEngine/Asset/Scene/Main.Scene`
  - `NipsEngine/Asset/Scene/Playing.Scene`
- Confirmed static mesh fallback candidates still exist:
  - `NipsEngine/Asset/Mesh/cyberpunk_katana/katana.obj`
  - `NipsEngine/Asset/Mesh/Bin/katana.bin`
  - `NipsEngine/Asset/Mesh/cyberpunk_arm/cyber_arm_right_small.obj`
  - `NipsEngine/Asset/Mesh/Bin/cyber_arm_right_small.bin`
  - `NipsEngine/Asset/Mesh/Dice/Dice.obj`
  - `NipsEngine/Asset/Mesh/Bin/Dice.bin`
- Confirmed `NipsEngine/Asset/Cooked/Mesh` is absent and `NipsEngine/Asset/Mesh/Bin` exists in this workspace.
- Final `git diff --check` passed with only CRLF conversion warnings.

Remaining:
- Build/compiler verification when explicitly requested.

### Batch I. Pending Follow-Up Cleanup

Progress: 80%

Scope:
- PIE Runtime UI and Editor UI composition order.
- Billboard scale inheritance policy.
- RmlUi ThirdParty prebuilt static library pipeline.
- SoLoud ThirdParty prebuilt static library pipeline.

Completed:
- PIE RmlUi now renders before final Editor ImGui draw submission.
- Billboard scale policy flag added and applied consistently to visual render, AABB, raycast, and selection-mask paths.
- `USubUVComponent` opts in to inherited scale, while helper Billboard components default to scale-independent behavior.
- RmlUi direct source compilation removed from the main engine project and replaced with a generated static library path.
- SoLoud direct source compilation removed from the main engine project and replaced with a generated static library path.
- PowerShell script syntax and `NipsEngine.vcxproj` XML parsing passed.
- Debug x64 MSBuild passed after the RmlUi toolchain stamp fix.
- Debug x64 MSBuild passed after the SoLoud static library split.

Remaining:
- Build/compiler verification remains for Editor Release, GameClient Debug, and GameClient Release.
- Manual Editor/PIE verification through the Pending verification steps above.

## Verification Matrix

Use this section before closing any patched bug. Do not mark a bug complete from code inspection alone.

### V1. Editor Duplicate Name/Tag Independence

Coverage:
- Bug 1
- Bug 5 duplicate identity parts
- Bug 7 Editor-side name/tag behavior

Steps:
- In Editor, select one Actor with Actor tags and at least one Component with Component tags.
- Press Ctrl+D.
- Select only the duplicated Actor.
- Edit Actor tags and Component tags on the duplicate.
- Select the source Actor and original Component again.

Expected:
- Source Actor tags do not change.
- Source Component tags do not change.
- Duplicated Actor has an Editor-unique Actor name.
- Duplicated Components keep their copied Component names and independent tags.

If failed:
- Recheck `AActor::PostDuplicate`, `UActorComponent::PostDuplicate`, and Details tag editing multi-selection state.

### V2. Multi-Selection Tag Editing

Coverage:
- Bug 1 user-facing ambiguity

Steps:
- Select multiple Actors.
- Edit Actor tags in Details.

Expected:
- All selected Actors change together. This is intentional multi-selection editing, not tag container sharing.

If failed:
- Recheck `FEditorPropertyWidget::RenderActorTags` behavior before changing duplicate code.

### V3. StaticMesh Binary-Only Runtime Load

Coverage:
- Bug 2

Steps:
- Prepare a scene that references an `.obj` StaticMesh.
- Ensure a valid `.bin` exists in one fallback location.
- Remove or hide the original `.obj`.
- Load the scene in Runtime(GameClient) or packaged output.

Expected:
- Mesh loads from binary without requiring OBJ.
- Log shows `[StaticMeshLoad] Redirect missing OBJ to binary mesh`.
- No OBJ material load is attempted when the source OBJ is missing.

If failed:
- Recheck fallback path order in `FResourceManager::LoadStaticMesh`.
- Verify whether the `.bin` path is cooked, sibling, or current cache.
- Verify packaged scene references were rewritten by `GamePackager`.

### V4. Details Component Selection Stability

Coverage:
- Bug 3
- Bug 5 selection parts

Steps:
- Open a scene with Actors that have multiple Components.
- Select a Component in Details.
- Move cursor into and out of Viewport.
- Save/reopen the scene.
- Repeat after Ctrl+D on the Actor.
- Repeat after starting and stopping PIE.

Expected:
- Details remains on the selected Component unless the user selects the Actor or clears selection.
- Reopening the scene does not randomly switch the same Component selection flow to Actor fallback.

If failed:
- Recheck `FSelectionManager::IsSelectedComponentAlive`.
- Recheck `FEditorPropertyWidget::UpdateSelectionState`.
- Capture whether the selected Component pointer is destroyed, replaced, or only rejected by validation.

### V5. Gizmo Visibility From Outliner And Viewport Hover

Coverage:
- Bug 4
- Bug 5 Gizmo target parts

Steps:
- Select an Actor in Outliner while cursor is outside the Viewport.
- Move cursor into the Viewport without clicking.
- Switch between Select, Translate, Rotate, and Scale modes.
- Repeat after Ctrl+D.
- Repeat after PIE start/stop.

Expected:
- In Translate/Rotate/Scale mode, Gizmo remains visible when cursor enters Viewport.
- In Select mode, Gizmo is hidden consistently before hover and after hover.
- PIE does not show Editor Gizmo.

If failed:
- Recheck `FEditorViewportClient::SyncGizmoVisualState`.
- Recheck `FSelectionManager::SyncGizmo`.
- Recheck Gizmo target validation when Editor and PIE worlds both exist.

### V6. Gizmo Drag Cursor Release

Coverage:
- Bug 6

Steps:
- Start dragging a Gizmo handle.
- Release normally.
- Start dragging again and change selection or delete the target if possible during/around the drag.
- Switch transform mode during drag and after release.

Expected:
- Cursor is not trapped after Gizmo disappears or target becomes invalid.
- `IsHolding()` and `IsPressedOnHandle()` are cleared after deactivation.
- `IsHolding()` and `IsPressedOnHandle()` are also cleared when selection changes to a new Gizmo target.
- Gizmo can appear again on the next valid selection.

If failed:
- Recheck `UGizmoComponent::Deactivate`.
- Recheck `FEditorViewportClient::WantsAbsoluteMouseClip`.
- Recheck input router mouse release/capture state.

### V7. PIE And Runtime Name/Tag Preservation

Coverage:
- Bug 7

Steps:
- In Editor, set distinct Actor names, Component names, Actor tags, and Component tags.
- Start PIE.
- Inspect logs/debug UI/Lua-visible data for the PIE world.
- Stop PIE and confirm Editor data is unchanged.
- Load the same scene in Runtime(GameClient).

Expected:
- PIE world keeps Editor scene Actor/Component names and tags at session start.
- Runtime(GameClient) restores names and tags from serialized scene data.
- Editor Ctrl+D unique naming does not affect PIE world duplication policy.

If failed:
- Recheck `AActor::PostDuplicate`, `UActorComponent::PostDuplicate`, `UWorld::PostDuplicate`, and `FActorSerialization::SpawnActorFromJson`.

### V8. Viewport Transform Shortcut Routing

Coverage:
- Bug 8

Steps:
- Click or edit a Details/Outliner text field, then stop editing.
- Move the cursor over the Viewport without clicking it.
- Press `Q/W/E/R`, `1/2/3/4`, and `Space`.
- Repeat while an actual text input is focused.

Expected:
- When the cursor is over the Viewport and ImGui/text input is not capturing keyboard, transform/gizmo mode changes immediately.
- When a text field is focused, typing is captured by the text field and viewport mode does not change.

If failed:
- Recheck `FInputRouter::FinalizeAndDispatchInput()` keyboard block masking.
- Recheck `FEditorMainPanel::Update()` ImGui keyboard capture state.

## Decision Log

- 2026-05-05: Created this tracker from the current bug report. First pass will start with Batch A, Bug 1.
- 2026-05-05: Chose Option A for duplicate name policy. Engine duplicate preserves Actor name; Editor Ctrl+D renames duplicates uniquely.
