# Actor Sequence / Curve Editor UX Plan

## Goal

Build an ImGui-based editor flow inspired by UE Sequencer, without copying every feature.

The intended workflow is:

```text
ActorSequenceComponent Details
  -> Open Sequencer
  -> runtime playback options only
  -> bottom timeline window
  -> select track/channel
  -> inspect timing and preview
  -> later edit sections/keys/curves directly
```

## Batch Progress Plan

```text
Batch 0. Record plan and Level Sequencer extension rule
Batch 1. Make Curve Editor view range stable during key/tangent editing
Batch 2. Add Curve Editor zoom/pan/fit UX
Batch 3. Improve Actor Sequencer first-pass selection/safety UX
Batch 4. Add Sequencer-owned track/key authoring
Batch 5. Add draggable playback range start/end handles
Batch 6. Build and verify
```

Progress is reported by batch, not by individual line edit.

## Level Sequencer Compatibility

The current first pass opens from `UActorSequenceComponent`, so it is an Actor Sequencer UI today.

It can become a Level Sequencer later if the UI is split like this:

```text
Sequencer UI
  -> reads FSequencerViewModel
  -> does not care whether the source is ActorSequence or LevelSequence

ActorSequence Adapter
  -> builds view model from UActorSequenceComponent

Future LevelSequence Adapter
  -> builds view model from level-owned sequence asset
  -> can bind multiple actors/components
```

The shared parts should be:

```text
timeline view transform
toolbar/playhead/scrub controls
track tree drawing
section block drawing
curve graph drawing
selection state
```

The source-specific parts should be:

```text
how tracks are collected
how object bindings are resolved
how edits are written back
how preview evaluation is executed
```

Do not hard-code future Level Sequencer behavior into the first pass. Keep the ActorSequence version simple, then extract a view model only when the second source appears.

## Current First Pass

Implemented:

```text
FEditorActorSequencerWidget
  - opens from ActorSequence details
  - shows the bound Actor row by default
  - top Add menu adds/pins owned components into the Sequencer tree
  - each Actor/Component row owns a + button for adding Property Tracks
  - Key button writes current property value to the selected channel curve
  - draws key markers inside the section row
  - left track/channel list
  - right timeline canvas
  - draggable playback start/end markers
  - playback start/end second labels
  - preview playhead
  - Play / Pause / Stop
  - current time edit
  - mouse wheel zoom
  - RMB/MMB pan
  - ruler LMB scrub
```

The first editing pass is intentionally small:

```text
Sequencer owns:
  - Add component rows
  - Add property/channel tracks from the component row
  - Add key at current playhead
  - Move playback range start/end

Details panel remains available:
  - Open Sequencer
  - Looping
  - Play Rate
  - Auto Play
  - Pause at End
  - Start Offset (seconds)
```

Track, section, key, curve, playback range, and property binding edits belong in the Sequencer window, not in Details.

## Track Tree UX

The Sequencer tree should read like this:

```text
Actor
  + Root/SceneComponent
    + Location.X
    + Rotation.Z
  + StaticMeshComponent
    + Visible
    + MaterialColor.R
  + MovementComponent
    + Speed
```

Rules:

```text
Top + Add:
  adds/pins a Component row

Actor row +:
  adds a Property Track against the Actor's RootComponent
  this is how Actor Transform is authored

Component row +:
  adds a Property Track for that Component

Property Track row:
  can be selected
  global Key button writes a key to the selected row
```

Actor Transform is treated as RootComponent `Location / Rotation / Scale`.
This matches the current engine data model, because Actor transform is backed by the root scene component.

## Property Support

Current supported reflected property types:

```text
Bool:
  Value channel, stored as 0/1

Int:
  Value channel, rounded from evaluated float

Float:
  Value channel

Vec3:
  X/Y/Z channels

Vec4:
  X/Y/Z/W channels

Color:
  R/G/B/A channels
```

The Sequencer no longer requires the property to opt into `Animatable`.
It still requires the property to be represented by one of the scalar-compatible reflected types above.

## Core UX Rule

The editor must not use normal child-window scroll as the main timeline navigation.

Use a custom view transform instead:

```text
ViewStartTime
ViewEndTime
TimeToX()
XToTime()
```

Mouse wheel zooms the timeline around the cursor. Drag panning shifts the view range.

This matches the feel of UE's Sequencer/Curve Editor better than a scrollable ImGui child.

## Curve Editor UX Guarantee

The current Curve Editor has a risky behavior:

```text
Key drag updates key value
-> auto-fit recalculates value range from the new key
-> same mouse position maps to a different curve value
-> value can explode, e.g. 10 -> 50000
```

Required fix:

```text
During key/tangent drag:
  - freeze view range
  - do not auto-fit MinValue/MaxValue
  - map mouse using the frozen range
  - optionally clamp dragged value to the current view range

After drag ends:
  - allow auto-fit again only if user requests it or if auto-fit mode is enabled
```

Suggested state:

```cpp
float ViewMinTime;
float ViewMaxTime;
float ViewMinValue;
float ViewMaxValue;
bool bViewRangeInitialized = false;
bool bDraggingCurveKey = false;
bool bDraggingTangent = false;
```

Preferred operations:

```text
Wheel:
  zoom around cursor

RMB/MMB drag:
  pan graph

LMB key drag:
  edit key using frozen view range

Alt:
  free drag without snap

F or Fit button:
  fit selected/all keys
```

## Sequencer Next Steps

1. Section interaction

```text
LMB drag section body:
  move StartTime

LMB drag section edges:
  resize StartTime / Duration

Snap:
  optional, based on frame or small time unit
```

2. Track selection integration

```text
Click track row:
  select channel
  show selected section/channel info
  optionally open linked curve in Curve Editor
```

3. Curve overlay in Sequencer

```text
For selected channel:
  draw compact curve preview inside the section row
  draw key markers based on referenced CurveAsset
```

4. Dedicated Curve Graph mode

```text
Left panel:
  track/channel tree

Right panel:
  value graph
  multi-curve overlay
  key/tangent editing
```

5. Data model decision

Current data model:

```text
FActorSequenceChannel
  -> Playback.CurveAssetPath
```

This means sequence tracks reference external curve assets.

Important implication:

```text
Editing keys edits the referenced Curve Asset, not a private per-sequence curve.
```

Later decision:

```text
Option A:
  keep external curve assets
  simple and reusable

Option B:
  store curve data inside FActorSequenceChannel
  closer to UE Sequencer
  bigger serialization/runtime change

Option C:
  auto-create a private curve asset when a track is added
  practical middle ground
```

For now, use Option A and make the UI clearly show the referenced curve path.

6. Playback range

UE Sequencer distinguishes the playback range from the visible range.

Current local mapping:

```text
green marker:
  Playback Start
  UActorSequence::StartTime

red marker:
  Playback End
  UActorSequence::StartTime + UActorSequence::Duration
```

Dragging the green marker preserves the old end time and changes StartTime/Duration.
Dragging the red marker preserves StartTime and changes Duration.

The playhead is clamped inside this playback range.

## Apply Mode Policy

Unreal Sequencer has Blend Types such as:

```text
Absolute
Additive
Relative
Additive From Base
Override
```

Our current enum is simpler and uses UE-aligned names where possible:

```text
Absolute:
  set property to evaluated curve value
  simple one-section version of UE Absolute

Additive:
  property = cached base value + evaluated curve value
  similar in spirit to Additive

Multiply:
  property = cached base value * evaluated curve value
  local engine convenience mode, not a direct UE Sequencer equivalent
```

Legacy names are intentionally removed:

```text
Direct -> Absolute
Add    -> Additive
```

Do not keep aliases for these names in editor UI, C++, or Lua API.

Do not present `Absolute / Additive / Multiply` as full UE parity.
If deeper Sequencer blending becomes a goal, replace this with an explicit section blend model:

```text
Absolute
Additive
Relative
AdditiveFromBase
Override
```

## Key Storage Decision

Current implementation:

```text
FActorSequenceChannel
  -> Playback.CurveAssetPath
  -> external UCurveFloatAsset
```

This means pressing Key edits the referenced Curve Asset.

Short-term advantage:

```text
small runtime change
reuses current Curve Editor and ResourceManager
easy to inspect curve assets directly
```

Main weakness:

```text
multiple sequence tracks can accidentally share and mutate the same curve asset
sequence data is not self-contained
adding keys in Sequencer may surprise users because it changes an external asset
```

Pending data-model step:

```text
.scene
  -> Actor
    -> UActorSequenceComponent
      -> SequenceAssetPath = "Asset/Sequence/Example.sequence"

.sequence
  -> Type = "ActorSequence"
  -> Version
  -> ActorSequence
    -> StartTime / Duration / Loop
    -> Tracks / Sections / Channels
    -> InlineCurve / OwnedCurve
```

Then Curve Assets become optional reusable sources, while normal Sequencer keying stores keys inside the `.sequence` asset.
That is closer to UE's Sequencer mental model and keeps the Scene from owning detailed key data.

Curve Asset extension policy:

```text
Use .curve
Do not keep .curve.json as a supported extension
```

Curve assets still store editable JSON internally for now. The extension describes engine asset type, not physical encoding.
Binary cooking can be added later as a runtime cache layer.

## Safety Rules

Do not keep long-lived actor/component pointers without invalidation.

For editor widgets:

```text
On scene reset / undo restore:
  clear cached target pointers

On render:
  validate target component
  validate sequence
  validate owner actor
```

This matters because Undo restores the world by destroying and recreating scene objects.

Current guard:

```text
FEditorActorSequenceEditModel::IsSequenceComponentLive()
  - checks UObjectManager before dereferencing cached pointers
  - checks owner actor existence
  - checks pending-kill actor state
```

The widget is allowed to store the selected `UActorSequenceComponent*`, but every render path must validate it before using it.

## Widget Responsibility Rule

The Widget should not own sequence-editing knowledge.

Current split:

```text
FEditorActorSequencerWidget
  - draws toolbar/menu/timeline
  - tracks selection and drag state
  - asks edit model to mutate data

FEditorActorSequenceEditModel
  - validates live ActorSequenceComponent
  - collects animatable component properties
  - resolves display track/channel handles
  - adds tracks/channels
  - adds curve keys
  - notifies editor undo/dirty state
```

This keeps the implementation usable later by a Level Sequencer adapter instead of binding all behavior directly to one ImGui widget.

## Implementation Notes

Prefer custom ImDrawList canvas for Sequencer and Curve Graph.

Avoid building the main editor interaction out of many small ImGui widgets because:

```text
scrolling becomes hard to control
drag behavior becomes inconsistent
UE-style zoom/pan needs a stable coordinate transform
```

Use ImGui widgets for toolbar, combo boxes, buttons, and numeric inputs.
Use DrawList for timeline, grid, playhead, sections, curves, keys, and tangent handles.
