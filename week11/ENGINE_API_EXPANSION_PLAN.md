# Engine API Expansion Plan

## Goal

Make `Engine.API` the single Lua-facing standard library for global engine services.

The Lua authoring style should feel Unity-like:

```lua
local actor = Engine.API.World.FindActorByName("Enemy_01")
actor.Location = Vector(0, 0, 100)
actor:AddTag("Enemy")
```

But the ownership split should stay Unreal-like internally:

```text
Engine.API
-> global services and systems

Actor / Component bindings
-> per-instance object state and behavior
```

## Official Root

Use only:

```lua
Engine.API
```

Avoid:

```lua
GameAPI
GM.API
```

## Responsibility Boundary

### Engine.API Owns

Global engine/system access:

- World queries and actor factory/destruction entry points
- Time and timescale
- Input polling
- Audio system playback/mixing
- Runtime UI global facade
- Save/load file access under safe roots
- Debug logging and later debug draw
- Asset lookup/loading helpers
- Scene/level flow once runtime scene loading is finalized
- Effect spawning/pooling once effect system is finalized

### Engine.API Does Not Own

Per-object state mutation:

- Actor transform changes
- Actor tags
- Actor visibility/active state
- Actor name
- Component lookup from a specific actor
- Component property mutation
- SoundComponent playback for a specific actor
- StaticMeshComponent material/mesh changes
- CameraComponent/SpringArmComponent per-instance values

These belong to Actor/Component Lua bindings.

Preferred:

```lua
actor.Location = Vector(0, 0, 100)
actor:AddTag("Enemy")
local sound = actor:Get_Component_By_Type("SoundComponent")
sound:Play()
```

Avoid:

```lua
Engine.API.World.SetActorLocation(actor, Vector(0, 0, 100))
Engine.API.Actor.AddTag(actor, "Enemy")
```

## Existing Actor / Component Binding Surface

Already available in the current binding layer:

### Object

```lua
object:GetUUID()
object:GetName()
object:GetType()
object.Name
object.Type
```

### Actor

```lua
actor.Name
actor.Location
actor.Rotation
actor.Scale
actor.UID
actor.RootComponent
actor.Active
actor.Visible
actor.TickInEditor

actor:Duplicate()
actor:GetActorForwardVector()
actor:AddTag(tag)
actor:RemoveTag(tag)
actor:HasTag(tag)
actor:ClearTags()
actor:GetTags()
actor:Get_Components()
actor:Get_Component_By_Type(typeName)
actor:Get_Static_Mesh_Component()
```

### ActorComponent

```lua
component:GetOwner()
component:IsActive()
component:SetActive(active)
component:IsAutoActivate()
component:SetAutoActivate(autoActivate)
component:IsComponentTickEnabled()
component:SetComponentTickEnabled(tickEnabled)

component.Owner
component.Active
component.AutoActivate
component.TickEnabled
component.EditorOnly
```

### SceneComponent

```lua
sceneComponent:GetParent()
sceneComponent:AttachToComponent(parent)
sceneComponent.Location
sceneComponent.Forward
```

### SoundComponent

```lua
soundComponent.Sound
soundComponent:Play()
soundComponent:Stop()
soundComponent:IsPlaying()
```

## Current Engine.API Surface

### Time

Status: completed.

```lua
Engine.API.Time.GetDeltaTime()
Engine.API.Time.GetUnscaledDeltaTime()
Engine.API.Time.GetGameTime()
Engine.API.Time.GetRealTime()
Engine.API.Time.SetTimeScale(scale)
Engine.API.Time.GetTimeScale()
```

### Input

Status: completed first pass.

Names:

- `IsKeyDown` means held this frame.
- `IsKeyPressed` means pressed this frame.
- `IsKeyReleased` means released this frame.

```lua
Engine.API.Input.IsKeyDown(key)
Engine.API.Input.IsKeyPressed(key)
Engine.API.Input.IsKeyReleased(key)
Engine.API.Input.IsMouseDown(button)
Engine.API.Input.IsMousePressed(button)
Engine.API.Input.IsMouseReleased(button)
Engine.API.Input.GetMousePosition()
Engine.API.Input.GetMouseDelta()
Engine.API.Input.GetScrollDelta()
Engine.API.Input.GetScrollNotches()
Engine.API.Input.IsAnyMouseButtonDown()
```

Supported key/button style:

```lua
Engine.API.Input.IsKeyDown("W")
Engine.API.Input.IsKeyPressed("Escape")
Engine.API.Input.IsKeyPressed("LeftShift")
Engine.API.Input.IsMousePressed("LeftMouse")
Engine.API.Input.IsMousePressed("LMB")
Engine.API.Input.IsKeyDown(0x57) -- numeric VK escape hatch
```

### World

Status: completed.

World is allowed to search, enumerate, spawn, destroy, and validate actors. It should not become an actor mutation namespace.

```lua
Engine.API.World.FindActorByName(name)
Engine.API.World.FindActorsByName(name)
Engine.API.World.FindActorByTag(tag)
Engine.API.World.FindActorsByTag(tag)
Engine.API.World.FindActorsByTags(tags)
Engine.API.World.FindActorsByType(typeName)
Engine.API.World.GetAllActors()
Engine.API.World.GetActorCount()
Engine.API.World.IsValidActor(actor)
Engine.API.World.SpawnActor(typeName)
Engine.API.World.DestroyActor(actor)
```

### Audio

Status: completed first pass.

```lua
Engine.API.Audio.PlayBGM(pathOrKey[, fadeIn])
Engine.API.Audio.StopBGM([fadeOut])
Engine.API.Audio.PlaySFX(pathOrKey[, volumeScale])
Engine.API.Audio.PlaySFX3D(pathOrKey, position[, volumeScale])
Engine.API.Audio.SetMasterVolume(volume)
Engine.API.Audio.SetBGMVolume(volume)
Engine.API.Audio.SetSFXVolume(volume)
Engine.API.Audio.GetMasterVolume()
Engine.API.Audio.GetBGMVolume()
Engine.API.Audio.GetSFXVolume()
Engine.API.Audio.SetListener(position, forward, up)
Engine.API.Audio.SetPlaybackPolicy(pathOrKey, maxConcurrent, cooldownSeconds)
Engine.API.Audio.RegisterSound(key, path)
Engine.API.Audio.ResolveSoundPath(keyOrPath)
Engine.API.Audio.ReloadSoundRegistry()
Engine.API.Audio.StopAll()
```

### UI

Status: completed first pass.

Runtime UI remains global because it owns screens/canvases rather than specific actors.

```lua
Engine.API.UI.ShowScreen(screenId[, canvasId])
Engine.API.UI.CreateText(screenId, widgetId, text, x, y, w, h[, parentId])
Engine.API.UI.CreateButton(screenId, widgetId, text, eventName, x, y, w, h[, parentId])
Engine.API.UI.CreateImage(screenId, widgetId, imagePath, x, y, w, h[, parentId])
Engine.API.UI.CreateProgressBar(screenId, widgetId, value, x, y, w, h[, parentId])
Engine.API.UI.RemoveWidget(widgetId)
Engine.API.UI.SetText(widgetId, text)
Engine.API.UI.SetImage(widgetId, imagePath)
Engine.API.UI.SetProgress(widgetId, value)
Engine.API.UI.SetVisible(widgetId, visible)
Engine.API.UI.SetEnabled(widgetId, enabled)
Engine.API.UI.SetActionEvent(widgetId, eventName)
Engine.API.UI.PollActionEvents()
```

### Save

Status: completed.

Lua owns gameplay data shape and JSON conversion. C++ only provides safe text file access.

```lua
Engine.API.Save.WriteText("SaveData/player.json", jsonText)
Engine.API.Save.ReadText("SaveData/player.json")
Engine.API.Save.Exists("SaveData/player.json")
Engine.API.Save.Delete("SaveData/player.json")
```

Rules:

- Paths stay inside `Saves/`.
- Absolute paths and `..` escape attempts fail.
- Missing read returns `nil`.

### Debug

Status: completed first pass.

```lua
Engine.API.Debug.Log(message)
Engine.API.Debug.Warn(message)
Engine.API.Debug.Error(message)
```

### Effect

Status: placeholder only.

### Asset

Status: completed first pass.

Asset API is intentionally metadata/path-only.

```lua
Engine.API.Asset.NormalizePath(path)
Engine.API.Asset.Exists(path)
Engine.API.Asset.GetTexturePaths()
Engine.API.Asset.GetStaticMeshPaths()
Engine.API.Asset.GetMaterialPaths()
Engine.API.Asset.GetScenePaths()
Engine.API.Asset.GetSoundPaths()
```

Rules:

- Returned paths are normalized `Asset/...` relative strings.
- Absolute paths and `..` escape attempts return `nil`/`false`.
- Resource pointer loading and per-component assignment stay outside `Engine.API`.

### Random

Status: completed.

```lua
Engine.API.Random.SetSeed(seed)
Engine.API.Random.RandomFloat01()
Engine.API.Random.RandomFloat(min, max)
Engine.API.Random.RandomInt(min, max)
Engine.API.Random.RandomBool(probability)
```

Notes:

- `RandomFloat` uses float distribution between `min` and `max`.
- `RandomInt` is inclusive on both ends.
- `RandomFloat01` returns a 0..1 value.
- No alias names are exposed so Lua gameplay code has one official spelling.

### Application

Status: completed first pass.

```lua
Engine.API.Application.QuitGame()
```

Rules:

- GameClient: posts a window close request.
- Editor/PIE: ignored with a log message so Lua does not accidentally close the editor.

## Expansion Candidates

### Batch API-1 - Reference and Guardrails

Priority: highest.

Deliverables:

- Create `ENGINE_API_REFERENCE.md`.
- Document current API signatures, returns, examples, and failure behavior.
- Add a tiny Lua smoke-test script later after team Lua template settles.
- Keep this batch mostly documentation to avoid churn during template merge.

### Batch API-2 - Input

Priority: highest for gameplay scripting.

Rationale:

Lua gameplay cannot avoid C++ logic without stable input access.

Target:

```lua
Engine.API.Input.IsKeyDown(key)
Engine.API.Input.IsKeyPressed(key)
Engine.API.Input.IsKeyReleased(key)
Engine.API.Input.IsMouseDown(button)
Engine.API.Input.IsMousePressed(button)
Engine.API.Input.IsMouseReleased(button)
Engine.API.Input.GetMousePosition()
Engine.API.Input.GetMouseDelta()
Engine.API.Input.GetScrollDelta()
Engine.API.Input.GetScrollNotches()
```

Status: completed first pass.

Design stance:

- `Engine.API.Input` is a low-level polling service, not the final gameplay input architecture.
- It is useful for quick scripts, prototypes, and manager-level input collection.
- Long-term gameplay code should usually read input through a Lua-side `InputManager` or `InputAction` layer.
- That Lua layer can translate raw keys/buttons into semantic actions:

```lua
InputAction.Attack
InputAction.MoveForward
InputAction.SlowTime
InputAction.Pause
```

Recommended usage:

```lua
-- Good: one Lua manager polls Engine.API.Input and emits semantic events.
if Engine.API.Input.IsMousePressed("LMB") then
    GM:Emit("Input.AttackPressed")
end

-- Avoid long-term: every enemy/UI/gameplay script polling raw keys directly.
if Engine.API.Input.IsKeyDown("W") then
    ...
end
```

Notes:

- Key names should support friendly strings like `"W"`, `"LeftMouse"`, `"Escape"`.
- Numeric VK codes can remain supported as an escape hatch.
- Input API masks captured mouse/keyboard input when Runtime UI or Editor GUI owns that device class.
- Editor-only focus/capture details should not leak into Lua API unless needed.

### Batch API-3 - Time

Priority: high.

Target:

```lua
Engine.API.Time.GetDeltaTime()
Engine.API.Time.GetUnscaledDeltaTime()
Engine.API.Time.GetGameTime()
Engine.API.Time.GetRealTime()
Engine.API.Time.SetTimeScale(scale)
Engine.API.Time.GetTimeScale()
```

Notes:

- Completed.
- `GetDeltaTime` follows the same scaled delta used by PIE/Game world ticking.
- `GetUnscaledDeltaTime` exposes raw frame delta.
- `GetGameTime` accumulates scaled delta, while `GetRealTime` accumulates raw delta.

### Batch API-4 - Asset

Priority: high for data-driven Lua.

Target:

```lua
Engine.API.Asset.Exists(path)
Engine.API.Asset.NormalizePath(path)
Engine.API.Asset.GetTexturePaths()
Engine.API.Asset.GetStaticMeshPaths()
Engine.API.Asset.GetMaterialPaths()
Engine.API.Asset.GetScenePaths()
Engine.API.Asset.GetSoundPaths()
```

Notes:

- Completed first pass.
- Do not expose raw renderer/resource pointers.
- Asset API should return strings and simple metadata only.
- Actual assignment belongs to components:
  - `staticMeshComponent:SetStaticMesh(...)`
  - `soundComponent.Sound = ...`

### Batch API-5 - Debug Draw

Priority: medium-high for game jam iteration.

Target:

```lua
Engine.API.Debug.DrawLine(start, finish, color[, duration])
Engine.API.Debug.DrawSphere(center, radius, color[, duration])
Engine.API.Debug.DrawBox(center, extent, color[, duration])
Engine.API.Debug.DrawText(position, text, color[, duration])
```

Notes:

- Should use runtime-safe debug render primitives.
- Must be compiled into GameClient debug/development if useful.
- Shipping behavior can be no-op later.

### Batch API-6 - Scene

Priority: deferred.

Target:

```lua
-- Runtime scene load/reload intentionally deferred.
Engine.API.Application.QuitGame()
```

Notes:

- `LoadScene` and `ReloadScene` are intentionally not exposed yet.
- `QuitGame` belongs under `Application`, not `Scene`.
- Runtime scene loading needs a separate world replacement/script reset policy.

### Batch API-7 - Random

Priority: medium.

Target:

```lua
Engine.API.Random.Range(min, max)
Engine.API.Random.RangeInt(min, max)
Engine.API.Random.SetSeed(seed)
Engine.API.Random.Chance(probability)
```

Notes:

- Completed.
- Lua has `math.random`, but an engine RNG wrapper gives deterministic game systems later.

### Batch API-8 - Effect

Priority: medium-low until effect system stabilizes.

Target shape:

```lua
Engine.API.Effect.Spawn(effectId, position[, rotation])
Engine.API.Effect.Stop(handle)
Engine.API.Effect.StopAllById(effectId)
```

Notes:

- Needs a native effect registry/pool decision first.
- Should not be tied to Editor-only preview systems.

## Things To Keep Out For Now

Do not add these to `Engine.API` until the lower-level binding shape is settled:

- `Engine.API.Actor.*`
- `Engine.API.Component.*`
- `Engine.API.Material.*` per-slot editing
- `Engine.API.Camera.*` per-camera mutation
- `Engine.API.Physics.*` unless physics system exists
- `Engine.API.Navigation.*` unless navigation system exists

## Recommended Immediate Batches

1. API-1 Reference and guardrails.
2. API-2 Input. - completed first pass
3. API-3 Time delta/accessors. - completed
4. API-4 Asset metadata/path helpers. - completed first pass
5. API-5 Debug draw if a runtime-safe draw path already exists.

This order maximizes Lua gameplay velocity without bloating `Engine.API` into an object-manipulation namespace.

## PlayerController Direction

`AGameJamPlayerController` was removed after input/gameplay logic moved toward Lua.

Current rule:

- `APlayerController` owns runtime bootstrap:
  - spawn default pawn
  - find `APlayerStart`
  - possess pawn
  - set view target
  - drive active runtime camera from the view target
- `APlayerController` does not implement default WASD/mouse gameplay movement.
- Lua reads `Engine.API.Input` through a Lua-side input manager and moves/rotates actors or components through Actor/Component bindings.
- Packaging and PIE default to `APlayerController`.

## Implementation Split Notes

Keep `FScriptManager` focused on Lua VM/script lifecycle and thin binding registration.

Recent API additions were split so engine behavior does not live inside `ScriptManager.cpp` anonymous namespace:

- `FEngineRandom`
  - Path: `NipsEngine/Source/Engine/Core/Random/EngineRandom.*`
  - Owns the engine RNG used by `Engine.API.Random`.
- `FAssetQueryService`
  - Path: `NipsEngine/Source/Engine/Asset/AssetQueryService.*`
  - Owns safe `Asset/...` path normalization and asset path listing used by `Engine.API.Asset`.
- `UEngine::RequestQuitGame`
  - Path: `NipsEngine/Source/Engine/Runtime/Engine.*`
  - Owns application quit behavior used by `Engine.API.Application.QuitGame`.

Engine.API binding code was also split by namespace:

- Shared declaration:
  - Path: `NipsEngine/Source/Engine/Runtime/Script/API/LuaEngineAPIBindings.h`
- Namespace binders:
  - Path: `NipsEngine/Source/Engine/Runtime/Script/API/LuaTimeAPI.cpp`
  - Path: `NipsEngine/Source/Engine/Runtime/Script/API/LuaInputAPI.cpp`
  - Path: `NipsEngine/Source/Engine/Runtime/Script/API/LuaSaveAPI.cpp`
  - Path: `NipsEngine/Source/Engine/Runtime/Script/API/LuaDebugAPI.cpp`
  - Path: `NipsEngine/Source/Engine/Runtime/Script/API/LuaWorldAPI.cpp`
  - Path: `NipsEngine/Source/Engine/Runtime/Script/API/LuaAudioAPI.cpp`
  - Path: `NipsEngine/Source/Engine/Runtime/Script/API/LuaUIAPI.cpp`
  - Path: `NipsEngine/Source/Engine/Runtime/Script/API/LuaAssetAPI.cpp`
  - Path: `NipsEngine/Source/Engine/Runtime/Script/API/LuaRandomAPI.cpp`
  - Path: `NipsEngine/Source/Engine/Runtime/Script/API/LuaApplicationAPI.cpp`
  - Path: `NipsEngine/Source/Engine/Runtime/Script/API/LuaEffectAPI.cpp`
- `FScriptManager::BindEngineAPI()` now only creates `Engine` / `API` tables and calls `FLuaEngineAPI::Bind*`.
- Verified:
  - `Debug|x64` build passed.
  - `GameClientDebug|x64` build passed.

Do not move unrelated Actor/Component bindings during this cleanup. That should be a later, separate refactor only after team merge pressure is lower.
