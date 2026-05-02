# Engine API Reference

`Engine.API` is the official Lua-facing global engine service root.

Use `Engine.API` for global systems. Use Actor/Component bindings for per-object mutation.

## Boundary

Good:

```lua
local enemy = Engine.API.World.FindActorByName("Enemy_01")
enemy.Location = Vector(0, 0, 100)
enemy:AddTag("Enemy")
```

Avoid:

```lua
Engine.API.World.SetActorLocation(enemy, Vector(0, 0, 100))
```

## Recommended Lua Usage Pattern

Use `Engine.API` as the global service layer, then keep gameplay meaning in Lua-side modules.

Recommended split:

```lua
-- InputManager.lua
local Input = Engine.API.Input

local M = {}

function M.IsAttackPressed()
    return Input.IsMousePressed("LMB")
end

function M.GetMoveAxis()
    local x = 0
    local y = 0
    if Input.IsKeyDown("D") then x = x + 1 end
    if Input.IsKeyDown("A") then x = x - 1 end
    if Input.IsKeyDown("W") then y = y + 1 end
    if Input.IsKeyDown("S") then y = y - 1 end
    return Vector(x, y, 0)
end

return M
```

```lua
-- Pause UI open
Engine.API.Input.SetInputModeUIOnly()
Engine.API.UI.ShowScreen("Pause")
```

```lua
-- Pause UI close
Engine.API.Input.SetInputModeGameOnly()
Engine.API.UI.ShowScreen("HUD")
```

Rules of thumb:

- `Engine.API.World` finds/spawns/destroys actors.
- Actor/component bindings mutate individual objects.
- `Engine.API.Input` reads raw device state or changes runtime input mode.
- `Engine.API.UI` builds screens/widgets and returns button events through `PollActionEvents`.
- `Engine.API.Audio` owns global BGM/SFX playback. `SoundComponent` owns actor-attached sound.
- `Engine.API.Save` stores simple data under `Saves/`.
- Keep game-specific names such as `Attack`, `Score`, `Pause`, `Wave` in Lua modules, not in C++ API names.

## VSCode Autocomplete

LuaLS/VSCode autocomplete stubs live under:

```text
NipsEngine/LuaDefinitions
```

These files are editor-only type definitions. They are not runtime scripts.

The workspace `.vscode/settings.json` includes this folder through:

```json
"Lua.workspace.library": [
  "${workspaceFolder}/NipsEngine/LuaDefinitions"
]
```

When `Engine.API` expands, update both this reference and the matching stub files.

## Vector

`Vector(x, y, z)` creates an engine `FVector`. Arithmetic operators are bound, so Lua code can use `+`, `-`, unary `-`, `* scalar`, and `/ scalar`.

Common helpers:

```lua
local move = Vector(1, 1, 0)
local dir = move:Normalized()
local len = move:Size()
local flatDir = move:Normalized2D()

local dot = dir:Dot(Actor:GetActorForwardVector())
local cross = dir:Cross(Vector(0, 0, 1))
local distance = Actor.Location:DistanceTo(target.Location)
local pos = Vector.Lerp(Actor.Location, target.Location, 0.5)
local forward = Vector.Forward()
```

Available methods:

```lua
Vector:Size()
Vector:Length()
Vector:SizeSquared()
Vector:LengthSquared()
Vector:Size2D()
Vector:SizeSquared2D()
Vector:Normalize([tolerance]) -- mutates self, returns success
Vector:GetSafeNormal([tolerance])
Vector:Normalized([tolerance])
Vector:GetSafeNormal2D([tolerance])
Vector:Normalized2D([tolerance])
Vector:Dot(other)
Vector:DotProduct(other)
Vector:Cross(other)
Vector:CrossProduct(other)
Vector:DistanceTo(other)
Vector:DistanceSquaredTo(other)
Vector.Distance(a, b)
Vector.Dist(a, b)
Vector.DistSquared(a, b)
Vector.Lerp(a, b, t)
Vector.Zero()
Vector.One()
Vector.Forward()
Vector.Backward()
Vector.Right()
Vector.Left()
Vector.Up()
Vector.Down()
```

## Object, Actor, Component

These are object userdata methods, not `Engine.API` globals. A script attached to an actor receives `Actor`, `Owner`, and `Component` in its environment.

```lua
local script = Actor:AddComponent("ScriptComponent")
script:SetScriptName("EnemyMovement")
script:LoadScript()

local camera = Actor:GetComponent("CameraComponent")
local primitives = Actor:GetComponentsByType("PrimitiveComponent")
local owner = Component:GetActor()
```

Actor helpers:

```lua
Actor:GetRootComponent()
Actor:GetComponents()
Actor:GetComponent(typeName)
Actor:GetComponentsByType(typeName)
Actor:AddComponent(typeName[, attachToRoot])
Actor:RemoveComponent(component)
Actor:AddTag(tag)
Actor:RemoveTag(tag)
Actor:HasTag(tag)
Actor:GetTags()
```

Component helpers:

```lua
Component:GetOwner()
Component:GetActor()
Component:IsActive()
Component:SetActive(active)
Component:IsComponentTickEnabled()
Component:SetComponentTickEnabled(enabled)
```

Common component `typeName` values include `"ScriptComponent"`, `"CameraComponent"`, `"StaticMeshComponent"`, `"PrimitiveComponent"`, `"SoundComponent"`, `"DecalComponent"`, `"BillboardComponent"`, `"SubUVComponent"`, `"BoxComponent"`, `"SphereComponent"`, and `"CapsuleComponent"`.

Common patterns:

```lua
-- Find one tagged player and attach a script component if missing.
local player = Engine.API.World.FindActorByTag("Player")
if player then
    local script = player:GetComponent("ScriptComponent")
    if not script then
        script = player:AddComponent("ScriptComponent")
    end
    script:SetScriptName("PlayerRuntime")
    script:LoadScript()
end
```

```lua
-- Toggle every primitive-like component on an actor.
local actor = Engine.API.World.FindActorByName("Enemy_01")
if actor then
    for _, component in ipairs(actor:GetComponentsByType("PrimitiveComponent")) do
        component:SetActive(false)
    end
end
```

```lua
-- Spawn from prefab, then tag it for later manager-side lookup.
local enemy = Engine.API.World.SpawnActorFromPrefab("Enemy.prefab")
if enemy then
    enemy:AddTag("Enemy")
end
```

## Time

```lua
Engine.API.Time.SetTimeScale(scale)
Engine.API.Time.GetTimeScale()
Engine.API.Time.GetDeltaTime()
Engine.API.Time.GetUnscaledDeltaTime()
Engine.API.Time.GetGameTime()
Engine.API.Time.GetRealTime()
```

`GetDeltaTime` returns the scaled frame delta used by gameplay world ticking. `GetUnscaledDeltaTime` ignores `TimeScale`.

## Input

Key/button arguments accept friendly strings or numeric VK codes.

`IsKeyDown` means held. `IsKeyPressed` means just pressed. `IsKeyReleased` means just released.
When Runtime UI or Editor GUI captures mouse/keyboard input, gameplay Lua input queries are masked for that captured device class.

This is a low-level service. For game code, prefer a Lua-side input manager that converts raw input into semantic actions such as `Attack`, `Pause`, `SlowTime`, or `MoveForward`.

```lua
if Engine.API.Input.IsKeyDown("W") then
    -- held
end

if Engine.API.Input.IsKeyPressed("Escape") then
    -- pressed this frame
end

if Engine.API.Input.IsMousePressed("LeftMouse") then
    -- LMB pressed this frame
end

local mouse = Engine.API.Input.GetMousePosition()
local delta = Engine.API.Input.GetMouseDelta()
local wheel = Engine.API.Input.GetScrollNotches()
```

Functions:

```lua
Engine.API.Input.IsKeyDown(key)
Engine.API.Input.IsKeyPressed(key)
Engine.API.Input.IsKeyReleased(key)
Engine.API.Input.IsMouseDown(button)
Engine.API.Input.IsMousePressed(button)
Engine.API.Input.IsMouseReleased(button)
Engine.API.Input.GetMousePosition() -- Vector(x, y, 0)
Engine.API.Input.GetMouseDelta() -- Vector(dx, dy, 0)
Engine.API.Input.GetScrollDelta()
Engine.API.Input.GetScrollNotches()
Engine.API.Input.IsAnyMouseButtonDown()
Engine.API.Input.SetInputMode("GameOnly" | "UIOnly" | "GameAndUI")
Engine.API.Input.SetInputModeGameOnly()
Engine.API.Input.SetInputModeUIOnly()
Engine.API.Input.SetInputModeGameAndUI()
Engine.API.Input.SetCursorVisible(visible)
```

Common key names:

```lua
"W", "A", "S", "D"
"Escape", "Enter", "Space", "Tab"
"LeftShift", "RightShift", "LeftCtrl", "RightCtrl", "LeftAlt", "RightAlt"
"Left", "Right", "Up", "Down"
"F1" ... "F24"
"LeftMouse", "RightMouse", "MiddleMouse", "LMB", "RMB", "MMB"
```

## World

World is for search/factory/destruction only. Per-actor changes belong to Actor bindings.

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
Engine.API.World.GetPlayerController()
Engine.API.World.GetPossessedActor()
Engine.API.World.GetViewTargetActor()
Engine.API.World.GetViewTargetCamera()
Engine.API.World.SpawnActor(typeName)
Engine.API.World.SpawnActorFromPrefab(relativePath)
Engine.API.World.DestroyActor(actor)
```

`GetPlayerController` works in both GameClient and PIE by returning the engine's primary runtime controller when one exists. This is the preferred bridge when Lua needs the possessed player actor or current gameplay camera.

```lua
local controller = Engine.API.World.GetPlayerController()
local player = Engine.API.World.GetPossessedActor()
local camera = Engine.API.World.GetViewTargetCamera()

if controller and player and camera then
    local mouse = Engine.API.Input.GetMouseDelta()
    local rot = player.Rotation
    rot.Z = rot.Z + mouse.X * 0.003
    player.Rotation = rot
    camera:add_pitch_input(-mouse.Y * 0.003)
end
```

Prefab paths are constrained under `Asset/`. If only a file name is passed, the engine looks under `Asset/Prefab/`.

```lua
local enemy = Engine.API.World.SpawnActorFromPrefab("Enemy.prefab")
local blade = Engine.API.World.SpawnActorFromPrefab("Asset/Prefab/BladePickup.prefab")
```

Spawned prefabs do not preserve saved UUIDs, so the same prefab can be spawned multiple times safely. Actor names are made unique automatically when there is a collision.

Prefab instances are intentionally treated as normal scene actors after spawn. The scene saves the actor/component data directly; it does not keep a live link back to the `.prefab` file, and there is no apply/revert override model yet. In the editor, `.prefab` files can be spawned from the Content Browser by dragging them into a viewport or by using `Spawn Prefab at Origin`.

Actor/component bindings expose enough of the player-controller and movement surface for Lua gameplay:

```lua
local projectile = Engine.API.World.SpawnActor("ABullet")
local movement = projectile and projectile:GetComponent("ProjectileMovementComponent")
if movement then
    movement.Velocity = player:GetActorForwardVector() * 40.0
    movement.MaxSpeed = 100.0
end
```

`Actor:GetComponent(typeName)` and `Actor:AddComponent(typeName[, attachToRoot])` accept both native names such as `UProjectileMovementComponent` and Lua-style names such as `ProjectileMovementComponent`. The game-facing component set currently includes scene, primitive, shape/collider, mesh, camera, spring arm, sound, script, movement/projectile/rotating/interp/pursuit, decal, fog, light, billboard, SubUV, text render, fireball, and the common box/sphere/capsule collider components. Editor-only components should still be avoided in game Lua.

## Audio

```lua
Engine.API.Audio.PlayBGM(pathOrKey[, fadeIn])
Engine.API.Audio.StopBGM([fadeOut])
local handle = Engine.API.Audio.PlaySFX(pathOrKey[, volumeScale])
local handle = Engine.API.Audio.PlaySFX3D(pathOrKey, position[, volumeScale])
Engine.API.Audio.StopSound(handle[, fadeOut])
Engine.API.Audio.IsSoundPlaying(handle)
Engine.API.Audio.SetSoundPosition(handle, position)
Engine.API.Audio.SetMasterVolume(volume)
Engine.API.Audio.SetBGMVolume(volume)
Engine.API.Audio.SetSFXVolume(volume)
Engine.API.Audio.GetMasterVolume()
Engine.API.Audio.GetBGMVolume()
Engine.API.Audio.GetSFXVolume()
Engine.API.Audio.SetListener(position, forward, up)
Engine.API.Audio.SetPlaybackPolicy(pathOrKey, maxConcurrent, cooldownSeconds)
Engine.API.Audio.ClearPlaybackPolicy(pathOrKey)
Engine.API.Audio.RegisterSound(key, path)
Engine.API.Audio.ResolveSoundPath(keyOrPath)
Engine.API.Audio.ReloadSoundRegistry()
Engine.API.Audio.StopAll()
```

## UI

Runtime UI uses screen/widget IDs. UI action events are polled by Lua and routed into game management code.

```lua
Engine.API.UI.ShowScreen(screenId[, canvasId])
Engine.API.UI.CreatePanel(screenId, widgetId, x, y, w, h[, parentId])
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
Engine.API.UI.SetZOrder(widgetId, zOrder)
Engine.API.UI.SetTint(widgetId, r, g, b, a)
Engine.API.UI.SetBackgroundColor(widgetId, r, g, b, a)
Engine.API.UI.SetTextColor(widgetId, r, g, b, a)
Engine.API.UI.SetAlpha(widgetId, alpha)
Engine.API.UI.SetRounding(widgetId, rounding)
Engine.API.UI.SetFontScale(widgetId, fontScale)
Engine.API.UI.SetWidgetTransform(widgetId, x, y, w, h)
Engine.API.UI.SetWidgetAnchors(widgetId, minX, minY, maxX, maxY, pivotX, pivotY)
Engine.API.UI.SetImageOptions(widgetId, uMinX, uMinY, uMaxX, uMaxY, drawMode[, left, top, right, bottom])
Engine.API.UI.SetButtonImages(widgetId, normal, hover, pressed, disabled)
Engine.API.UI.SetSpriteFrame(widgetId, columns, rows, frameIndex)
Engine.API.UI.SetSpriteFrameByMeta(widgetId, particleNameOrPath, frameIndex)
Engine.API.UI.SetProgressImages(widgetId, background, fill, frame[, direction])
Engine.API.UI.AnimateTransform(widgetId, x, y, w, h, duration[, loop, pingPong[, easing]])
Engine.API.UI.AnimatePatrol(widgetId, x0, y0, x1, y1, duration[, loop, pingPong[, easing]])
Engine.API.UI.AnimatePatrolBySpeed(widgetId, x0, y0, x1, y1, speed[, loop, pingPong[, easing]])
Engine.API.UI.StopAnimation(widgetId)
Engine.API.UI.PollActionEvents()
```

`drawMode` accepts `"Simple"` or `"NineSlice"`. Progress `direction` accepts `"LeftToRight"`, `"RightToLeft"`, `"BottomToTop"`, or `"TopToBottom"`.
`SetSpriteFrame` cuts a texture atlas by grid. `SetSpriteFrameByMeta` uses registered Particle/SubUV atlas meta data and also applies the atlas image path to the widget.
Animation `easing` accepts `"Linear"`, `"EaseIn"`, `"EaseOut"`, `"EaseInOut"`, or `"SmoothStep"`. Omit it to keep the old linear interpolation.
Font face switching is not exposed yet; `SetFontScale` controls runtime UI text size using the current ImGui font.
Anchors and pivot are applied during layout. With the default scale-with-screen canvas, local position and size are scaled from the reference resolution while anchors stay relative to the current viewport.

```lua
-- Center a panel and keep it centered while the window resizes.
Engine.API.UI.SetWidgetAnchors("PausePanel", 0.5, 0.5, 0.5, 0.5, 0.5, 0.5)

-- Stretch a bar across the bottom. Size is an offset added after stretch.
Engine.API.UI.SetWidgetAnchors("BottomBar", 0.0, 1.0, 1.0, 1.0, 0.0, 1.0)
Engine.API.UI.SetWidgetTransform("BottomBar", 0, 0, 0, 64)
```

For menu/pause screens in GameClient, switch input mode while the UI is open:

```lua
Engine.API.Input.SetInputModeUIOnly()
Engine.API.UI.ShowScreen("Pause")

-- closing the menu
Engine.API.Input.SetInputModeGameOnly()
```

`UIOnly` releases the GameClient mouse lock and blocks PlayerController input. `GameOnly` restores normal first-person capture.

## Save

Save paths are constrained to `Saves/`. Invalid paths fail instead of escaping the save root.

```lua
Engine.API.Save.WriteText(relativePath, text)
Engine.API.Save.ReadText(relativePath) -- text or nil
Engine.API.Save.Exists(relativePath)
Engine.API.Save.Delete(relativePath)
```

## Asset

Asset paths are constrained to `Asset/`. This API returns normalized relative path strings only, not renderer/resource pointers.

```lua
Engine.API.Asset.NormalizePath(path) -- "Asset/..." or nil
Engine.API.Asset.Exists(path)
Engine.API.Asset.GetTexturePaths()
Engine.API.Asset.GetStaticMeshPaths()
Engine.API.Asset.GetMaterialPaths()
Engine.API.Asset.GetScenePaths()
Engine.API.Asset.GetSoundPaths()
```

Use this when Lua needs to discover data paths for UI images, sound keys/paths, material names, or quick data-driven menus. Actual object/component assignment still belongs to the target Actor/Component binding.

## Debug

```lua
Engine.API.Debug.Log(message)
Engine.API.Debug.Warn(message)
Engine.API.Debug.Error(message)
```

## Random

```lua
Engine.API.Random.SetSeed(seed)
Engine.API.Random.RandomFloat01()
Engine.API.Random.RandomFloat(min, max)
Engine.API.Random.RandomInt(min, max)
Engine.API.Random.RandomBool(probability) -- 0.0 to 1.0
```

`RandomInt` is inclusive on both ends.

## Application

```lua
Engine.API.Application.QuitGame()
```

`QuitGame` requests the GameClient window to close. In editor builds it is ignored to avoid accidentally closing the editor from PIE/Lua.

## Effect

Placeholder only for now.
