# Input Policy Proposal

Updated: 2026-05-06

Scope: Engine, Editor, PIE, GameClient, Runtime UI, Lua.

This document defines the target input ownership policy before refactoring. It is not an implementation checklist by itself. Any code work from this document should be batched and approved first.

## Current Problem

The project already has useful pieces:

- `InputSystem`: samples OS keyboard/mouse state, drag state, raw mouse delta, GUI capture flags, text input.
- `FInputRouter`: resolves hovered/focused/captured viewport, relative mouse mode, absolute mouse clip, ImGui blocking.
- `FEditorViewportClient`: consumes editor viewport input, transform shortcuts, gizmo interaction, selection, camera navigation, PIE possess/eject.
- `FEditorWorldController`, `FGameInputBridge`, `PlayerController`: receive input in different modes.
- RML UI and ImGui both set or imply input capture.
- Lua can read input through script APIs.

The weak point is not that these pieces exist. The weak point is that ownership is not declared as one explicit policy. RML, ImGui, Editor viewport tools, PIE, PlayerController, and Lua can all look at input from different layers. That makes bugs like gizmo hover during capture, transform shortcut suppression, PIE cursor capture, and UI/game input leakage likely to recur.

## UE Reference Model

Unreal's model is useful as a policy reference, not something to copy line for line.

- Hardware input is converted into game-facing input data through `PlayerInput`, which belongs to `PlayerController`.
- Gameplay input is consumed through `InputComponent` priority. UE documents the priority order as input-enabled Actors, Controllers, Level Script, then Pawns. Once an input component consumes input, lower priority targets do not receive it.
- Slate/CommonUI routes UI input before or at the viewport layer. CommonUI describes an input routing flow where the viewport captures input, finds the topmost painted active node, and recursively checks handlers.
- CommonUI also uses input configs with modes equivalent to UI only, game only, or both. It includes mouse capture policies such as no capture, permanent capture, capture during mouse down, and right-mouse-only capture.

Project interpretation:

- UI gets first chance when it is explicitly active/capturing.
- Viewport owns pointer input only when hovered/focused/captured or in relative mouse mode.
- Runtime gameplay receives input through a player-facing layer, not by reading raw OS state everywhere.
- Lua should observe the same post-policy input as gameplay, unless a deliberate debug/editor API is used.

References:

- Epic Input Overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/input-overview-in-unreal-engine
- Epic CommonUI Input Technical Guide: https://dev.epicgames.com/documentation/en-us/unreal-engine/commonui-input-technical-guide-for-unreal-engine
- Epic CommonUI Input Fundamentals: https://dev.epicgames.com/documentation/en-us/unreal-engine/input-fundamentals-for-commonui-in-unreal-engine

## Target Ownership Chain

One frame of input should flow like this:

1. Platform
   - Windows messages, raw mouse, text input, wheel, key state.

2. `InputSystem`
   - Samples hardware.
   - Produces immutable `FInputSystemSnapshot`.
   - Does not decide Editor vs Game vs UI ownership.
   - Does not dispatch to gameplay or Lua directly.

3. `FInputPolicyRouter`
   - New or evolved layer from current `FInputRouter`.
   - Consumes snapshot plus UI capture declarations.
   - Produces one `FInputFrameDispatch` with domain-specific views:
     - `EditorUI`
     - `EditorViewport`
     - `PIEViewport`
     - `RuntimeUI`
     - `Game`
     - `Lua`
   - Decides focus, capture, hover, suppression, relative mouse, absolute clip.

4. Domain consumers
   - ImGui consumes `EditorUI`.
   - Editor viewport tools consume `EditorViewport`.
   - PIE session consumes `PIEViewport`.
   - RML consumes `RuntimeUI`.
   - PlayerController/InputComponents consume `Game`.
   - Lua consumes `Lua`, derived from Game or UI-safe text events depending on mode.

5. Side-effect systems
   - Cursor visibility/lock.
   - Mouse clipping.
   - Gizmo hover state.
   - Picking and selection.
   - Text input queue.

Only the policy router should decide whether these side effects are allowed in the current frame.

## Domains

### EditorUI

Owner:

- ImGui editor panels, menu bar, content browser, property widget, debug windows.

Allowed:

- UI clicks, text input, menu shortcuts when focused.
- Capture mouse/keyboard when ImGui wants capture.

Blocks:

- Editor viewport picking.
- Gizmo hover tint.
- Editor camera navigation.
- Game/PIE input unless the active PIE viewport has an explicit focus override.

### EditorViewport

Owner:

- Editor viewport client and editor viewport tools.

Allowed:

- Selection, ID Picking, gizmo hover/drag, transform shortcuts, camera navigation.

Blocks:

- RML runtime UI input.
- Game input.

Rules:

- LMB/RMB capture or drag owns pointer input until release.
- While pointer is captured for box select, camera look, or gizmo drag:
  - No passive picking.
  - No passive gizmo hover tint updates, except the currently held gizmo axis.
  - No selection hover changes.
- Transform mode shortcuts should be handled as editor commands, not as raw key polling in unrelated layers.

### PIEViewport

Owner:

- Editor-hosted runtime viewport/session.

Modes:

- `PIEPossessed`: runtime owns game input.
- `PIEEditorControl`: editor tools own viewport input while looking into PIE world.
- `PIEPaused`: editor may inspect, runtime may receive only explicitly allowed UI input.

Rules:

- PIE must have a session object that owns:
  - active PIE world
  - active viewport index
  - player controller
  - possessed/editor-control state
  - mouse focus released/reacquired state
  - runtime UI context
- PIE input should pass through the same runtime UI/game policy as GameClient after the editor shell grants viewport ownership.
- Editor shell may draw around PIE, but should not directly mutate runtime input after policy resolution.

### RuntimeUI

Owner:

- RML UI.

Allowed:

- Pointer events, keyboard navigation, text input, form control input.

Input modes:

- `UIOnly`: RML receives UI input, Game receives no movement/look/actions.
- `GameOnly`: RML receives no gameplay-blocking input except optional debug hover.
- `GameAndUI`: RML receives UI input and Game receives non-consumed gameplay input.

Rules:

- If focused RML element is a form control:
  - text input goes to RuntimeUI only.
  - Game action keys are blocked unless explicitly whitelisted.
- If RML captures mouse:
  - Game look and fire actions are blocked.
  - Lua gameplay input mirrors the blocked Game state.
- RuntimeUI should be shared by PIE and GameClient through one adapter.

### Game

Owner:

- `PlayerController`, Pawn, input components or equivalent future stack.

Allowed:

- Movement, look, actions, gameplay cursor lock/visibility.

Rules:

- Game receives policy-filtered input, never raw `InputSystem` directly.
- Target structure should mimic UE's conceptual priority:
  - temporary high-priority actors or input components
  - controller
  - world/level script
  - pawn
- Consumption should stop propagation to lower priority gameplay listeners.

### Lua

Owner:

- Runtime scripts bound through `ScriptManager` APIs.

Policy:

- Lua gameplay input should be based on the same filtered frame as Game.
- Lua UI text input should be based on RuntimeUI policy.
- Lua should not bypass UI capture by reading raw hardware state unless using an explicit editor/debug-only API.

Recommended API split:

- `LuaInput.GetAction("Jump")`: policy-filtered gameplay action.
- `LuaInput.GetAxis("MoveForward")`: policy-filtered gameplay axis.
- `LuaInput.IsKeyDown(...)`: discouraged for gameplay; if kept, it should default to policy-filtered state.
- `LuaInput.IsRawKeyDown(...)`: editor/debug only, guarded or clearly named.
- `LuaInput.ConsumeTextInput()`: receives text only when RuntimeUI/Game policy allows script text.
- `LuaInput.GetMouseDelta()`: policy-filtered; zero when UI owns look input.

## Input Modes

Add a small explicit mode model. Names can change, but the concepts should exist.

```cpp
enum class EInputDomain
{
    None,
    EditorUI,
    EditorViewport,
    PIEViewport,
    RuntimeUI,
    Game,
    Lua
};

enum class ERuntimeInputMode
{
    GameOnly,
    UIOnly,
    GameAndUI
};

enum class EPointerCaptureMode
{
    None,
    CaptureDuringMouseDown,
    CaptureDuringRightMouseDown,
    CapturePermanently,
    AbsoluteClip,
    RelativeMouse
};
```

Current `ERuntimeInputMode` already exists. It should become policy input, not something each subsystem interprets separately.

## Proposed Core Types

### `FInputSystemSnapshot`

Already exists. Keep it hardware-focused.

Allowed fields:

- raw key state
- raw mouse position/delta
- wheel
- drag state
- text input queue

Not allowed:

- "RML owns this"
- "Editor viewport owns this"
- "Game should ignore look"

### `FInputCaptureRequest`

Each UI/runtime/editor layer submits capture intent for the next dispatch.

```cpp
struct FInputCaptureRequest
{
    EInputDomain Domain = EInputDomain::None;
    bool bWantsMouse = false;
    bool bWantsKeyboard = false;
    bool bWantsTextInput = false;
    bool bBlocksGameLook = false;
    bool bBlocksGameMove = false;
    bool bAllowsViewportFocusByClick = false;
    EPointerCaptureMode PointerCaptureMode = EPointerCaptureMode::None;
};
```

Examples:

- ImGui text box: `EditorUI`, keyboard + text, block viewport keyboard.
- RML form control in PIE: `RuntimeUI`, keyboard + text, block Game.
- PIE possessed viewport: `PIEViewport`, relative mouse, Game input allowed.
- Gizmo drag: `EditorViewport`, absolute clip, block passive picking/hover.

### `FInputFrameDispatch`

The router output.

```cpp
struct FInputFrameDispatch
{
    FInputFrame EditorUI;
    FViewportInputContext EditorViewport;
    FViewportInputContext PIEViewport;
    FInputFrame RuntimeUI;
    FInputFrame Game;
    FInputFrame Lua;

    EInputDomain PointerOwner = EInputDomain::None;
    EInputDomain KeyboardOwner = EInputDomain::None;
    EInputDomain TextOwner = EInputDomain::None;

    bool bAllowPicking = false;
    bool bAllowGizmoHover = false;
    bool bAllowGameLook = false;
    bool bAllowGameMove = false;
};
```

## Priority Policy

Pointer priority:

1. Active capture owner until release or explicit cancel.
2. Editor modal/menu/text input.
3. Runtime UI under cursor if active and mode is `UIOnly` or `GameAndUI`.
4. Hovered focused viewport.
5. Game fallback only when viewport/game has focus.

Keyboard priority:

1. Text input owner.
2. Editor command layer when editor is active and not in possessed PIE.
3. Runtime UI focused widget.
4. Game actions.
5. Lua gameplay observers from filtered Game frame.

Text priority:

1. ImGui text field.
2. RML form control.
3. Lua/script text only if explicitly requested by game mode.
4. Otherwise discarded for gameplay.

## Required Behavioral Rules

- UI capture must zero blocked input in the frame visible to lower domains.
- Capture state must be sticky until release where appropriate.
- A mouse-down that starts UI capture must not also select an actor.
- A mouse-down that starts PIE capture must follow the selected mouse capture mode.
- During LMB/RMB viewport capture:
  - picking is blocked unless the active tool specifically requests it.
  - gizmo hover tint is blocked unless gizmo is currently held.
  - cursor side effects are owned by the capture owner.
- Relative mouse mode should produce centered mouse position and delta only for the owning domain.
- Absolute clip should be editor-only except explicit runtime use.
- GameClient and PIE should use the same RuntimeUI/Game filtered frame after PIE shell grants ownership.
- Lua receives filtered Game/Lua input, not raw hardware input.

## Migration Plan

### Batch 1: Policy document and naming

- Keep code behavior unchanged.
- Add comments/types around domains and capture modes.
- Mark raw input APIs that should not be used by gameplay.

Validation:

- No build behavior changes.

### Batch 2: Make `InputSystem` sampler-only

- Remove or deprecate GUI ownership meaning from `InputSystem`.
- Keep compatibility setters temporarily if needed, but route them into `FInputCaptureRequest`.
- Make text queues clearly owned by policy output.

Validation:

- Editor typing in ImGui.
- RML text input in PIE/GameClient.
- Lua text input smoke.

### Batch 3: Promote `FInputRouter` to policy router

- Add explicit domain outputs.
- Move hard-block logic from scattered code into router policy.
- Add `bAllowPicking`, `bAllowGizmoHover`, `bAllowGameLook`, `bAllowGameMove`.

Validation:

- Transform shortcuts `QWER/1234/Space`.
- LMB/RMB capture blocks picking and hover tint.
- Gizmo drag does not lose cursor/clip state.
- Editor UI clicks do not select actors.

### Batch 4: PIE session boundary

- Add `FPIESession` owner.
- Move PIE player controller, viewport index, possess state, mouse release/reacquire there.
- Feed PIE through runtime UI/game policy.

Validation:

- PIE start/stop.
- PIE possess/eject.
- RML over viewport but under editor UI.
- GameClient parity check.

### Batch 5: Lua input cleanup

- Route Lua input through filtered Game/Lua frame.
- Rename or guard raw key APIs.
- Add action/axis style helpers before removing old APIs.

Validation:

- Existing Lua gameplay still works.
- UI-focused text blocks gameplay script input.
- Debug/raw input available only where intended.

## Acceptance Tests

Editor:

- Clicking ImGui property fields never picks actors.
- Dragging editor panels never changes gizmo hover.
- `QWER`, `1234`, `Space` transform shortcuts work when viewport owns keyboard.
- LMB box select suppresses passive gizmo hover and ID picking.
- RMB camera look suppresses passive picking.

PIE:

- Possessed PIE sends movement/look/action to PlayerController.
- RML form input blocks game movement/look.
- Releasing PIE mouse focus allows editor UI interaction.
- Reacquiring PIE focus is explicit and stable.
- Editor toolbar shortcuts do not leak into game while possessed unless whitelisted.

GameClient:

- Runtime UI modes `GameOnly`, `UIOnly`, `GameAndUI` behave the same as PIE runtime mode.
- Lua gameplay input is blocked when RML form control owns text.
- Mouse lock/visibility follows runtime mode.

Lua:

- `GetAction` and `GetAxis` match PlayerController state.
- Raw key access is not used by gameplay scripts by default.
- Text input is consumed once and by the correct owner.

## Open Decisions

- Whether to keep `InputSystem::GetKey` available globally or require domain-specific input access.
- Whether Editor command shortcuts should be a separate priority layer above viewport keyboard.
- Whether Lua raw input should be compiled out in GameClient Release or kept as a named debug API.
- Whether RML should be allowed to operate in `GameAndUI` with non-consuming hover, or only focused/captured widgets should receive input.
- Whether current `FEditorInputRouter` should be renamed/moved, or replaced by an engine-level `FInputPolicyRouter` plus editor adapter.
