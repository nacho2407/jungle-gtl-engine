# Curve / Timeline / ActorSequence Roadmap

## Goal

Build a lightweight curve playback stack that fits the current Engine / Editor / PIE / GameClient split.

This is not a full Level Sequencer pass.

Current scope:

- `UCurveFloatAsset`: reusable float curve data asset.
- `FTimelinePlayer`: lightweight code/Lua playback helper that returns curve values through callbacks.
- `UActorSequenceComponent`: actor-local sequence component that binds curve tracks to component properties.

Deferred:

- CameraModifier implementation. Another developer owns CameraModifier work.
- LevelSequence, camera cuts, event tracks, audio tracks.
- Full render/engine dependency cleanup from the previous refactor roadmap.

## Current Engine Facts

Observed in the latest code:

- Runtime object base is `UObject`.
- Components derive from `UActorComponent`.
- Components already serialize through `FArchive`, `FJsonReader`, and `FJsonWriter`.
- Scene save/load uses `FActorSerialization::BuildActorJson()` and `SpawnActorFromJson()`.
- Components currently have runtime `uint32 UUID`, but no persistent `FGuid` type exists yet.
- Details panel uses manual `FPropertyDescriptor` from `GetEditableProperties()`.
- There is no `CollectAnimatableProperties()` hook yet.
- `FResourceManager` currently owns texture/material/static mesh/font/particle/shader style loading.
- Lua bindings are grouped under `Engine/Runtime/Script/API`.

Implication:

- Curve/Timeline/ActorSequence runtime code should live under `Engine`.
- ActorSequence editing UI should live under `Editor`.
- Curve editor UI should be Editor-only.
- GameClient must not compile Editor-only curve editor or details UI.

## Recommended UI Direction

### Decision

Use both:

- Details Panel: compact ActorSequence track inspector.
- Dedicated Curve Editor window/widget: full curve key/tangent editing.

### Why

ActorSequence can have multiple tracks. Embedding a full curve editor inside component Details would make the Details panel too large and would mix three responsibilities:

- component property inspection,
- sequence track editing,
- reusable curve asset editing.

The Details panel should answer:

- Which tracks exist?
- Which target component/property does each track drive?
- Which curve asset is assigned?
- What are playback settings?
- Can I preview/play/stop/scrub this actor-local sequence?

The dedicated Curve Editor should answer:

- What keys are in the curve asset?
- How are tangents/interpolation edited?
- What does the curve evaluate to over time?
- Save/load the `UCurveFloatAsset`.

### First Pass Policy

First implementation should not build a full sequencer window.

Use:

- `EditorPropertyWidget`: compact Details integration for `UActorSequenceComponent`.
- `EditorCurveEditorWidget`: standalone reusable curve asset editor opened from Details or Content Browser.

Details may show a small curve preview thumbnail later, but not the full Bezier/key editor.

## Target Runtime File Layout

```text
NipsEngine/Source/Engine
  Asset/
    CurveFloatAsset.h
    CurveFloatAsset.cpp
    CurveAssetLoader.h
    CurveAssetLoader.cpp

  Animation/
    CurvePlayback.h
    CurvePlayback.cpp
    TimelinePlayer.h
    TimelinePlayer.cpp
    ActorSequence.h
    ActorSequence.cpp

  Component/
    ActorSequenceComponent.h
    ActorSequenceComponent.cpp

  Core/
    Guid.h
    Guid.cpp
```

Names can be adjusted to match project convention before implementation, but responsibilities should stay separated.

Current implementation note:

- ActorSequence runtime data and player currently live in `Engine/Animation/ActorSequence.*`.
- `FPropertyTrackBinder` is intentionally not implemented yet because the animatable-property / Reflection decision is still gated.
- Component guid migration currently runs from scene load, not a separate `SceneGuidMigration` file, to keep this batch small.

## Target Editor File Layout

```text
NipsEngine/Source/Editor
  UI/
    EditorCurveEditorWidget.h
    EditorCurveEditorWidget.cpp
    EditorActorSequenceDetails.h
    EditorActorSequenceDetails.cpp
```

Details integration should be kept out of `UActorSequenceComponent` as much as possible.

`UActorSequenceComponent::GetEditableProperties()` should expose only simple values if needed. Track list editing should be drawn by an Editor-only helper/widget because it needs combos, curve asset picker, preview buttons, and track mutation.

## Core Runtime Types

### Curve Data

- `FCurveKey`
- `FFloatCurve`
- `UCurveFloatAsset`

Responsibilities:

- store keys,
- sort keys,
- evaluate Constant / Linear / Cubic,
- serialize curve data,
- remember asset path.

Not responsible for:

- playback state,
- component property modification,
- editor preview restore.

### Playback

- `FCurvePlaybackDesc`
- `FSequenceCurvePlaybackDesc`
- `FCurvePlaybackEvaluator`
- `FCurvePlaybackEvalResult`

Responsibilities:

- shared time mapping for Timeline and ActorSequence,
- loop handling,
- normalized-time and curve-time evaluation.

### Timeline

- `FTimelineFloatTrack`
- `FTimelinePlayer`

Responsibilities:

- lightweight owned-by-caller playback,
- callbacks with evaluated float values.

Not responsible for:

- target component binding,
- property reflection,
- editor preview state.

### ActorSequence

- `FSequenceObjectBinding`
- `FActorSequenceTrack`
- `FResolvedActorSequenceTrack`
- `UActorSequence`
- `UActorSequencePlayer`
- `UActorSequenceComponent`

Responsibilities:

- store actor-local tracks,
- resolve target component/property,
- evaluate tracks,
- apply runtime or editor-preview values.

## Persistent Component Guid Policy

The spec asks for `FGuid`, but this engine currently has only `uint32 UUID`.

Recommended implementation:

1. Add `FGuid` in `Engine/Core/Guid.h`.
2. Store `FGuid PersistentGuid` on `UActorComponent`.
3. Serialize it in `UActorComponent::Serialize()`.
4. Generate one when a component is created or loaded without a valid guid.
5. Regenerate it on normal actor/component duplication.
6. Preserve it only for explicit load/undo/redo restore paths.

Why not reuse `uint32 UUID`:

- current UUID is runtime/object identity and save/load identity,
- ActorSequence needs a stable component binding identity that survives load but changes on normal duplicate,
- keeping these separate is easier to reason about.

Implementation note:

- Existing scene files will not have `PersistentGuid`.
- `SceneGuidMigration` should fill missing component guids after scene load.
- Duplicate guid validation should run after load, but must have an option to preserve during undo/redo style restore.

## Animatable Property Policy

Use the existing manual property descriptor path as the first-pass reflection layer:

```cpp
struct FPropertyDescriptor
{
    const char* Name;
    EPropertyType Type;
    void* ValuePtr;
    EPropertyUsageFlags UsageFlags;
};
```

`GetEditableProperties()` remains the current collector name for compatibility, but descriptors now carry usage:

- `Editable`: Details can draw it.
- `Animatable`: ActorSequence can bind it.

Reason:

- The engine already uses `FPropertyDescriptor` as a manual reflection layer.
- ActorSequence can reuse the same component/property discovery path without adding a second reflection API.
- Runtime and preview state stay in `UActorSequencePlayer`, not in the descriptor.

First pass animatable targets:

- `ULightComponent` / `UPointLightComponent`
  - intensity-like float properties if present,
  - attenuation radius if useful.
- `UCameraComponent`
  - FOV,
  - OrthoWidth.
- `USceneComponent`
  - location/rotation/scale are marked animatable for future Vec3 tracks.

Vec3/Color/Transform tracks should be deferred until float pipeline is stable.

## Property Apply Policy

Use descriptor-based resolve in `UActorSequencePlayer` for the first pass.

Apply modes:

- Direct
- Add
- Multiply

Lerp is deferred.

Preview vs Runtime:

- Runtime writes through the resolved descriptor `ValuePtr`.
- Editor preview writes through the same path and uses `EPropertyChangeType::Preview`.
- Stop restores the cached base float value when previewing or when `bRestoreOnStop` is true.

Important:

- This is intentionally not a full Reflection system.
- `FPropertyDescriptor` must not own editor UI state, curve state, preview caches, or runtime track data.

## Serialization Policy

ActorSequenceComponent serialized data:

- `bAutoPlay`
- `bRestoreOnStop`
- `UActorSequence` inline data for first pass
- sequence duration / loop
- tracks
- binding guid
- target object persistent guid
- target object name fallback
- target property path
- curve asset path
- playback desc
- apply mode

Do not serialize:

- `UActorSequencePlayer`
- current time
- resolved target pointer
- resolved property descriptor
- timeline callbacks
- Lua callback references
- editor preview runtime state

## Curve Asset File Policy

Recommended first extension:

- `*.curve.json`

Recommended asset root:

- `NipsEngine/Asset/Curves`

ResourceManager integration:

- add `LoadCurve(Path)`
- add `SaveCurve(Path, CurveAsset)`
- add `GetCurvePaths()`

Question before implementation:

- Should curves be managed directly by `FResourceManager`, or should we introduce a small `FCurveAssetLoader` and keep `FResourceManager` as facade only?

Recommended:

- `FCurveAssetLoader` owns JSON parsing/writing.
- `FResourceManager` only caches and exposes curves, matching the direction of making ResourceManager a facade over focused loaders.

## Lua Policy

Timeline Lua:

- expose Timeline creation through Lua only after C++ timeline is stable.
- prefer LuaScriptComponent-owned timelines so callbacks are cleaned up when the script component is destroyed.

ActorSequence Lua:

- expose `Play`, `Pause`, `Stop` first.
- expose `AddFloatTrack` after runtime binding is stable.

Do not bind CameraModifier curve channels in this scope.

## ImGui Curve Widget Policy

The provided ImGui Bezier widget is useful as reference, but should not be copied as-is into Details.

Adaptation needed:

- remove namespace-level generic ImGui pollution where possible,
- place under an Editor-only widget file,
- support multiple keys, not only a single easing Bezier,
- separate draw code from `UCurveFloatAsset` data mutation,
- make it work with the engine's curve key/tangent model.

First curve editor milestone:

- draw grid,
- draw float curve,
- add/remove/select keys,
- edit time/value,
- save/load curve asset.

Tangent handle editing can be second pass if time is tight.

## Batch Plan

### Batch 1: Planning and Build Guard

- Create this roadmap.
- Confirm ambiguous decisions.
- Add project entries only after code begins.

Status: Done.

### Batch 2: Curve Runtime Data

- Add `FGuid` if approved.
- Add `FCurveKey`, `FFloatCurve`, `UCurveFloatAsset`.
- Add curve JSON serialization.
- Add focused unit/manual test path if the project has a suitable pattern.

Status: Done.

Notes:

- Added `FGuid`.
- Added `UCurveFloatAsset`, `FFloatCurve`, and `FCurveKey`.
- Supports Constant / Linear / Cubic Hermite evaluation.

### Batch 3: Resource Loading

- Add `FCurveAssetLoader`.
- Add `FResourceManager::LoadCurve`, `FindCurve`, `GetCurvePaths`.
- Add Content Browser recognition for `.curve.json` if needed.

Status: Done.

Notes:

- Added `FCurveAssetLoader`.
- Added `FResourceManager::LoadCurve`, `FindCurve`, `SaveCurve`, `GetCurvePaths`.
- Added `FAssetQueryService::GetCurvePaths`.
- Asset scan recognizes `.curve` and `.curve.json`.
- Content Browser can create a default `.curve.json` asset.
- Content Browser tiles/details recognize curve assets and expose a small read-only summary.

### Batch 4: Playback Core

- Add `FCurvePlaybackDesc`.
- Add `FCurvePlaybackEvaluator`.
- Add `FTimelinePlayer`.
- Add simple C++ validation path.

Status: Done.

Notes:

- Added `FCurvePlaybackDesc`, `FSequenceCurvePlaybackDesc`, `FCurvePlaybackEvaluator`.
- Added `FTimelinePlayer`.
- Lua Timeline creation is not bound yet because Lua callback lifetime should be owned by `ULuaScriptComponent`.

### Batch 5: Persistent Component Guid

- Add component persistent guid.
- Add serialize/load migration.
- Add duplicate regenerate policy.
- Add duplicate guid validation after scene load.

Status: Done.

Notes:

- `UActorComponent` now owns `PersistentGuid`.
- Missing guids are generated during serialize/load.
- Normal duplicate regenerates component persistent guid.
- Scene load validates uniqueness and regenerates duplicate component guids.

### Batch 6: Animatable Properties

- Add `EPropertyUsageFlags`.
- Add `FPropertyChangedEvent` and UE-like `PostEditChangeProperty`.
- Mark first animatable descriptors on scene/camera/light components.

Status: Done for first pass.

Done:

- `FPropertyDescriptor` now has `UsageFlags`.
- `FPropertyChangedEvent` and `EPropertyChangeType` are available.
- `UObject::PostEditChangeProperty()` routes to existing `PostEditProperty()` overrides for compatibility.
- Transform, camera FOV/ortho width, and light float/color candidates are marked animatable.

Deferred:

- Full rename from `GetEditableProperties()` to a broader collector name.
- Vec3/Color track application.

### Batch 7: ActorSequence Runtime

- Add sequence data structs.
- Add `UActorSequence`.
- Add `UActorSequencePlayer`.
- Add `FPropertyTrackBinder`.
- Add `UActorSequenceComponent`.

Status: Done for first pass.

Done:

- Added `FSequenceObjectBinding`, `FActorSequenceTrack`, `FResolvedActorSequenceTrack`.
- Added `UActorSequence`.
- Added `UActorSequencePlayer`.
- Added `UActorSequenceComponent`.
- Added component guid based target resolve and name fallback.
- Added curve evaluation cache.
- Added descriptor-based float property resolve/apply.
- Stop restores cached base float values for editor preview or when `bRestoreOnStop` is true.
- Resolved track cache now validates owner/component liveness before applying or restoring values, preventing dangling component pointer use after external component deletion.
- If a bound component is deleted externally, serialized track data remains intact, but the resolved runtime cache is invalidated and marked dirty instead of dereferencing the stale component pointer.

Deferred:

- Vec3/Color/Transform track application.
- Dedicated `FPropertyTrackBinder` can still be introduced later if apply logic grows.

### Batch 8: ActorSequence Details UI

- Add compact Details integration.
- Track add/remove.
- Target component combo.
- Target property combo.
- Curve path picker.
- Preview play/pause/stop/scrub.

Status: Partially done.

Done:

- `EditorPropertyWidget` draws a compact `UActorSequenceComponent` section.
- Track add/remove is available from Details.
- Target component combo is filtered to components that expose animatable float descriptors.
- Target property combo is driven by existing `FPropertyDescriptor` data.
- Curve asset assignment supports ResourceManager curve path combo and Content Browser drag/drop.
- Track playback fields are editable: start time, duration, play rate, loop, apply mode, and time mapping.
- Preview play/pause/stop/scrub is available from Details.
- Runtime `SequencePlayer` and EditorPreview `PreviewSequencePlayer` are separate on `UActorSequenceComponent`, so Details preview does not mutate the runtime player context.

Deferred:

- A dedicated `EditorActorSequenceDetails` helper can be split out after the UI shape stabilizes.

### Batch 9: Curve Editor Window

- Add standalone `EditorCurveEditorWidget`.
- Open from Content Browser or ActorSequence track.
- Save/load `UCurveFloatAsset`.
- Keep this separate from Details.

Status: Done for first pass.

Done:

- Added standalone `EditorCurveEditorWidget`.
- Content Browser double-click opens curve assets in the Curve Editor.
- ActorSequence Details track rows can open their assigned curve directly in the Curve Editor.
- Curve Editor can add/remove/select keys.
- Curve Editor can edit key time/value/interpolation/tangent values.
- Curve Editor can save and reload through `FResourceManager::SaveCurve` / `LoadCurve`.
- Curve Editor is Editor-only and excluded from GameClient configurations.

Deferred:

- Graphical tangent handle dragging.
- Reference preview against ActorSequence tracks.

### Batch 10: Lua Bindings

- Bind Timeline creation/playback through LuaScriptComponent-owned lifetime.
- Bind ActorSequence play/pause/stop.
- Bind AddFloatTrack only after runtime/editor track mutation is stable.

Status: Partially done.

Done:

- `Engine.API.Asset.GetCurvePaths`.
- `Engine.API.Asset.LoadCurve`.
- `CurveFloatAsset:Evaluate`.
- `ActorSequenceComponent:Play/Pause/Stop`.
- `ActorSequencePlayer` basic control binding.
- `ActorSequenceComponent:AddFloatTrack(desc)` for float tracks.

Deferred:

- Lua-owned Timeline callback API.

Reason:

- Lua-owned Timeline callback lifetime should be owned by `ULuaScriptComponent`, so it remains deferred until that ownership path is added.

## Current Progress

Overall progress: about 91%.

Verified builds:

- `Debug|x64`: success, warnings 0, errors 0.
- `GameClientDebug|x64`: success, warnings 0, errors 0.

Latest verification includes the descriptor-based animatable property batch and the first ActorSequence Details editing pass.

Latest verification also includes the first standalone Curve Editor pass and dangling component protection in `UActorSequencePlayer`.

## Decisions Needed Before Code

1. `FGuid` introduction:
   - Decision: add real `FGuid` type instead of reusing `uint32 UUID`.
   - Reason: this follows the UE-style concept and keeps runtime object UUID separate from persistent sequence binding identity.
   - Existing scenes must be handled by load-time migration: missing component persistent guids are generated after load and written on next save.

2. Curve editor location:
   - Decision: full editor as standalone Editor widget/window.
   - Details only gets compact track inspector and optional small preview.

3. First editor scope:
   - Option A: Details-based ActorSequence editing first, Curve Editor second.
   - Option B: Curve Editor first, ActorSequence Details second.
   - Decision: Option A. Details-based ActorSequence editing comes first, Curve Editor comes after the data path is proven.

4. Curve asset loader ownership:
   - Decision: `FCurveAssetLoader` plus `FResourceManager` facade.

5. First animatable properties:
   - Decision: reuse the existing manual `FPropertyDescriptor` path.
   - Do not add a separate Reflection layer for this pass.
   - `UsageFlags` separates Details-visible properties from ActorSequence-bindable properties.

## Required Reconfirmation Gate

Closed for second pass.

Decision:

- Vec3 / Color / Transform track application uses sub-tracks.
- Each channel is driven by a float curve.
- Examples: `Transform.Location.X`, `Transform.Location.Y`, `Light.Color.R`, `Light.Color.G`.
- This keeps the current `UCurveFloatAsset` reusable and avoids introducing separate vector/color curve assets before the float pipeline is fully proven.
- `GetEditableProperties()` stays for now. `EPropertyUsageFlags` remains the shared bridge for Details and ActorSequence binding.

## Second-Pass Batch Plan

CameraModifier integration is excluded from this pass because another developer owns that workstream.

### Second Pass Batch 0: UE-like Sequence Model

- Replace the first-pass flat track model with a lightweight UE-like hierarchy.
- Runtime data shape is now:

```text
UActorSequence
  FActorSequenceBinding
    FActorSequenceTrack
      FActorSequenceSection
        FActorSequenceChannel
```

- Existing first-pass flat save data is migrated into one Binding / Track / Section / `Value` Channel on load.
- Float tracks are represented as a `Value` channel.
- Vec3 and Color properties can be represented as scalar channels such as `X/Y/Z` and `R/G/B/A`.

Status: Done.

### Second Pass Batch 1: EditorActorSequenceDetails Split

- Move ActorSequence Details rendering out of `EditorPropertyWidget`.
- Keep `EditorPropertyWidget` responsible for selection and generic property rendering only.
- Keep track mutation, curve picker, preview play/pause/stop/scrub in an Editor-only helper.

Status: Done.

Done:

- Added `EditorActorSequenceDetails.h/.cpp`.
- `EditorPropertyWidget` now delegates ActorSequence component UI to the Editor-only helper.
- Track mutation, curve picking, preview play/pause/stop, and scrub handling are contained in the helper.

### Second Pass Batch 2: Curve Editor Interaction

- Add graphical tangent handle dragging.
- Keep numeric tangent editing as the fallback and exact-edit path.

Status: Done.

Done:

- Selected cubic keys now draw arrive/leave tangent handles in the Curve Editor canvas.
- Dragging a handle updates `ArriveTangent` or `LeaveTangent` and switches the key to `User` tangent mode.
- Numeric tangent editing remains available in the key table.

### Second Pass Batch 3: Curve Reference Preview

- Curve Editor can preview the current curve through ActorSequence tracks that reference it.
- Preview must use `EditorPreview` context and restore values on stop/close.
- Preview must not mutate runtime `SequencePlayer`.

Status: Done.

Done:

- Curve Editor can start/stop reference preview for ActorSequence components that reference the open curve path.
- Preview uses each component's preview player and calls `StopPreview` to restore editor values.
- Runtime `SequencePlayer` state is not touched.

### Second Pass Batch 4: Lua-owned Timeline

- `ULuaScriptComponent` owns Lua-created timelines.
- Lua callback lifetime is tied to the owning script component.
- Destroying or reloading the script component clears callbacks and timelines.

Status: Done.

Done:

- Added `FLuaTimeline` as the Lua-facing wrapper over `FTimelinePlayer`.
- Script environments now expose `Timeline.New()`.
- `UScriptComponent` owns created timelines, ticks them every frame, and clears them on reload/end play.

### Second Pass Batch 5: Sub-track Property Channels

- Extend animatable descriptors or binding paths so scalar sub-channels can target Vec3/Color/Transform fields.
- Keep runtime apply path float-based.
- Add UI support for selecting channel paths.

Status: Done.

Done:

- Runtime apply path can read/write `Float`, `Vec3`, and `Color` scalar channels.
- Details target component filtering now includes animatable `Float`, `Vec3`, and `Color` descriptors.
- Details UI exposes a `Channel` combo for `Value`, `X/Y/Z`, or `R/G/B/A` depending on track type.
- The previous Target Component empty-list issue on transform-only actors is addressed by treating Vec3 transform properties as channel-capable animatable targets.
- Track headers now show scalar target names such as `Location.X` or `Color.R`.
- Details UI provides `Add All Channels` and per-track `Add Missing Channels` shortcuts for Vec3/Color properties.

## Current Recommendation

For the next implementation pass:

1. Confirm the five decisions above.
2. Start with Curve runtime data and ResourceManager facade.
3. Build `FTimelinePlayer` before ActorSequence.
4. Add ActorSequence runtime without full UI.
5. Add compact Details UI.
6. Add standalone Curve Editor after the data path is proven.

This keeps the system testable in small pieces and avoids turning Details into a large sequencer surface too early.
