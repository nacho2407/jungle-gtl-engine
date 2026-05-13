# Feature Refactor Batch Plan

## Goal

This pass cleans up recently added editor features without changing their intended behavior.

Focus:

- Keep current feature behavior.
- Clarify ownership and routing.
- Remove duplicate helper code.
- Reduce fragile viewer/editor coupling.
- Leave larger file splits for a later safe batch.

## Batches

### Batch 0 - Record Scope

- [x] Write the refactor scope and batch order in this md.

Done when:

- The current batch plan is visible in the repo.

### Batch 1 - Small Safety Fixes

- [x] Viewer render loop skips only the broken viewer instead of returning from the whole viewer render pass.
- [x] Remove debug destructor output from `FWorldContext`.

Done when:

- Build passes.
- One invalid viewer cannot stop all remaining viewers from rendering.

### Batch 2 - Viewer Tab ID / Label Rule

- [x] Remove duplicated `MakeViewerTabId` helpers.
- [x] Centralize viewer tab id and label generation.

Done when:

- `EditorMainPanelTabs.cpp` and `EditorMainPanelWidgetSetup.cpp` use the same tab id/label helper.
- Open, close, detach, and routing paths share the same viewer tab identity rule.

### Batch 3 - Viewer Toolbar Duplication

- [x] Route active document viewer toolbar drawing through the existing shared helper.

Done when:

- Embedded/detached viewer toolbar behavior is still the same.
- The large duplicated viewer toolbar block is removed from the active document toolbar path.

### Batch 4 - Viewer Selection Ownership

- [x] Move bone/socket selection state ownership to `FEditorViewer`.
- [x] Keep `FEditorViewerWindowWidget` as a UI/controller layer that reads selection and sends commands.
- [x] Clear bone/socket/gizmo selection consistently when empty space or deleted sockets invalidate selection.

Done when:

- `FEditorViewerWindowWidget` no longer owns separate selected bone/socket indices.
- Selection mutation flows through `FEditorViewer` methods.

### Batch 5 - Verification

- [x] Build Debug x64.
- [x] Summarize changed areas.
- [x] Record remaining follow-up candidates.

Result:

- `MSBuild NipsEngine.sln /p:Configuration=Debug /p:Platform=x64 /m` passed with 0 warnings and 0 errors.
- Viewer tab identity, toolbar routing, and viewer selection ownership are now less duplicated.
- Bone/socket selection state is owned by `FEditorViewer`; the widget only reads it or sends selection commands.

### Batch 6 - Skeletal Viewer Viewport Client Split

- [x] Move `FSkeletalMeshViewportClient` input/bone-pick helper implementations out of the header.
- [x] Keep the public API small: show flags, pick handler setter, `ProcessInput`.
- [x] Reduce header dependencies on gizmo, scene viewport, and input details.

Done when:

- `SkeletalMeshViewportClient.h` only declares behavior.
- `SkeletalMeshViewportClient.cpp` owns the implementation.
- Build passes.

### Batch 7 - Viewer Widget Small API Cleanup

- [x] Remove direct viewer internals from the widget where a small helper/read API is enough.
- [x] Keep widget methods null-safe around viewer shutdown/detach.

Done when:

- Viewer widget remains a UI/controller layer.
- Build passes.

### Batch 8 - Verification

- [x] Build Debug x64.
- [x] Summarize changed areas.
- [x] Record remaining follow-up candidates.

Result:

- `MSBuild NipsEngine.sln /p:Configuration=Debug /p:Platform=x64 /m` passed with 0 warnings and 0 errors.
- `FSkeletalMeshViewportClient` now has a small header and owns input/bone-pick details in its `.cpp`.
- Socket selection now goes through `FEditorViewer::SelectSocket(int32)`, so the widget no longer passes socket names into gizmo/proxy setup.

### Batch 9 - Legacy API Sweep

- [x] Search for legacy/deprecated/backward-compatibility API surfaces across the current implementation.
- [x] Separate compatibility code that must stay from obsolete APIs that can be removed now.
- [x] Remove the safe obsolete API surface and migrate internal call sites.

Done when:

- Obsolete APIs are not kept only for old call-site convenience.
- Intentional compatibility paths remain documented as retained.
- Build passes.

Candidates:

- `UDecalComponent` legacy no-slot material accessors were migrated to slot-based access and removed.
- `SceneSaveManager` legacy primitive conversion should stay for old scene compatibility.
- Shader path/cache backward compatibility should stay while old assets may exist.
- ViewportClient legacy event hooks need a wider input migration pass, so do not remove in this batch.

### Batch 10 - Legacy Verification

- [x] Build Debug x64.
- [x] Summarize removed API and retained compatibility paths.

Result:

- `MSBuild NipsEngine.sln /p:Configuration=Debug /p:Platform=x64 /m` passed with 0 warnings and 0 errors.
- Removed the C++ convenience API `UDecalComponent::SetMaterial(UMaterialInterface*)` / `UDecalComponent::GetMaterial()`.
- Updated engine, render, actor, and Lua binding call sites to use slot 0 explicitly.
- Retained scene/material/settings compatibility code because those paths protect older saved data or assets.

### Batch 11 - Editor Main Panel Tab Routing Split

- [x] Move viewport routing and detached viewer visibility logic out of `EditorMainPanelTabs.cpp`.
- [x] Keep tab strip, document menu, toolbar, and tab actions in the existing tabs file for now.
- [x] Keep routing behavior identical: active level tab owns level input; detached viewer windows can own viewer input by focus/hover; z-order remains ImGui-window based.

Done when:

- `EditorMainPanelTabs.cpp` no longer owns ImGui window hierarchy helpers for routing.
- New routing file is registered in the Visual Studio project.
- Build passes.

### Batch 12 - Tabs Split Verification

- [x] Build Debug x64.
- [x] Check that split did not change public routing APIs.
- [x] Record remaining large-file split candidates.

Result:

- `MSBuild NipsEngine.sln /p:Configuration=Debug /p:Platform=x64 /m` passed with 0 warnings and 0 errors.
- Added `EditorMainPanelTabRouting.cpp` for level/viewer viewport visibility, input routing, detached viewer focus/hover checks, and viewer z-order lookup.
- `EditorMainPanelTabs.cpp` now keeps tab strip, document toolbar/menu, and tab action handling.
- `imgui_internal.h` remains in `EditorMainPanelTabs.cpp` because the tab label ellipsis rendering still uses ImGui internal text helpers.

### Batch 13 - Editor Document Chrome Split

- [x] Move active document toolbar and document-specific main menu out of `EditorMainPanelTabs.cpp`.
- [x] Keep public call sites unchanged: toolbar, detached viewer window, and menu provider still call the same `FEditorMainPanel` methods.
- [x] Keep viewer toolbar controls shared between docked and detached viewer windows.

Done when:

- `EditorMainPanelDocumentChrome.cpp` owns active document toolbar/menu rendering.
- `EditorMainPanelTabs.cpp` is focused on tab strip and tab state actions.
- Build passes.

### Batch 14 - Document Chrome Verification

- [x] Build Debug x64.
- [x] Check that active document menu/toolbar APIs remain stable.
- [x] Record remaining split candidates.

Result:

- `MSBuild NipsEngine.sln /p:Configuration=Debug /p:Platform=x64 /m` passed with 0 warnings and 0 errors.
- Added `EditorMainPanelDocumentChrome.cpp` for active document toolbar, viewer toolbar controls, and document-specific main menu.
- `EditorMainPanelTabs.cpp` now focuses on tab strip rendering and tab state actions.

## Deferred Items

- Consider splitting tab actions (`ActivateEditorTab`, close, detach) into a small tab actions file if the tab manager grows further.
- Consider a dedicated viewer selection model if mesh/socket/bone editing grows beyond the current previewer scope.
