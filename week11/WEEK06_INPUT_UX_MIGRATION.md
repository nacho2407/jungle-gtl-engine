# Week06 Input/UX Migration Plan

## Goal

Bring the richer Week06_Team3 input and viewport UX architecture into the current Week09 engine without losing existing Week09 rendering, editor, scene, and viewport behavior.

The migration should be tracked here so work can resume safely after context compaction.

## Source Project

- Week06 source: `C:/Users/jungle/Desktop/YG/Week06/Projects/Week06_Team3/KraftonEngine`
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
- [ ] full Week06 debug/shortcut/stat overlay parity. Not yet fully migrated.

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

- [ ] Full Week06 shortcut overlay/debug overlay/stat overlay parity. Week09 has overlay widgets, but the exact Week06 debug/shortcut surface has not been fully re-created.
- [ ] Week06 `FOverlayStatSystem` parity. Week09 has `EditorStatWidget` and viewport debug stats, but the Week06 grouped overlay-stat pipeline is not fully migrated.
- [ ] Week06 `RenderEditorDebugPanel`, `RenderHiZDebug`, and `Place Actors (Grid)` parity. Week09 has renderer/debug data of its own, so this should be adapted without removing Week09 shadow/culling/decal debug UI.
- [ ] Viewport toolbar polish pass: paired snap button rounding, layout icon popup styling, and exact Week06 hover/active spacing still need runtime visual comparison.
- [ ] Viewport settings overlay content parity. Week09 now uses an overlay, but it also preserves Week09-only shadow/light-culling controls, so exact Week06 menu grouping still needs review.
- [ ] Main menu parity. Week09 still keeps its existing `Files/View/Edit/Help` surface; compare against Week06 menu grouping before deciding whether to replace or preserve.
- [ ] PIE editor-window hide/restore behavior. PIE routing and viewport tracking are migrated, but Week06-style full editor panel hide/restore needs explicit comparison.
- [ ] Week06 editor debug utilities/place-grid/debug panels, if required, are not fully migrated yet.

### Week09 Features To Preserve During Further Week06 Refactor

- [ ] Material Editor dock/window and material workflows.
- [ ] Jungle Property Window behavior and selected-object editing.
- [ ] Scene Manager and Stat Profiler panels.
- [ ] Week09 File/asset actions from the existing toolbar/menu layer.
- [ ] Shadow atlas/cube preview controls in Viewport Settings.
- [ ] Light culling modes: Clustered, Tiled, None.
- [ ] Week09 view modes: Heatmap, Depth, Normal.
- [ ] Existing layout/settings persistence in `Editor.ini` / `imgui.ini`.
- [ ] Current Week09 Add Actor primitive/light entries such as Fireball, DecalSpotLight, and light actors.

### Recently Observed Regressions

- [x] Cursor could remain invisible or flicker while idle because cursor show/hide normalization was called every frame. Fixed by making cursor ownership cleanup transition-based.
- [x] Viewport toolbar right-side controls could overlap or clip in narrow panes. Fixed by folding Type/View/Stats/Settings/Layout/Split into a Week06-style overflow menu when the row is too tight.

## Verification Checklist

### Build

- [ ] Project generation still succeeds.
- [x] Full solution build succeeds.
- [x] No newly introduced include cycles.
- [x] No unresolved external symbols from copied Week06 files.

### Input

- [ ] keyboard pressed/down/released works. Implemented; runtime QA pending.
- [ ] mouse pressed/released works. Implemented; runtime QA pending.
- [ ] wheel works. Implemented; runtime QA pending.
- [ ] left/right drag start/end works. Implemented; runtime QA pending.
- [ ] raw mouse works in relative mode. Implemented; runtime QA pending.
- [ ] focus loss releases stuck keys/buttons. Implemented; runtime QA pending.
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
- [ ] camera right-drag look works.
- [ ] middle pan works.
- [ ] alt-left orbit works.
- [ ] alt-right dolly works.
- [ ] alt-middle pan works.
- [ ] WASDQE movement works.
- [ ] arrow rotation works.
- [ ] wheel zoom/dolly works.

### UX

- [ ] viewport toolbar does not leak input into scene.
- [ ] viewport image toolbar buttons switch tools/layouts.
- [ ] viewport snapping toolbar buttons quantize gizmo motion. Implemented; runtime QA pending.
- [ ] top toolbar Add Actor popup spawns primitives. Implemented; runtime QA pending.
- [ ] right-click Place Actor popup spawns at clicked viewport point. Implemented; runtime QA pending.
- [ ] bottom console drawer opens/closes with footer button and backtick cycle. Implemented; runtime QA pending.
- [ ] non-viewport ImGui windows do not leak input into scene.
- [ ] popup close does not click-through into viewport.
- [ ] cursor hides and restores correctly.
- [ ] cursor restores after RMB/MMB release without legacy double-hide. Implemented; runtime QA pending.
- [ ] cursor show/hide counter is normalized to prevent invisible cursor drift. Implemented; runtime QA pending.
- [ ] cursor does not flicker while idle. Implemented; runtime QA pending.
- [ ] cursor does not jump on entering relative mode.
- [ ] viewport toolbar right controls overflow instead of overlapping in narrow panes. Implemented; runtime QA pending.
- [ ] viewport toolbar buttons are vertically centered in the 34px pane toolbar. Implemented; runtime QA pending.
- [ ] viewport toolbar left group has Week06-like light left padding and right group is positioned independently from left-flow cursor state. Implemented; runtime QA pending.
- [ ] Win32 client cursor restores to arrow after viewport drag/relative cursor release instead of keeping hidden or resize cursor shape. Implemented; runtime QA pending.
- [ ] hidden viewport slots do not receive input.

### PIE

- [ ] start PIE from active viewport.
- [ ] possess/eject toggle works.
- [ ] end PIE shortcut works.
- [ ] editor input resumes after PIE.
- [ ] active viewport state remains correct.

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

