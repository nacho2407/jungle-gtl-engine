# Game Management Structure Plan

## Goal

Build a Lua-first management layer for the game jam project while keeping C++ dependencies minimal.

The runtime UI Lua bridge is intentionally deferred. This pass only establishes the gameplay management architecture.

## Rules

- One attachable entry script owns the management lifecycle.
- Individual managers are Lua modules.
- Managers communicate through `EventBus`, not direct cross-calls whenever possible.
- `GameManager` owns run state and score.
- No `ScoreManager`.
- No `CombatManager`.
- No `SceneFlowManager`; UI/game flow is represented by FSM states.
- C++ only supports module loading and GameClient Lua initialization.

## Runtime Shape

```text
Empty Actor
-> UScriptComponent
   -> GeneralManager.lua
      -> EventBus
      -> DataManager
      -> SoundManager
      -> GameManager
         -> StateMachine
            -> Boot / Title / Playing / Pause / Result
```

## Completed

- Added Lua module package path support:
  - `LuaScript/?.lua`
  - `LuaScript/?/init.lua`
- GameClient now initializes and shuts down `FScriptManager`.
- Removed the temporary Lua management implementation before the team Lua template merge:
  - `LuaScript/GeneralManager.lua`
  - `LuaScript/Game/Core/*`
  - `LuaScript/Game/Management/*`
- Management architecture is now documentation-only until the new team Lua template lands.

## Next

- Rebuild management Lua after the team Lua template is merged.
- Keep native engine/API work moving first:
  - `Engine.API`
  - Runtime UI Preview
  - Audio/System/Component
  - Save/Debug/World helpers
- Reintroduce `GeneralManager` / `GameManager` / FSM only after the final Lua module style is fixed.

## Pending Game API Bridge

Goal: expose a small C++ -> Lua game API surface without letting Lua scripts depend on editor-only systems.

Recommended Lua shape:

```lua
Engine.API.Time
Engine.API.World
Engine.API.Audio
Engine.API.UI
Engine.API.Effect
Engine.API.Save
Engine.API.Debug
```

`Engine.API` is the single official API root. `GM.API` and `GameAPI` aliases are intentionally not used.

Initial priority:

- `Engine.API.Time`
  - `SetTimeScale(scale)` - completed
  - `GetTimeScale()` - completed
- `Engine.API.World`
  - `FindActorByName(name)` - completed
  - `FindActorsByName(name)` - completed
  - `FindActorsByTag(tag)` - completed
  - `FindActorsByTags(tags)` - completed
  - `SpawnActor(typeName)` - completed
  - `DestroyActor(actor)` - completed
- `Engine.API.Audio`
  - Real `FAudioSystem` binding - completed
- `Engine.API.UI`
  - `ShowScreen(screenId[, canvasId])` - completed
  - `CreateText(screenId, widgetId, text, x, y, w, h[, parentId])` - completed
  - `CreateButton(screenId, widgetId, text, eventName, x, y, w, h[, parentId])` - completed
  - `CreateImage(screenId, widgetId, imagePath, x, y, w, h[, parentId])` - completed
  - `CreateProgressBar(screenId, widgetId, value, x, y, w, h[, parentId])` - completed
  - `SetText`, `SetImage`, `SetProgress`, `SetVisible`, `SetEnabled`, `SetActionEvent` - completed
  - `RemoveWidget(widgetId)` - completed
  - `PollActionEvents()` - completed
- `Engine.API.Save`
  - `WriteText(relativePath, text)` - completed
  - `ReadText(relativePath)` - completed
  - `Exists(relativePath)` - completed
  - `Delete(relativePath)` - completed
  - Paths are constrained to `Saves/` - completed
- `Engine.API.Debug`
  - `Log(message)` - completed
  - `Warn(message)` - completed
  - `Error(message)` - completed

## Pending Audio System

Decision: use SoLoud as the native audio backend unless integration risk changes.

Intended ownership:

```text
Lua SoundManager
-> Engine.API.Audio
-> C++ FAudioSystem
-> SoLoud
```

Lua responsibilities:

- Decide when to play BGM/SFX.
- React to game events.
- Use semantic sound names where possible.
- Avoid knowing SoLoud details directly.

C++ `FAudioSystem` responsibilities:

- Initialize/shutdown SoLoud. - completed
- Cache loaded audio sources. - completed
- Manage one active BGM channel. - completed
- Play fire-and-forget SFX. - completed
- Play positional 3D SFX. - completed
- Manage master/BGM/SFX volume. - completed
- Support fade in / fade out. - completed
- Support max concurrent voices per sound. - completed
- Support cooldown per sound key to prevent excessive overlap. - completed
- Support group-level volume and stop.
- Update listener transform each frame. - completed
- Call SoLoud `update3dAudio()`. - completed
- Later support occlusion/obstruction through engine collision/raycast policy.

Recommended first C++ API:

```cpp
bool Initialize();
void Shutdown();
void Tick(float DeltaTime);

void PlayBGM(const FString& KeyOrPath, float FadeInSeconds = 0.0f);
void StopBGM(float FadeOutSeconds = 0.0f);

void PlaySFX(const FString& KeyOrPath);
void PlaySFX3D(const FString& KeyOrPath, const FVector& Position);

void SetMasterVolume(float Volume);
void SetBGMVolume(float Volume);
void SetSFXVolume(float Volume);
```

Recommended later policy data:

```text
SoundKey
- Path
- Group
- Volume
- Loop
- FadeIn
- FadeOut
- MaxConcurrent
- CooldownSeconds
- bStopOldest
- MinDistance
- MaxDistance
- Rolloff
- AttenuationModel
```

Recommended Lua API:

```lua
Engine.API.Audio.PlayBGM("Asset/Audio/BGM_Run.ogg", 1.0)
Engine.API.Audio.StopBGM(1.0)
Engine.API.Audio.PlaySFX("Asset/Audio/Slash.wav")
Engine.API.Audio.PlaySFX3D("Asset/Audio/EnemyHit.wav", Actor:GetLocation())
Engine.API.Audio.SetMasterVolume(0.8)
```

Pending integration steps:

1. Add SoLoud under `NipsEngine/ThirdParty/SoLoud`. - completed
2. Add include/lib/source integration to `NipsEngine.vcxproj`. - completed
3. Add `Source/Engine/Audio/AudioSystem.h/.cpp`. - completed
4. Own `FAudioSystem` from `UEngine` or `UGameEngine`. - completed
5. Initialize/shutdown/tick audio with engine lifecycle. - completed
6. Replace old audio stub with real `Engine.API.Audio` calls. - completed
7. Add packaging copy rule for audio assets.
8. Add optional sound settings file later, such as `Settings/Sound.ini`.
9. Add `USoundComponent` as an actor-attached scene component. - completed
10. Add key-based sound lookup through `Settings/Sound.ini` or `Asset/Audio/SoundRegistry.ini`. - completed

Verification:

- `Debug|x64` build passed after SoLoud/AudioSystem integration.
- `GameClientDebug|x64` build passed after SoLoud/AudioSystem integration.
- `GameClientRelease|x64` build passed after SoLoud/AudioSystem integration.
- `Debug|x64`, `GameClientDebug|x64`, and `GameClientRelease|x64` also passed after `USoundComponent`, sound registry, listener auto-update, and Lua API expansion.

Current practical usage:

```lua
Engine.API.Audio.PlaySFX("Slash")
Engine.API.Audio.PlaySFX3D("EnemyHit", Vector(0, 0, 1))
Engine.API.Audio.PlayBGM("BGM_Run", 1.0)
Engine.API.Audio.SetPlaybackPolicy("Slash", 3, 0.05)
```

Management-side usage:

```lua
local Sound = GM:GetManager("Sound")
Sound:PlayBGM(Sound.BGM.Default)
Sound:PlaySFX(Sound.SFX.Default)
```

Registry file examples:

```ini
Slash=Asset/Audio/Slash.wav
BGM_Run=Asset/Audio/BGM_Run.ogg
EnemyHit=Asset/Audio/EnemyHit.wav
BGM_Default=Asset/Audio/BGM/Default.ogg
SFX_Default=Asset/Audio/SFX/Default.wav
```

Keep deferred:

- AudioVolume / reverb / occlusion volumes.
- SoundCue-style graph authoring.
- Per-group mixer UI.
- Complex attenuation asset authoring.

## Pending Tag System

Goal: every Actor can carry lightweight gameplay tags for Lua/API lookup.

Recommended direction:

- Add an Actor-level tag container first, not a separate required component. - completed
- Expose simple methods:
  - `Actor:AddTag(tag)` - completed
  - `Actor:RemoveTag(tag)` - completed
  - `Actor:HasTag(tag)` - completed
  - `Actor:GetTags()` - completed
- Serialize tags with scenes. - completed
- Display the Actor tag container as a built-in `Actor Tags` section. - completed
- Avoid a global project tag registry for now because this engine does not have project separation yet.
- If tag typo risk becomes a problem, add a simple optional text file later, such as `Settings/GameplayTags.ini`.

Reasoning:

- Auto-attaching a `TagComponent` to every actor increases component noise and scene serialization churn.
- Actor-level tags match the actual use case: querying gameplay objects by semantic label.
- A future `UTagComponent` can still be added later if tags need component behavior.
