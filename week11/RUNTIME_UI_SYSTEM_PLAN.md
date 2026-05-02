# Runtime UI System Plan

## Goal

Build a reusable engine-level Runtime UI system that can be driven by C++, GameClient, PIE, and later Lua without exposing ImGui directly to game code.

The system should feel Unity-like for game UI authors:

- Canvas-based coordinate space
- Screen-based UI flow
- Widget hierarchy
- Parent/child visibility and enabled inheritance
- Image-first widgets
- Action events instead of hard-bound Lua callbacks
- Backend abstraction so the current implementation can use ImGui, while a future renderer/widget backend can replace it

## Non-Goals

- Do not add Lua UI files during the core backend work.
- Do not couple Engine/UI to Editor, Game, or Script.
- Do not build a full UI editor in this pass.
- Do not implement complex auto-layout, rich text, IME, or navigation yet.

## Dependency Rules

- `Source/Engine/UI` may depend on Engine Core/Containers and basic C++ STL.
- `Source/Engine/UI` must not depend on `Source/Editor`.
- `Source/Engine/UI` must not depend on `Source/Game`.
- `Source/Engine/UI` must not depend on Lua or `FScriptManager`.
- ImGui usage must stay inside a backend implementation, not inside `FRuntimeUISystem`.

## Architecture

```text
Game / Lua / Editor adapter
-> FRuntimeUISystem
   -> Canvas
      -> Screen
         -> Widget hierarchy
   -> IRuntimeUIBackend
      -> ImGuiRuntimeUIBackend
      -> Future custom backend
```

## Core Concepts

### Canvas

Canvas is the root coordinate space for Runtime UI.

- Has reference resolution
- Has scale mode
- Owns one active screen
- Provides the viewport rect for layout

### Screen

Screen is a logical UI page.

Examples:

- Title
- HUD
- Pause
- Result

### Widget

Widget is a retained UI node.

Minimum widget types:

- Panel
- Text
- Button
- Image
- ProgressBar

### Hierarchy

Widgets have parent/children relationships.

- Child moves with parent
- Child visibility follows parent
- Child enabled state follows parent
- Child alpha is multiplied by parent alpha

### Action Event

Buttons do not call Lua callbacks directly.

Instead:

```text
Button clicked
-> Runtime UI pushes ActionEvent string
-> Later Lua/Game bridge polls events
-> GM:Emit(ActionEvent)
```

## Batch Plan

### Batch 1 - Core Data Model and Layout

Status: Completed

- Add `Source/Engine/UI`
- Add runtime UI types
- Add Canvas / Screen / Widget structures
- Add retained hierarchy
- Add visibility/enabled/alpha inheritance
- Add basic layout computation
- Add action event queue
- Add project-file integration
- No ImGui dependency
- No Lua dependency

Completed files:

- `Source/Engine/UI/RuntimeUITypes.h`
- `Source/Engine/UI/RuntimeUIStyle.h`
- `Source/Engine/UI/RuntimeUIWidget.h/.cpp`
- `Source/Engine/UI/RuntimeUIScreen.h/.cpp`
- `Source/Engine/UI/RuntimeUICanvas.h/.cpp`
- `Source/Engine/UI/RuntimeUISystem.h/.cpp`

Validation:

- `Debug|x64` build passed
- `GameClientDebug|x64` build passed
- Existing `LNK4098` warning remains unrelated

### Batch 2 - ImGui Backend

Status: Completed

- Add backend interface
- Add ImGui backend implementation
- Draw Panel/Text/Button/Image/ProgressBar
- Draw inside GameClient full viewport
- Draw over PIE viewport rect

Completed files:

- `Source/Engine/UI/RuntimeUIBackend.h`
- `Source/Engine/UI/Backends/ImGuiRuntimeUIBackend.h/.cpp`

Validation:

- `Debug|x64` build passed
- `GameClientDebug|x64` build passed
- Existing `LNK4098` warning remains unrelated

### Batch 3 - Input Routing

Status: Completed

- Add hit testing
- Add hover/pressed/click states
- Add input consume result
- Add action event push on click
- Prepare GameClient and PIE input priority integration

Completed files:

- `Source/Engine/UI/RuntimeUIInput.h`
- `FRuntimeUISystem::HandleInput`
- `FRuntimeUISystem::HitTest`
- Widget hover/pressed state
- Button action event queue push

Validation:

- `Debug|x64` build passed
- `GameClientDebug|x64` build passed

### Batch 4 - Image-First UI Features

Status: Completed

- Add UV rect
- Add tint and alpha
- Add sprite atlas rect support
- Add button state images
- Add clipped progress fill image
- Add 9-slice rendering contract

Completed features:

- `FRuntimeUIUVRect`
- `FRuntimeUIMargin`
- `ERuntimeUIImageDrawMode`
- `ERuntimeUIProgressFillDirection`
- Button normal/hover/pressed/disabled image paths
- Progress background/fill/frame image paths
- ImGui image resolver with texture size
- ImGui 9-slice draw path
- Directional progress fill image clipping

Validation:

- `Debug|x64` build passed
- `GameClientDebug|x64` build passed

### Batch 5 - C++ Sample and Validation

Status: Completed

- Add small C++ sample screen creation path
- Validate GameClient render path
- Validate PIE overlay render path
- Validate no Editor/Lua/Game dependency leak

Completed files:

- `Source/Engine/UI/RuntimeUISampleScreens.h/.cpp`

Completed validation:

- C++ sample screen tree builds without Lua
- Engine/UI still has no Editor/Lua/Game dependency
- `Debug|x64` build passed
- `GameClientDebug|x64` build passed

Pending render-path validation:

- Actual GameClient ImGui frame ownership is not wired yet.
- Actual PIE overlay call site is not wired yet.
- These are split into Batch 6 to avoid mixing core UI data/backend work with runtime/editor frame ownership.

### Batch 6 - Runtime/PIE Integration

Status: Completed

- Add a runtime UI owner accessible from `UEngine`
- Add GameClient ImGui frame ownership or a safe hook into the existing frame path
- Add PIE overlay call site after viewport image rendering
- Keep Editor dependency one-way: Editor may call Engine/UI, Engine/UI must not call Editor
- Add input consume hooks without changing Lua

Completed:

- `UEngine` owns `FRuntimeUISystem`
- `UEngine::GetRuntimeUI()` accessor added
- `UGameEngine` owns GameClient ImGui Runtime UI backend
- GameClient render pipeline calls `UEngine::RenderRuntimeUI` after scene composite and before frame end
- PIE viewport host draws Runtime UI over the focused PIE viewport image
- GameClient Runtime UI mouse input can consume input before `PlayerController`
- PIE Runtime UI can consume ImGui mouse input and request viewport mouse block

Validation:

- `Debug|x64` build passed
- `GameClientDebug|x64` build passed

### Batch 7 - Lua-Ready Facade

Status: Completed

- Add a small C++ facade that future Lua bindings can expose without giving script code direct access to all UI internals
- Keep facade inside Engine/UI
- Keep facade independent from Lua, Editor, and Game code
- Provide Unity-like helper commands for canvas/screen/widget creation and mutation
- Provide action event polling as the bridge point to the future game management layer

Completed files:

- `Source/Engine/UI/RuntimeUIFacade.h/.cpp`

Completed API surface:

- `EnsureCanvas`
- `EnsureScreen`
- `ShowScreen`
- `CreatePanel`
- `CreateText`
- `CreateButton`
- `CreateImage`
- `CreateProgressBar`
- `SetText`
- `SetImage`
- `SetProgress`
- `SetVisible`
- `SetEnabled`
- `SetActionEvent`
- `SetWidgetTransform`
- `SetWidgetAnchors`
- `SetImageOptions`
- `SetButtonImages`
- `SetProgressImages`
- `PollActionEvents`

Validation:

- `Debug|x64` build passed
- `GameClientDebug|x64` build passed
- Existing `LNK4098` warning remains unrelated

### Batch 8 - Lua Bridge Later

Status: Completed

- Add `Engine.API.UI` bindings
- Bind screen/widget mutation functions
- Bind action event polling
- Keep Runtime UI core independent from Lua

Completed:

- `Engine.API.UI.ShowScreen`
- `Engine.API.UI.CreateText`
- `Engine.API.UI.CreateButton`
- `Engine.API.UI.CreateImage`
- `Engine.API.UI.CreateProgressBar`
- `Engine.API.UI.RemoveWidget`
- `Engine.API.UI.SetText`
- `Engine.API.UI.SetImage`
- `Engine.API.UI.SetProgress`
- `Engine.API.UI.SetVisible`
- `Engine.API.UI.SetEnabled`
- `Engine.API.UI.SetActionEvent`
- `Engine.API.UI.PollActionEvents`

### Batch 9 - Editor Preview Panel

Status: Completed

- Add an Editor-only Runtime UI Preview panel.
- Preview owns a separate `FRuntimeUISystem` so it does not mutate PIE or GameClient UI.
- Initial source uses the C++ GameJam sample UI set.
- Optional read-only source can render the live engine runtime UI into the Editor panel.
- Add resolution presets, zoom, screen selector, widget summary, and action-event log.
- Add View menu toggle: `View > Runtime UI Preview`.

Completed files:

- `Source/Editor/UI/EditorRuntimeUIPreviewWidget.h/.cpp`
- `Source/Editor/UI/EditorMainPanel.h/.cpp`
- `Source/Editor/UI/EditorToolbarWidget.h/.cpp`

Validation:

- `Debug|x64` build passed.
- `GameClientDebug|x64` build passed.

## UI Set Authoring Rule

State machines should own flow, not widget construction details.

Preferred Lua shape:

```lua
local TitleUI = {}

function TitleUI.Build(UI)
    UI.ShowScreen("Title")
    UI.CreateText("Title", "Title.Name", "SSAMURAI", 80, 70, 500, 80)
    UI.CreateButton("Title", "Title.Start", "START", "StartRequested", 160, 240, 240, 56)
end

return TitleUI
```

Runtime:

```lua
TitleUI.Build(Engine.API.UI)
```

Editor Preview later:

```lua
TitleUI.Build(PreviewUI)
```

This keeps the same UI set usable in both GameClient/PIE and Editor preview without executing gameplay state logic.

## Current Public API Target

```cpp
CreateCanvas(CanvasId)
CreateScreen(ScreenId, CanvasId)
SetActiveScreen(CanvasId, ScreenId)

CreateWidget(ScreenId, WidgetDesc)
RemoveWidget(WidgetId)

SetText(WidgetId, Text)
SetImage(WidgetId, ImagePath)
SetProgress(WidgetId, Value)
SetVisible(WidgetId, bVisible)
SetEnabled(WidgetId, bEnabled)
SetActionEvent(WidgetId, EventName)

UpdateLayout(RenderContext)
ConsumeActionEvents()
```

## Notes

- Runtime UI is retained-mode even if the first backend uses ImGui.
- PIE should render Runtime UI as an overlay on top of the viewport image, not inside the viewport render target.
- GameClient should render Runtime UI over the full backbuffer.
