# Week06 Input/UX Migration Plan

## Goal

Bring the richer Week06_Team3 input and viewport UX architecture into the current Week09 engine without losing existing Week09 rendering, editor, scene, and viewport behavior.

The migration should be tracked here so work can resume safely after context compaction.

## Source Project

- Week06 source: `C:/Users/jungle/Desktop/YG/Week06/Projects/Week06_Team3/KraftonEngine`
- Week08 content-browser reference: `C:/Users/jungle/Desktop/YG/Week08/Projects/GTL_Week8_Team7/KraftonEngine`
- Current target: `C:/Users/jungle/Desktop/YG/Week09/Project/Week09_Team03_Engine/NipsEngine`

## Migration Strategy

Do not copy the whole Week06 editor at once. Move in layers and keep the engine buildable after each layer.

1. Input core
2. Viewport input routing
3. Editor viewport action mapping
4. Editor viewport tools
5. ImGui/viewport capture UX
6. Layout and pane UX
7. PIE routing and game viewport control
8. Final cleanup and regression pass

## Current Week09 Baseline

- Input is mostly `InputSystem` polling with `CurrentStates[256]` and `PrevStates[256]`.
- `EditorViewportClient::TickInput` directly reads `InputSystem`.
- `EditorInputRouter` routes to `EditorWorldController` or `PIEController`.
- `SViewport`, `FSceneViewport`, and `FViewportClient` have event-style hooks, but much of the real input path bypasses them.
- ImGui capture is handled from `EditorMainPanel`, and viewport operation state is handled by `ViewportLayout`.

## Week06 Features To Preserve

### Input Core

- [x] `FInputSystemSnapshot`
- [x] `FInputEvent`
- [x] `FInputFrame`
- [x] `FViewportInputContext`
- [x] `FInteractionBinding`
- [x] `EInteractionDomain`: Editor, PIE, EditorOnPIE
- [x] `EInputBindingTrigger`: Pressed, Down, Released, EventType
- [x] `FInputBinding`
- [x] raw mouse support through `WM_INPUT`
- [x] GUI mouse, keyboard, and text-input capture state
- [x] focus-loss input reset

### Input Router

- [x] target viewport registration
- [x] hovered viewport tracking
- [x] focused viewport tracking
- [x] captured viewport tracking
- [x] ImGui mouse/keyboard block mask
- [x] relative mouse mode
- [x] absolute mouse clip
- [x] raw mouse delta integration
- [x] dispatch to `FViewportClient::ProcessInput`

### Editor Viewport Actions

- [x] mode cycle
- [x] select mode
- [x] translate mode
- [x] rotate mode
- [x] scale mode
- [x] gizmo mode cycle
- [x] coordinate space toggle
- [x] focus selection
- [x] delete selection
- [x] select all
- [x] duplicate selection
- [x] new scene
- [x] load scene
- [x] save scene
- [x] save as
- [x] PIE possess/eject toggle
- [x] end PIE
- [x] primary select
- [x] toggle select
- [x] additive select
- [x] box select replace
- [x] box select additive
- [x] alt-left orbit
- [x] alt-right dolly
- [x] alt-middle pan
- [x] right mouse look
- [x] middle mouse pan
- [x] WASDQE movement
- [x] arrow-key rotation
- [x] wheel scroll/dolly/zoom

### Editor Viewport Tool Stack

- [x] command context
- [x] gizmo context
- [x] selection context
- [x] navigation context
- [x] context priority ordering
- [x] command tool
- [x] gizmo tool
- [x] selection tool
- [x] navigation tool
- [x] transform modes: select, translate, rotate, scale

### Viewport UX

- [x] active viewport outline
- [x] viewport pane toolbar dead zone
- [x] viewport image toolbar buttons
- [x] transform snapping toolbar buttons
- [x] translate/rotate/scale snapping during gizmo drag
- [x] mouse release to viewport when hovering viewport content
- [x] block viewport input when interacting with non-viewport ImGui
- [x] suppress viewport mouse after popup closes until buttons release
- [x] selection marquee rendering
- [x] gizmo drag cursor clipping
- [x] relative mouse cursor hiding/locking
- [x] restore cursor on relative mouse exit
- [x] right-click viewport context menu
- [x] right-click short click does not immediately enter relative mouse warp
- [x] right-click context menu pre-selects the clicked actor
- [x] right-click context menu supports Place Actor entries
- [x] right-click context menu orders Place Actor/Delete first like Week06 while preserving Week09 extras

### UI Layout / Design Parity

- [x] Week06-style dark ImGui palette and rounded controls
- [x] Week06 editor icon assets copied into Week09 asset tree
- [x] Week06-style top editor toolbar with Add Actor icon
- [x] Week06-style top toolbar play/pause/stop icon buttons
- [x] main menu text play controls removed to reduce visual mismatch
- [x] Week06-style main menu -> 40px toolbar -> dockspace -> 32px footer render order
- [x] Week06-style bottom console drawer with animated open/close
- [x] Week06-style footer console input and status/domain line
- [x] View menu Console entry renamed/repurposed as Console Drawer
- [x] footer console button uses filled triangle glyphs like Week06
- [x] viewport toolbar moved from menu-bar composition to Week06-style pane overlay composition
- [x] viewport toolbar uses custom drawn icon/text buttons instead of default ImGui image buttons
- [x] viewport Type/View/Stats/Layout controls preserved as right-side toolbar popups
- [x] viewport toolbar right-side controls align to the pane edge like Week06
- [x] viewport toolbar Split/Merge control restored as a direct right-side button
- [x] viewport settings render as focused-viewport overlay instead of normal dock/window
- [x] full Week06 debug/stat overlay parity within the active migration scope. Shortcut overlay and grouped ImGui stat/debug overlays are migrated; render-pipeline text-line overlay is excluded by request.

### Layout UX

- [x] one pane
- [x] two panes horizontal
- [x] two panes vertical
- [x] three panes left
- [x] three panes right
- [x] three panes top
- [x] three panes bottom
- [x] four panes 2x2
- [x] four panes left
- [x] four panes right
- [x] four panes top
- [x] four panes bottom
- [x] animated layout transitions
- [x] split/merge toggle
- [x] layout preset image grid
- [x] layout settings persistence
- [x] active viewport preservation across layout changes
- [x] hidden viewport rect zeroing to prevent stale hit targets

### PIE UX

- [x] PIE request queue
- [x] PIE possessed mode
- [x] PIE ejected mode
- [x] viewport domain routing to game viewport client
- [x] PIE entry viewport tracking
- [x] PIE start outline flash
- [x] end PIE from input
- [x] restore editor viewport behavior after PIE

## Target Week09 Integration Points

- `NipsEngine/Source/Engine/Input/InputSystem.*`
- `NipsEngine/Source/Engine/Input/InputTypes.h`
- `NipsEngine/Source/Engine/Input/InputBinding.h`
- `NipsEngine/Source/Engine/Input/InputRouter.*`
- `NipsEngine/Source/Engine/Input/CursorControl.h`
- `NipsEngine/Source/Engine/Runtime/WindowsApplication.*`
- `NipsEngine/Source/Engine/Runtime/ViewportClient.*`
- `NipsEngine/Source/Editor/Input/*`
- `NipsEngine/Source/Editor/Viewport/*`
- `NipsEngine/Source/Editor/UI/EditorMainPanel.*`
- `NipsEngine/Source/Editor/EditorEngine.*`

## Compatibility Notes

- Week09 has `Engine/Slate`, `SViewport`, and `FSceneViewport`; Week06 uses a different `Engine/Viewport` model.
- Preserve Week09 render pipeline and scene viewport render-target ownership unless a specific Week06 feature requires a controlled adaptation.
- Prefer adapting Week06 input concepts into Week09's current viewport classes over wholesale replacing renderer-facing viewport resources.
- Keep old Week09 input APIs temporarily as compatibility wrappers until all consumers move to `FViewportInputContext`.

## Open Audit: Week06 Parity / Week09 Preservation

### Week06 Features Still Needing Parity Checks

- [x] Week06-style shortcut overlay parity. Shortcuts now render as a dimmed full-screen overlay with centered fixed panel and blocker input surface.
- [x] Full Week06 debug/stat overlay parity within the active migration scope. Week09 has Week06-style shortcut, grouped stats, and editor debug overlays; render-pipeline text-line overlay and `RenderHiZDebug` are explicitly excluded by request.
- [x] Week06-style grouped stat overlay. Implemented as a focused-viewport ImGui overlay that preserves Week09 culling/decal/light/shadow/memory stats.
- [x] Week06 `FOverlayStatSystem` render-pipeline text-line parity excluded by request. Grouped ImGui overlay parity remains the supported migrated path.
- [x] Week06-style `RenderEditorDebugPanel` and `Place Actors (Grid)` parity. Implemented as an Editor Debug panel using Week09 viewport settings/show flags and existing primitive spawn list.
- [x] Viewport toolbar polish pass implemented. Snap toggle/value buttons and Layout/Split controls now use Week06-style paired rounding, selected button colors are aligned, layout icon popup spacing is tightened, and OnePane camera speed control is restored; runtime visual comparison remains in QA.
- [x] Viewport settings overlay content parity. Overlay now includes Week06-style View/Show/Grid/Camera grouping and preserves Week09 view modes, light culling, shadow filter, BVH, and shadow previews.
- [x] Week06-style OnePane viewport camera speed toolbar control. Implemented as a right-side `Cam` dropdown that edits the focused viewport move speed and folds into the overflow menu when narrow.
- [x] Main menu parity pass. Top menu now follows Week06-style `File/View/Settings/Help` grouping while preserving Week09 Material/Stat and cache actions.
- [x] PIE editor-window hide/restore behavior. PIE now snapshots visible panels/viewport overlays, hides editor-only panels during play, and restores the previous state after PIE.
- [x] PIE fullscreen viewport toggle. When enabled, PIE snapshots the current viewport layout, maximizes the active viewport as OnePane, and restores the previous layout after PIE or when toggled off.
- [x] Week06 editor debug utilities/place-grid/debug panels covered through `RenderEditorDebugPanel`; `RenderHiZDebug` excluded by request.

### Week09 Features To Preserve During Further Week06 Refactor

- [x] Material Editor dock/window and material workflows.
- [x] Jungle Property Window behavior and selected-object editing.
- [x] Scene Manager and Stat Profiler panels.
- [x] Week09 File/asset actions from the existing toolbar/menu layer.
- [x] Shadow atlas/cube preview controls in Viewport Settings.
- [x] Light culling modes: Clustered, Tiled, None.
- [x] Week09 view modes: Heatmap, Depth, Normal.
- [x] Existing layout/settings persistence in `Editor.ini` / `imgui.ini`.
- [x] Current Week09 Add Actor primitive/light entries such as Fireball, DecalSpotLight, and light actors.

### Recently Observed Regressions

- [x] Cursor could remain invisible or flicker while idle because cursor show/hide normalization was called every frame. Fixed by making cursor ownership cleanup transition-based.
- [x] Viewport toolbar right-side controls could overlap or clip in narrow panes. Fixed by folding Type/View/Stats/Settings/Layout/Split into a Week06-style overflow menu when the row is too tight.

### New QA / Feature Follow-up Scope

- [x] PIE fullscreen toggle must reliably apply On/Off during PIE and restore the previous layout when disabled or when PIE ends. Restore is now independent from panel visibility snapshot state and PIE fullscreen uses animated OnePane transition.
- [x] PIE viewport toolbar must switch to the limited Week06 PIE toolbar surface instead of showing the full editor toolbar.
- [x] Single Viewport <-> Multi Viewport animated transition should be smoothed against Week06 timing/easing behavior.
- [x] RMB context menu should remove duplicate actions and disable selection-only actions when no actor is picked.
- [x] LMB viewport drag should support UE-style camera behavior: horizontal drag rotates, vertical drag moves along the look direction.
- [x] Camera movement/rotation interpolation should be migrated from Week06 and exposed through Editor Debug/Settings controls.
- [x] Editor Debug Setting panel should absorb Week06 grid actor creation and useful Week09 debug/settings controls.
- [x] Jungle Control Panel should be slimmed down: move everything except Camera Location/Rotation into the Week06-style debug/settings panel; remove World/Local, gizmo transform, and actor spawn controls there.
- [x] Viewport input should become more strictly consumed by active interaction state to prevent broadcast-like duplicate handling, e.g. QWER mode changes should not also feed movement.
- [x] Camera Move/Rotation Smooth Speed needs retuning. Exposed ranges are now doubled to 40 so values around 20 are reachable without internal compensation.
- [x] Camera Speed, Move Sensitivity, and Rotate Sensitivity should be unified into one coherent camera speed system so sensitivity does not make the original speed feel ineffective. Editor movement now uses per-viewport move speed directly, while rotation uses Camera Rotate Speed.
- [x] Viewport toolbar `Cam` control should use the unified camera speed system and display the value as a multiplier, e.g. `nx` from the default speed.
- [x] RMB-down + mouse wheel should adjust camera speed through the unified camera speed system.
- [x] LMB drag horizontal camera rotation is currently not working and should be fixed in the LMB navigation path. The LMB path now updates both camera rotation/location and the smoothing targets.
- [x] Mouse wheel should dolly the camera by default instead of changing FOV.
- [x] Floating editor windows/popups such as Editor Debug and viewport layout selection must render above focused-viewport outline overlays instead of being covered by them.
- [x] Viewport Toolbar's Viewport Settings overlay should be movable.
- [x] Single <-> Multi viewport transition animation still needs a Week06 comparison pass: the upper-left viewport path behaves oddly while the other viewport transitions look acceptable. The focused viewport is now drawn last/top during viewport image and toolbar passes to remove the upper-left overlap artifact.
- [x] Console helper should support recommendations, recommended command listing, and command/help listing.
- [x] Content Browser should be added with Ctrl+Space animation, based on Week08, including drag-and-drop oriented asset workflows.
- [x] Content Browser should support Console-style bottom drawer presentation plus a movable floating window mode, with Console/Content mutual exclusion so the most recently opened drawer wins.
- [x] Content Browser visible roots should be limited to project `Asset` and `Shaders` folders.
- [x] Material Editor should absorb Week7-style preview affordances safely. Full offscreen 3D preview requires Week7 preview-world/render-target infrastructure that Week09 does not currently expose, so the first safe pass adds material color/parameter/texture thumbnail preview inside the existing editor.

## Verification Checklist

### Build

- [x] Project generation still succeeds.
- [x] Full solution build succeeds.
- [x] No newly introduced include cycles.
- [x] No unresolved external symbols from copied Week06 files.

### Input

- [ ] keyboard pressed/down/released works. Implemented; runtime QA pending.
- [ ] mouse pressed/released works. Implemented; runtime QA pending.
- [ ] wheel works. Perspective Dolly and RMB+wheel speed adjustment are implemented; runtime QA pending.
- [ ] left/right drag start/end works. Implemented; runtime QA pending.
- [ ] raw mouse works in relative mode. Implemented; runtime QA pending.
- [ ] focus loss releases stuck keys/buttons. Implemented; runtime QA pending.
- [ ] Shift+F1 releases PIE mouse focus and viewport click reacquires it. Implemented; runtime QA pending.
- [ ] ImGui text input blocks viewport keyboard. Implemented; runtime QA pending.
- [ ] ImGui panels block viewport mouse. Implemented; runtime QA pending.
- [ ] viewport content receives keyboard/mouse when hovered. Implemented with viewport-content capture release; runtime QA pending.

### Viewport

- [ ] active viewport switches on click.
- [ ] gizmo hover works.
- [ ] gizmo drag works.
- [ ] actor selection works.
- [ ] actor click selection happens on non-drag LMB release. Implemented; runtime QA pending.
- [ ] additive/toggle selection works.
- [ ] box selection works.
- [ ] camera right-drag look works. Camera smoothing/sensitivity is implemented; runtime QA pending.
- [ ] middle pan works.
- [ ] alt-left orbit works.
- [ ] alt-right dolly works.
- [ ] alt-middle pan works.
- [ ] WASDQE movement works.
- [ ] arrow rotation works.
- [ ] wheel zoom/dolly works. Perspective Dolly and orthographic zoom are implemented; runtime QA pending.

### UX

- [ ] viewport toolbar does not leak input into scene.
- [ ] viewport image toolbar buttons switch tools/layouts.
- [ ] View menu and viewport Stats menu can toggle Week06-style grouped stat overlay. Implemented; runtime QA pending.
- [ ] View menu can toggle Week06-style Editor Debug panel and grid actor placement. Implemented; runtime QA pending.
- [ ] viewport snapping toolbar buttons quantize gizmo motion. Implemented; runtime QA pending.
- [ ] top toolbar Add Actor popup spawns primitives. Implemented; runtime QA pending.
- [ ] right-click Place Actor popup spawns at clicked viewport point. Implemented; runtime QA pending.
- [ ] bottom console drawer opens/closes with footer button and backtick cycle. Implemented; runtime QA pending.
- [ ] non-viewport ImGui windows do not leak input into scene.
- [x] popup close does not click-through into viewport. Implemented with viewport mouse blocking until button release.
- [ ] cursor hides and restores correctly.
- [ ] cursor restores after RMB/MMB release without legacy double-hide. Implemented; runtime QA pending.
- [ ] cursor show/hide counter is normalized to prevent invisible cursor drift. Implemented; runtime QA pending.
- [ ] cursor does not flicker while idle. Implemented; runtime QA pending.
- [x] cursor restores to its pre-lock screen position after relative mouse/viewport lock ends, preventing center-position drift.
- [ ] viewport toolbar right controls overflow instead of overlapping in narrow panes. Implemented; runtime QA pending.
- [ ] viewport toolbar overflow threshold keeps the right group from overlapping the left tools during MultiViewport resize. Implemented; runtime QA pending.
- [ ] viewport toolbar buttons are vertically centered in the 34px pane toolbar. Implemented; runtime QA pending.
- [ ] viewport toolbar left group has Week06-like light left padding and right group is positioned independently from left-flow cursor state. Implemented; runtime QA pending.
- [ ] Win32 client cursor restores to arrow after viewport drag/relative cursor release instead of keeping hidden or resize cursor shape. Implemented; runtime QA pending.
- [x] hidden viewport slots do not receive input. Hidden slots are zero-sized and ignored by hover/target routing.

### PIE

- [x] start PIE from active viewport.
- [x] possess/eject toggle works.
- [x] possess/eject shortcut is mapped to F8; legacy F4 handling has been removed.
- [x] end PIE shortcut works.
- [x] editor input resumes after PIE. Cursor, panel visibility, and viewport layout restore paths are implemented; runtime QA still recommended.
- [x] active viewport state remains correct. Active PIE viewport tracking and fullscreen layout restore preserve the previous focused viewport.
- [ ] PIE fullscreen viewport toggle enters OnePane for the active viewport and restores the previous layout after PIE. Restore path was hardened and animated OnePane apply is implemented; runtime QA pending.
- [ ] PIE fullscreen off preserves the current editor panel/layout state instead of maximizing/hiding panels. Implemented; runtime QA pending.
- [ ] Focused viewport outline renders below floating/editor panels. Implemented; runtime QA pending.
- [ ] Toolbar Place Actor spawns at origin, while viewport RMB Place Actor spawns on a Week06-style plane in front of the camera. Implemented; runtime QA pending.
- [ ] Material Editor renders a real 3D material preview through a Week09 offscreen render target. Implemented; runtime QA pending.
- [ ] Content Browser tiles show image/material thumbnails, extension labels, folder icons, back/up navigation, and bottom-right tile sizing without clipping the drawer footer. Implemented; runtime QA pending.
- [ ] Double-clicking a material asset in Content Browser opens it in Material Editor asset-edit mode. Implemented; runtime QA pending.
- [ ] PIE Eject behaves like returning to editor-style interaction while PIE continues. Runtime QA pending.
- [ ] Orthographic viewport input follows UE-style expectations: plain LMB drag is box selection and RMB drag pans instead of perspective look. Implemented; runtime QA pending.
- [ ] Material Editor preview is simplified, base materials are read-only until an instance is created, and preview drag rotates the material sphere instead of orbiting a fixed-lit camera. Implemented; runtime QA pending.
- [ ] Footer uses left-side log status, Content before Console, no layout/domain labels, and shows current scene as a relative `Scene\...` level label. Implemented; runtime QA pending.
- [ ] Content Browser file tiles have clearer spacing, material thumbnail generation is cached/throttled, `.mat` DiffuseMap parameters participate in preview shader permutations, and selecting a StaticMesh while viewing a browser material returns Material Editor to the selected mesh material. Implemented; runtime QA pending.
- [ ] New Scene uses an unsaved `Untitled` level state instead of displaying `Default.Scene`, Save on an unsaved level prompts Save As, and closing the editor asks whether to save dirty unsaved level state. Implemented; runtime QA pending.
- [ ] Footer places Content/Console controls on the left, shows Level immediately after Console, and shows transient editor event logs on the far right for about 5 seconds. Implemented; runtime QA pending.
- [ ] Content Browser header no longer lets the path/search area clip off the right edge. Implemented; runtime QA pending.
- [ ] Scene material serialization prefers Material/MaterialInstance asset paths when available, so StaticMesh material choices are less dependent on transient MTL slot names. Implemented as a safe foundation; full MTL-to-`.mat` promotion remains a separate material-asset batch.
- [ ] StaticMeshComponent owns and exposes its Material Slots in the Property panel; selecting a slot can open that exact slot in Material Editor, slot assignment is scene-dirty, and MaterialInstance creation stays standardized on `.matinst`. Implemented; runtime QA pending.
- [ ] Content Browser treats `.mtl` as an import source rather than a previewable material asset; only `.mat` and `.matinst` get material thumbnails/details/double-click Material Editor behavior. Implemented; runtime QA pending.
- [ ] MTL-imported `newmtl` entries are promoted into local read-only-style base `.mat` assets under `Asset/Material/Auto`, and StaticMesh material slot rows have separators plus hover preview thumbnails. Implemented; runtime QA pending.
- [ ] Material Editor no longer owns slot lists or slot assignment UI; it edits one opened material asset or the one material from a StaticMesh slot `Edit` action, and `Create Instance` writes a local `.matinst` that can be assigned back to that slot. Implemented; runtime QA pending.
- [ ] Property Material Slot `Edit` brings the Material Editor tab/window to focus, Content Browser material preview cache is cleared safely after navigation, and material previews use a Week7-style UV sphere instead of the old UV-less sphere. Implemented; runtime QA pending.
- [ ] Content Browser material tiles/details no longer draw live/offscreen rendered Material SRVs; grid tiles use lightweight `.mat`/`.matinst` icons and Details shows color/texture parameters only. 3D Material preview remains Material Editor-only until a thumbnail manager/disk-cache path is added. Implemented; runtime QA pending.
- [ ] Dragging `.obj` or `.bin` mesh items from Content Browser onto an editing viewport spawns a StaticMeshActor at the drop point. `.bin` remains a cache path but can be dropped directly for convenience. Implemented; runtime QA pending.
- [ ] Content Browser opens to the Asset root by default, Stat Profiler/Jungle Control Panel are hidden by default, Scene Manager/Property labels are renamed to Outliner/Details, Outliner supports RMB Place Actor/Delete plus Ctrl+A select-all, Console helper aliases are hardened, and Content Browser RMB supports Create Folder/Text/Material plus Delete. Implemented; runtime QA pending.
- [ ] Content Browser drawer interactions block viewport mouse routing while UI items, popups, or drag/drop are active; `.bin` mesh drops open binary files through wide filesystem paths; Outliner context menu stays open after mouse movement; Content Browser supports guarded Rename without global reference remapping yet. Implemented; runtime QA pending.
- [ ] Multi-viewport toolbar overflow accounts for the actual left-group endpoint before collapsing the right group, and focused/hovered viewport outlines are drawn directly over the viewport image instead of a foreground/panel overlay. Implemented; runtime QA pending.
- [ ] Toolbar, viewport RMB, and Outliner Place Actor menus use the same `FEditorControlWidget` menu/spawn path with an `Empty Actor` entry and grouped separators. Implemented; runtime QA pending.
- [ ] Empty Actor now carries an editor-only billboard icon, Fog Actor is available through the shared Place Actor path with a real `UHeightFogComponent` plus billboard, and Console auto-suggestion overlay no longer steals keyboard focus from the input while appearing. Implemented; runtime QA pending.
- [ ] Viewport toolbar no longer shows Focus Selection, Outliner double-click focuses the selected actor like `F`, and Content Browser mesh drag/drop uses a release-over-viewport fallback instead of relying on ImGui image drag-drop target ordering. Implemented; runtime QA pending.
- [ ] Top editor toolbar has a Save button using `Asset/Editor/ToolIcons/Save.png` to the left of Place Actor, routed through the existing scene save flow. Implemented; runtime QA pending.
- [ ] Viewport toolbar Stats control is removed, Console shows clickable prefix suggestions above the input line, and viewport splitters draw in the viewport host layer instead of foreground so panels/popups remain above them. Implemented; runtime QA pending.
- [ ] Viewport split/merge animation uses the upper-left viewport's left-top as the hidden-rect anchor instead of true screen origin, and the grouped FPS/stat overlay is shifted down by 5px to clear the toolbar. Implemented; runtime QA pending.
- [ ] During viewport split/merge transitions, only the focused viewport toolbar is rendered so non-focused viewport toolbars do not clutter the animation. Implemented; runtime QA pending.

## Progress Log

- 2026-04-30: Created migration tracking document after comparing Week09 and Week06 input/UX structures.
- 2026-04-30: Batch 1 migrated input core, raw mouse, cursor control, input router, viewport-client process hook, and central editor viewport target registration. Debug x64 build succeeded.
- 2026-04-30: Batch 2 routed `FEditorViewportClient` through `FViewportInputContext` while suppressing duplicate legacy polling for the same frame. Debug x64 build succeeded with pre-existing warnings only.
- 2026-04-30: Batch 3 added Week06 editor viewport action mapping and wired mapped shortcuts that already have Week09 engine endpoints. Debug x64 build succeeded.
- 2026-04-30: Batch 4 wired mapped file commands, duplicate selection, PIE possess/eject, Shift additive selection, and box-select click-through suppression. Debug x64 build succeeded.
- 2026-04-30: Batch 5 added ordered editor viewport input contexts and routed `FViewportInputContext` through command, gizmo, selection, and navigation context stages. Debug x64 build succeeded.
- 2026-04-30: Batch 6 promoted viewport input contexts into concrete command/gizmo/selection/navigation tools and added Alt+LMB orbit, Alt+RMB dolly, and Alt+MMB pan. Debug x64 build succeeded.
- 2026-04-30: Batch 7 added viewport transform mode state for select/translate/rotate/scale, wired Tab/QWER/1-4 mode switching, and prevented hidden/select-mode gizmos from stealing hover/click/drag input. Debug x64 build succeeded with pre-existing warnings only.
- 2026-04-30: Batch 8 added active/hovered viewport outlines, wired viewport menu bar dead-zone blocking into viewport client mouse routing, confirmed existing marquee and relative cursor hide/restore paths, and kept ImGui capture blocking in the Week06 input router path. Debug x64 build succeeded with pre-existing warnings only.
- 2026-04-30: Batch 9 latched ImGui-captured mouse input until all buttons are released to prevent popup/menu click-through, and enabled absolute cursor clipping while dragging visible gizmo handles. Debug x64 build succeeded.
- 2026-04-30: Batch 10 added Week06-style 12 viewport layout presets, layout menu entries, split/merge toggle, layout persistence, active viewport preservation, and zero rects for hidden slots. Existing 2x2 splitter drag remains intact. Debug x64 build succeeded.
- 2026-04-30: Batch 11 hardened PIE viewport tracking so pause/resume/stop/end-PIE resolve the viewport that entered PIE even if focus moves after play starts. Possess/eject and End PIE input remain routed through mapped actions. Debug x64 build succeeded.
- 2026-04-30: Batch 12 added PIE start outline flash through the viewport overlay and cleaned the remaining layout conversion warning. Debug x64 build succeeded without warnings.
- 2026-04-30: Batch 13 added viewport right-click context menu with right-click-drag separation, transform/selection/layout/view commands, PIE stop entry, and popup-safe behavior through the existing ImGui capture latch. Debug x64 build succeeded.
- 2026-04-30: Batch 14 copied Week06 editor icon assets, added D3D11 SRV loading/release for viewport tool and layout icons, wired image toolbar buttons for transform/coordinate/focus/settings, and added a layout preset icon grid popup. Debug x64 build succeeded.
- 2026-04-30: Batch 15 added Week06-style viewport-content capture release so hovered viewport content receives mouse release/keyboard input, and changed Start/Stop PIE calls into next-tick request slots to avoid running PIE transitions inside UI/input callbacks. Debug x64 build succeeded.
- 2026-04-30: Batch 16 replaced the placeholder animated layout path with rect-based layout transitions for Week09's preset layout system, wired menu/icon layout changes through the animated path, and kept immediate layout mode available for restore/settings paths. Debug x64 build succeeded.
- 2026-04-30: Batch 17 added Week06-style ImGui palette/cursor ownership, transform snap icon controls, gizmo snap quantization, RMB relative-mode gating, and RMB click pre-selection. Debug x64 build succeeded.
- 2026-04-30: Batch 18 added Week06-style top editor toolbar with Add Actor and icon play controls, removed the duplicate text play controls from the main menu bar, converted LMB actor picking to release-based click selection, and added/reordered Place Actor and Delete entries in the viewport RMB menu. Debug x64 build succeeded.
- 2026-04-30: Batch 19 changed the editor render order to Week06's main-menu/toolbar/dock/footer stack, added a Week06-style animated bottom console drawer and footer input/status bar, added a footer log system, and repurposed the View menu Console item as Console Drawer. Debug x64 build succeeded.
- 2026-04-30: Batch 20 fixed editor cursor restoration by removing the legacy double-hide path and hardening `FCursorControl::Clear`, replaced ASCII footer arrows with filled triangle glyphs, converted the viewport pane toolbar from a menu-bar composition into a Week06-style overlay button strip, preserved Type/View/Stats/Layout as toolbar popups, and changed Viewport Settings into a focused viewport overlay. Debug x64 build succeeded.
- 2026-04-30: Batch 21 normalized all editor cursor show/hide calls back to stable Win32 cursor counts, force-cleared cursor ownership after relative mouse deactivation/no-button frames, right-aligned the viewport toolbar's Type/View/Stats/Settings/Layout/Split group like Week06, and restored direct Split/Merge toolbar access. Debug x64 build succeeded.
- 2026-04-30: Batch 22 removed the remaining idle-frame cursor show/clear calls from the routed and legacy viewport paths, made cursor ownership cleanup transition-based, added a Week06-style overflow menu for narrow viewport toolbar rows, and documented remaining Week06 parity / Week09 preservation audit items. Debug x64 build succeeded.
- 2026-04-30: Batch 23 vertically centered viewport toolbar controls, restored the Week06-style default Win32 arrow cursor on the window class/client cursor path, and forced arrow restoration when editor cursor ownership or legacy cursor visibility returns to visible. Debug x64 build succeeded.
- 2026-04-30: Batch 24 added explicit left padding to the viewport toolbar and changed right-side toolbar placement to absolute child-window positioning so Type/View/Stats/Settings/Layout/Split or overflow controls do not depend on the left group's ImGui cursor flow. Debug x64 build succeeded after closing a running `NipsEngine.exe`.
- 2026-05-01: Batch 25 added a Week06-style focused-viewport grouped stat overlay for FPS, culling, decal, light, shadow, and memory information while preserving existing Week09 per-viewport debug stats and shadow previews. Added toggles in the View menu and viewport Stats menus. Debug x64 build succeeded.
- 2026-05-01: Batch 26 added a Week06-style Editor Debug panel with viewport camera tuning, show-flag toggles, and `Place Actors (Grid)` using the existing Week09 primitive/light actor spawn list. Added the panel to the View menu. Debug x64 build succeeded.
- 2026-05-01: Batch 27 excluded `RenderHiZDebug` by request, added PIE editor-panel hide/restore, main menu grouping parity, Week06-style shortcut overlay, Viewport Settings content parity, relative cursor restore-to-entry-position, and PIE fullscreen viewport toggle with layout snapshot/restore. Debug x64 build succeeded.
- 2026-05-01: Batch 28 excluded Week06 `FOverlayStatSystem` render-pipeline text-line parity by request. Remaining meaningful scope is runtime QA plus viewport toolbar visual polish.
- 2026-05-01: Batch 29 finished the remaining viewport toolbar implementation polish: paired snap/layout rounding, Week06 selected colors/layout popup spacing, and OnePane `Cam` speed dropdown including overflow support. Project generation succeeded and Debug x64 build succeeded after regeneration.
- 2026-05-01: Batch 30 addressed the direct QA follow-up items around PIE/RMB/LMB/camera feel: PIE fullscreen now restores independently from panel snapshot state, PIE viewport toolbar is reduced to a Week06-like play surface, RMB selection-only commands are disabled without selection and duplicate Delete was removed, Single/Multi viewport transition duration matches Week06, UE-style LMB drag navigation was added, QWER/WASD camera movement is gated by active viewport navigation state, and Week06 camera smoothing settings were added to Editor Debug. Debug x64 build succeeded.
- 2026-05-01: Added direct QA follow-up pending items for camera speed/sensitivity unification, smoothing range tuning, `Cam` multiplier display, RMB-wheel camera speed adjustment, overlay draw ordering, movable viewport settings, and the upper-left viewport transition anomaly.
- 2026-05-01: Added direct QA follow-up pending items for LMB horizontal rotation not working and default mouse-wheel behavior needing Dolly instead of FOV changes.
- 2026-05-01: Added hypothesis for the Single/Multi viewport animation artifact: focused viewport may need to be drawn last/on top during transitions.
- 2026-05-01: Batch 31 implemented the direct QA follow-up fixes: camera speed now centers on base speed plus per-viewport multiplier, toolbar `Cam` displays `nx`, RMB+wheel changes that multiplier, smoothing ranges now reach 40, LMB drag updates smoothing targets so horizontal rotation is not reverted, perspective wheel uses Dolly instead of FOV, viewport frame overlays render below floating editor windows, Viewport Settings is movable, and focused viewport rendering is last/top during transitions. Debug x64 build succeeded.
- 2026-05-01: Batch 32 added console helper commands: `help`, `commands`/`cmds`, `suggest`/`recommend`, and `clear`. Unknown commands now show closest-match guidance and point users to the helper commands. Debug x64 build succeeded.
- 2026-05-01: Batch 33 slimmed Jungle Control Panel down to camera location/rotation only, and added a Week08-inspired Content Browser with Ctrl+Space animation, View-menu toggle, folder tree, searchable asset grid, tile-size control, details pane, double-click file open/directory navigation, path persistence, and drag payloads for obj/material/png/generic files. Debug x64 build succeeded.
- 2026-05-01: Batch 34 tuned camera navigation feel with editor settings for Dolly Speed Scale (default 0.60x) and Pan Speed Scale (default 3.00x), applied those scales to LMB/Alt drag, wheel dolly, middle/ortho pan paths, removed legacy F4 PIE possess/eject handling in favor of F8, added Shift+F1 PIE mouse-focus release plus click-to-reacquire, expanded the shortcut window, and made the viewport toolbar overflow earlier during narrow MultiViewport resizing. Week09 already has the Week06-style command/gizmo/selection/navigation tool split through `IEditorViewportTool`, so a larger class split is optional cleanup rather than a blocker. Debug x64 build succeeded after closing a running `NipsEngine.exe` that locked the output binary.
- 2026-05-01: Batch 35 changed Content Browser into a Console-style bottom drawer by default, added an in-browser Drawer/Window mode toggle so it can still be used as a movable floating window, enforced last-open-wins behavior between Console and Content Browser, limited browser roots to `Asset` and `Shaders`, added image/material details previews for browser selections, and expanded Material Editor with a safe Week7-inspired preview section for material colors, parameters, and texture thumbnails. Debug x64 build succeeded after closing a running `NipsEngine.exe` that locked the output binary.
- 2026-05-01: Batch 36 fixed the direct QA items: PIE fullscreen off no longer hides editor panels or forces OnePane, focused viewport outlines now draw through a lower-order overlay draw list instead of ImGui foreground so panels/popups are not covered, toolbar/menu Place Actor spawns at the world origin, and RMB viewport Place Actor spawns on a camera-facing plane a fixed distance in front of the camera. Debug x64 build succeeded.
- 2026-05-01: Batch 37 added safe Material Editor 3D preview render infrastructure in the Week09 style: `FRenderer` now owns a reusable offscreen preview render target, `FEditorRenderPipeline` can render a mesh/material pair into it using the existing render bus/pipeline, and Material Editor displays the resulting SRV with orbit drag and wheel zoom. This avoids importing Week7's preview world wholesale while preserving a path to richer preview scenes later. Debug x64 build succeeded after closing a running `NipsEngine.exe` that locked the output binary.
- 2026-05-01: Batch 38 polished Content Browser tiles and asset flow: image thumbnails now use loaded texture SRVs, material tiles use cached snapshots from the Material Editor 3D preview path, file extensions are shown below tile names, folders use a drawn folder icon, navigation gained Back and Up icon buttons, tile Size moved to the asset pane's lower-right control strip to avoid toolbar crowding, and material double-click opens the asset in Material Editor. Added PIE Eject editor-interaction behavior to the runtime QA list. Debug x64 build succeeded.
- 2026-05-01: Batch 39 addressed the follow-up polish: orthographic viewports no longer enter perspective LMB relative-look; plain LMB drag starts box selection while RMB drag keeps ortho pan behavior, Material Editor preview/details were decluttered and base materials now clearly require Create Instance before editing, material preview interaction now rotates the preview mesh under stable lighting, Content Browser tile overlay extension text was removed, material asset resolution was hardened for double-click open, and the footer now shows log status, Content before Console, and the current `Scene\...` level label instead of Domain/Layout text. Debug x64 build succeeded.
- 2026-05-01: Batch 40 tightened Content Browser and Material Editor integration: asset tiles now use larger row/column gaps, material thumbnail baking is limited to one new snapshot per frame while keeping details previews high-priority, `.mat` material loading now preserves `DiffuseMap`/specular/ambient/bump texture flags for preview permutations, legacy `FVector*` param type names are accepted, and Material Editor now yields asset-edit mode when a selected primitive/StaticMesh material is available. Debug x64 compile passed; final link remained blocked by a running `NipsEngine.exe` process holding the output binary.
- 2026-05-01: Batch 41 added unsaved level lifecycle handling and footer polish: New Scene now becomes `Untitled`/unsaved instead of `Default.Scene`, unsaved Save opens Save As, editor close/New/Load prompt for dirty unsaved state, Footer removes `Ready`, shows Level next to Console, and shows transient event logs at the far right. Content Browser toolbar path/search sizing was clamped to avoid header clipping. Scene material serialization now writes base material file paths when available, and ResourceManager can resolve materials by FilePath as well as name. Debug x64 build succeeded.
- 2026-05-01: Batch 42 moved StaticMesh material-slot UX toward component ownership: Property panel now renders `Materials` as per-slot combo rows with an `Edit` action, `Edit` opens the selected component slot in Material Editor, slot assignment marks the level dirty, StaticMesh material changes invalidate render state, ResourceManager exposes combined base/instance material choices, and `.matinst` remains the unified instance extension. MTL-derived base materials still use `newmtl` names until a dedicated MTL-to-`.mat` promotion batch. Debug x64 build succeeded after closing a running `NipsEngine.exe`.
- 2026-05-01: Batch 43 corrected Content Browser material preview semantics: `.mtl` files are no longer treated as previewable/openable material assets, while `.mat` and `.matinst` keep thumbnail/details/double-click Material Editor support. Debug x64 build succeeded.
- 2026-05-01: Batch 44 added local MTL material promotion and slot hover previews: `LoadMaterial(.mtl)` now writes each parsed `newmtl` as an auto `.mat` asset under `Asset/Material/Auto/<mtl>_<material>.mat`, preserving per-section identity while reducing future dependency on raw MTL parsing. StaticMeshComponent material slot property rows now start with a separator and show a small rendered material preview tooltip on hover. Debug x64 build succeeded after closing a running `NipsEngine.exe`.
- 2026-05-01: Batch 45 narrowed Material Editor responsibility to single-material editing only: slot lists and replacement combos were removed from the editor, slot assignment remains in StaticMeshComponent properties, slot `Edit` opens only that slot's current material, and `Create Instance` saves local `.matinst` files while assigning them back to the slot context when applicable. Resource refresh now reloads `.matinst`, and serialized instances prefer parent `.mat` asset paths when available. Debug x64 build succeeded.
- 2026-05-01: Hotfix after Batch 45: fixed a Windows narrow/wide path conversion assertion in the `.matinst` creation path by keeping generated material-instance paths on the wide-string filesystem path route and hardening `FPaths::ToWide` against failed UTF-8 conversion. Debug x64 build succeeded.
- 2026-05-01: Crash-dump follow-up hotfix: latest dump showed startup assertion in `FResourceManager::SerializeMaterial` while auto-promoting MTL materials to `.mat`. Replaced `SimpleJSON` initializer-list assignments for Vector2/3/4 params with explicit JSON arrays, because this JSON library interprets initializer lists as object key/value pairs rather than arrays. Debug x64 build succeeded.
- 2026-05-01: Batch 46 addressed Material Editor/Content Browser follow-ups: Material Slot `Edit` now focuses Material Editor, Content Browser no longer immediately frees material preview SRVs during navigation into folders such as `Asset/Material/Auto`, preview rendering now uses a copied Week7 UV sphere asset (`Asset/Mesh/PreviewSphere.obj`) instead of the old UV-less sphere, and auto-promoted material names are made unique from their source/material stem to reduce duplicate `.mat` identity collisions. Debug x64 build succeeded.
- 2026-05-01: Content Browser material-preview crash hotfix: latest dump again ended in `ImGui_ImplDX11_RenderDrawData`, consistent with an unsafe/problematic rendered Material SRV being consumed by the browser draw list. Removed 3D rendered Material SRVs from Content Browser grid/details; material files now show lightweight icons plus parameter/texture details, while the 3D preview path stays scoped to Material Editor. Debug x64 build succeeded.
- 2026-05-01: Added Content Browser to Viewport mesh drag/drop placement: `.obj` and `.bin` items emit the static-mesh payload, viewport render items accept it only while editing, drop position uses the same camera-facing placement helper as RMB placement, the spawned StaticMeshActor is selected and marks the scene dirty, and ResourceManager can directly load dropped `.bin` cache files. Debug x64 build succeeded after closing a running `NipsEngine.exe` that locked the output binary.
- 2026-05-01: Batch 47 addressed the editor shell UX follow-ups: Content Browser now resets to `Asset` when opened, Stat Profiler and Jungle Control Panel default to hidden, Scene Manager/Property labels are now Outliner/Details, Outliner gained Ctrl+A select-all plus a context menu for origin Place Actor/Delete, Console helper aliases now include `?`, `list`, and `recommendations` with slash-prefix tolerance, and Content Browser context menus can create folders/text/material assets or delete selected content while protecting browser roots. Debug x64 build succeeded.
- 2026-05-01: Batch 48 fixed Content Drawer/Outliner follow-ups: ImGui active items, popups, and drag/drop now keep viewport input capture blocked so the cursor is not pulled into relative mode while dragging from the drawer; `.bin` mesh cache loading now uses wide filesystem paths; Outliner RMB context menu uses a stable popup rather than an item-bound popup; and Content Browser gained guarded Rename support for files/folders while explicitly leaving reference remapping for a future asset-redirector batch. Debug x64 build succeeded after closing a running `NipsEngine.exe`.
- 2026-05-01: Batch 49 fixed viewport toolbar/outline polish: the right toolbar group now collapses based on the left group's actual rendered endpoint instead of the current ImGui cursor flow, and focused/hovered/PIE-flash viewport outlines are drawn in the viewport host draw list immediately after the scene image so they appear over the viewport but below editor panels/popups. Debug x64 build succeeded after closing a running `NipsEngine.exe`.
- 2026-05-01: Batch 50 unified Place Actor menu behavior across toolbar, viewport RMB, and Outliner. The shared menu now keeps the useful `Empty Actor` label and separators while routing all entries through `FEditorControlWidget::SpawnPrimitive`, so Outliner no longer has a divergent manual actor-spawn switch. Debug x64 build succeeded.
- 2026-05-01: Batch 51 removed the viewport toolbar Focus Selection button, added Outliner row double-click focus using the focused viewport client's `FocusSelection()`, and replaced the unreliable Content Browser-to-viewport ImGui image drag target with a Content Browser payload cache consumed on mouse release over a viewport rect. Debug x64 build succeeded.
- 2026-05-01: Batch 52 added a top toolbar Save button to the left of Place Actor, loading `Asset/Editor/ToolIcons/Save.png` through the existing WIC SRV icon path and calling the current scene save flow. Save is disabled while not editing. Debug x64 build succeeded after closing a running `NipsEngine.exe`.
- 2026-05-01: Batch 53 removed the viewport toolbar Stats dropdown, added clickable Console command suggestions above the input for prefixes such as `s` -> `stat fps` / `shadow filter ...`, kept `/help` routed to the full command list, and moved viewport splitter drawing from the ImGui foreground draw list into the viewport host draw list so other editor windows and popups render above it. Debug x64 build succeeded.
- 2026-05-01: Batch 54 adjusted viewport split/merge transitions so hidden zero-size rects are anchored at viewport 0's upper-left corner rather than the true screen origin, and shifted the grouped FPS/stat overlay 5px lower to avoid toolbar overlap. Debug x64 build succeeded.
- 2026-05-01: Batch 55 hides non-focused viewport toolbars while layout split/merge transition animation is active, leaving only the focused viewport toolbar visible during the animation. Debug x64 build succeeded after closing a running `NipsEngine.exe`.
- 2026-05-01: Batch 56 added editor-only billboards for Empty Actor, added `AFogActor` with `UHeightFogComponent` and the fog billboard icon to the shared Place Actor menu/spawn path, and changed Console suggestions into a no-focus overlay with manual click handling so the input keeps keyboard focus while suggestions are visible. Debug x64 build succeeded.
- 2026-05-01: Stabilization batch 1 hardened Content Browser and viewport input ownership: Content Browser now exposes its screen rect as a persistent mouse-blocking region while visible/animating, Browser-covered viewport areas no longer feed viewport RMB/menu tracking, Browser hover forces UI mouse capture, and stale duplicate Content Browser toolbar code is compiled out. Debug x64 build succeeded.
- 2026-05-01: Stabilization batch 2 added snapshot-based world Undo/Redo with a 50-entry limit, Ctrl+Z/Ctrl+Shift+Z routing, redo-branch flushing on new edits, Footer Log messages for undo/redo/history clear/restore, and a footer-toggleable `Undo History` window that can jump back to an earlier checkpoint. Save/New/Load clear history, major world edits now capture checkpoints, and startup/New Scene default lights now spawn as real actors at (0,0,1)/(0,0,2) with the directional light pitched to -44 degrees. Debug x64 build succeeded.
- 2026-05-01: Stabilization batch 3 changed multi-selection primary actor semantics so the most recently selected/range-clicked actor owns the gizmo, Details focus, and camera focus target. Group rotation and scale gizmo operations now use that primary actor as the pivot. Added a top-level `Edit` menu between `File` and `View` with Undo, Redo, and Undo History toggles, and removed the temporary footer History button. Debug x64 build succeeded.
- 2026-05-01: Stabilization batch 4 added History resource stats. `Undo History` now shows a `Stat History` section with entry counts, logical snapshot bytes, reserved string memory, and approximate total memory; Console also supports `stat history` and suggestions/help include it. Debug x64 build succeeded after closing a running `NipsEngine.exe` that locked the output binary.
- 2026-05-01: Stabilization batch 5 removed editor camera state from Undo/Redo snapshots so camera movement does not affect history contents or restore position. Undo restore now preserves the current editor camera when a history snapshot has no camera payload. Fixed the `Ctrl+Shift+Z` Redo binding by correcting the input chord from Ctrl+Alt+Z to Ctrl+Shift+Z, and corrected the same modifier-order issue for `Ctrl+Shift+S`. Shortcut help now lists Undo/Redo. Debug x64 build succeeded.
- 2026-05-01: Stabilization batch 6 halved orthographic RMB pan sensitivity, delayed LMB marquee selection until the drag threshold is crossed so plain clicks remain clicks, and kept marquee start anchored to the original press point. Latest crash dump (`CrashLog_20260501_200310.txt`) pointed to a debug assert in `FFrustum::UpdateFromCamera` when the inverse view-projection mid-frustum test point becomes non-finite; the frustum safety/debug check now guards non-finite values instead of asserting. Debug x64 build succeeded.
- 2026-05-01: Stabilization batch 7 changed viewport RMB `Place Actor` placement in orthographic views to use the active ortho work plane: Top/Bottom -> XY at Z=0, Front/Back -> YZ at X=0, Left/Right -> XZ at Y=0. Perspective placement still uses the camera-facing plane in front of the view. Debug x64 build succeeded after closing a running `NipsEngine.exe` that locked the output binary.

