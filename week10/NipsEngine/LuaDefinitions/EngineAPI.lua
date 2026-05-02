---@meta

---@alias InputKey string|integer
---@alias MouseButton string|integer
---@alias UIEasing "'Linear'"|"'EaseIn'"|"'EaseOut'"|"'EaseInOut'"|"'SmoothStep'"|string

---@class Object
local Object = {}
---@return integer
function Object:GetUUID() end
---@return string
function Object:GetName() end
---@return string
function Object:GetType() end
---@param typeName string
---@return boolean
function Object:IsA(typeName) end

---@class ActorComponent: Object
---@field TypeName string
local ActorComponent = {}
---@return Actor
function ActorComponent:GetOwner() end
---@return Actor
function ActorComponent:GetActor() end
---@return boolean
function ActorComponent:IsActive() end
---@param active boolean
function ActorComponent:SetActive(active) end
---@return boolean
function ActorComponent:IsComponentTickEnabled() end
---@param enabled boolean
function ActorComponent:SetComponentTickEnabled(enabled) end

---@class SceneComponent: ActorComponent
local SceneComponent = {}
---@return SceneComponent
function SceneComponent:GetParent() end
---@param parent SceneComponent
function SceneComponent:AttachToComponent(parent) end
---@return Vector
function SceneComponent:GetRelativeLocation() end
---@param location Vector
function SceneComponent:SetRelativeLocation(location) end

---@class MovementComponent: ActorComponent
---@field Velocity Vector
---@field PendingInputVector Vector
---@field PlaneConstraintNormal Vector
local MovementComponent = {}
---@param component SceneComponent
function MovementComponent:SetUpdatedComponent(component) end
---@return SceneComponent|nil
function MovementComponent:GetUpdatedComponent() end
---@param direction Vector
---@param scale? number
function MovementComponent:AddInputVector(direction, scale) end
---@return Vector
function MovementComponent:ConsumeInputVector() end
---@return number
function MovementComponent:GetMaxSpeed() end
function MovementComponent:StopMovementImmediately() end

---@class ProjectileMovementComponent: MovementComponent
---@field InitialSpeed number
---@field MaxSpeed number
---@field GravityScale number
---@field RotationFollowsVelocity boolean
local ProjectileMovementComponent = {}

---@class RotatingMovementComponent: MovementComponent
---@field RotationRate Vector
---@field PivotTranslation Vector
---@field RotationInLocalSpace boolean
local RotatingMovementComponent = {}

---@class InterpToMovementComponent: MovementComponent
---@field Duration number
---@field AutoActivate boolean
local InterpToMovementComponent = {}
---@param point Vector
function InterpToMovementComponent:AddControlPoint(point) end
---@param index integer
function InterpToMovementComponent:RemoveControlPoint(index) end
function InterpToMovementComponent:Initiate() end
function InterpToMovementComponent:Reset() end
function InterpToMovementComponent:ResetAndHalt() end

---@class ScriptComponent: ActorComponent
local ScriptComponent = {}
---@param scriptName string
function ScriptComponent:SetScriptName(scriptName) end
---@return string
function ScriptComponent:GetScriptName() end
---@return boolean
function ScriptComponent:LoadScript() end
---@return boolean
function ScriptComponent:HotReloadScript() end
function ScriptComponent:ClearScript() end

---@class Actor: Object
---@field TypeName string
local Actor = {}
---@return SceneComponent
function Actor:GetRootComponent() end
---@return ActorComponent[]
function Actor:GetComponents() end
---@param typeName string
---@return ActorComponent|nil
function Actor:GetComponent(typeName) end
---@param typeName string
---@return ActorComponent[]
function Actor:GetComponentsByType(typeName) end
---@param typeName string
---@param attachToRoot? boolean
---@return ActorComponent|nil
function Actor:AddComponent(typeName, attachToRoot) end
---@param component ActorComponent
---@return boolean
function Actor:RemoveComponent(component) end
---@param tag string
function Actor:AddTag(tag) end
---@param tag string
function Actor:RemoveTag(tag) end
---@param tag string
---@return boolean
function Actor:HasTag(tag) end
---@return string[]
function Actor:GetTags() end

---@class PlayerController: Actor
---@field PossessedActor Actor|nil
---@field ViewTargetActor Actor|nil
---@field ViewTargetCamera CameraComponent|nil
local PlayerController = {}
---@param actor Actor
function PlayerController:Possess(actor) end
function PlayerController:UnPossess() end
---@param actor Actor
function PlayerController:SetViewTarget(actor) end
---@param camera CameraComponent
function PlayerController:SetViewTargetCamera(camera) end
---@return Actor|nil
function PlayerController:GetPossessedActor() end
---@return Actor|nil
function PlayerController:GetViewTargetActor() end
---@return CameraComponent|nil
function PlayerController:GetViewTargetCamera() end

---@class EngineAPITime
local EngineAPITime = {}

---@param scale number
function EngineAPITime.SetTimeScale(scale) end

---@return number
function EngineAPITime.GetTimeScale() end

---@return number
function EngineAPITime.GetDeltaTime() end

---@return number
function EngineAPITime.GetUnscaledDeltaTime() end

---@return number
function EngineAPITime.GetGameTime() end

---@return number
function EngineAPITime.GetRealTime() end

---@class EngineAPIInput
local EngineAPIInput = {}

---@param key InputKey
---@return boolean
function EngineAPIInput.IsKeyDown(key) end

---@param key InputKey
---@return boolean
function EngineAPIInput.IsKeyPressed(key) end

---@param key InputKey
---@return boolean
function EngineAPIInput.IsKeyReleased(key) end

---@param button MouseButton
---@return boolean
function EngineAPIInput.IsMouseDown(button) end

---@param button MouseButton
---@return boolean
function EngineAPIInput.IsMousePressed(button) end

---@param button MouseButton
---@return boolean
function EngineAPIInput.IsMouseReleased(button) end

---@return Vector
function EngineAPIInput.GetMousePosition() end

---@return Vector
function EngineAPIInput.GetMouseDelta() end

---@return integer
function EngineAPIInput.GetScrollDelta() end

---@return number
function EngineAPIInput.GetScrollNotches() end

---@return boolean
function EngineAPIInput.IsAnyMouseButtonDown() end

---@param mode "'GameOnly'"|"'UIOnly'"|"'GameAndUI'"|string
function EngineAPIInput.SetInputMode(mode) end

function EngineAPIInput.SetInputModeGameOnly() end
function EngineAPIInput.SetInputModeUIOnly() end
function EngineAPIInput.SetInputModeGameAndUI() end

---@param visible boolean
function EngineAPIInput.SetCursorVisible(visible) end

---@class EngineAPIWorld
local EngineAPIWorld = {}

---@param name string
---@return AActor|nil
function EngineAPIWorld.FindActorByName(name) end

---@param name string
---@return AActor[]
function EngineAPIWorld.FindActorsByName(name) end

---@param tag string
---@return AActor|nil
function EngineAPIWorld.FindActorByTag(tag) end

---@param tag string
---@return AActor[]
function EngineAPIWorld.FindActorsByTag(tag) end

---@param tags string[]
---@return AActor[]
function EngineAPIWorld.FindActorsByTags(tags) end

---@param typeName string
---@return AActor[]
function EngineAPIWorld.FindActorsByType(typeName) end

---@return AActor[]
function EngineAPIWorld.GetAllActors() end

---@return integer
function EngineAPIWorld.GetActorCount() end

---@param actor AActor
---@return boolean
function EngineAPIWorld.IsValidActor(actor) end

---@return PlayerController|nil
function EngineAPIWorld.GetPlayerController() end

---@return AActor|nil
function EngineAPIWorld.GetPossessedActor() end

---@return AActor|nil
function EngineAPIWorld.GetViewTargetActor() end

---@return CameraComponent|nil
function EngineAPIWorld.GetViewTargetCamera() end

---@param typeName string
---@return AActor|nil
function EngineAPIWorld.SpawnActor(typeName) end

---@param relativePath string
---@return AActor|nil
function EngineAPIWorld.SpawnActorFromPrefab(relativePath) end

---@param actor AActor
function EngineAPIWorld.DestroyActor(actor) end

---@class EngineAPIAudio
local EngineAPIAudio = {}

---@param pathOrKey string
---@param fadeIn? number
function EngineAPIAudio.PlayBGM(pathOrKey, fadeIn) end

---@param fadeOut? number
function EngineAPIAudio.StopBGM(fadeOut) end

---@param pathOrKey string
---@param volumeScale? number
---@return integer handle
function EngineAPIAudio.PlaySFX(pathOrKey, volumeScale) end

---@param pathOrKey string
---@param position Vector
---@param volumeScale? number
---@return integer handle
function EngineAPIAudio.PlaySFX3D(pathOrKey, position, volumeScale) end

---@param volume number
function EngineAPIAudio.SetMasterVolume(volume) end

---@param volume number
function EngineAPIAudio.SetBGMVolume(volume) end

---@param volume number
function EngineAPIAudio.SetSFXVolume(volume) end

---@return number
function EngineAPIAudio.GetMasterVolume() end

---@return number
function EngineAPIAudio.GetBGMVolume() end

---@return number
function EngineAPIAudio.GetSFXVolume() end

---@param position Vector
---@param forward Vector
---@param up Vector
function EngineAPIAudio.SetListener(position, forward, up) end

---@param pathOrKey string
---@param maxConcurrent integer
---@param cooldownSeconds number
function EngineAPIAudio.SetPlaybackPolicy(pathOrKey, maxConcurrent, cooldownSeconds) end

---@param pathOrKey string
function EngineAPIAudio.ClearPlaybackPolicy(pathOrKey) end

---@param handle integer
---@param fadeOut? number
function EngineAPIAudio.StopSound(handle, fadeOut) end

---@param handle integer
---@return boolean
function EngineAPIAudio.IsSoundPlaying(handle) end

---@param handle integer
---@param position Vector
function EngineAPIAudio.SetSoundPosition(handle, position) end

---@param key string
---@param path string
function EngineAPIAudio.RegisterSound(key, path) end

---@param keyOrPath string
---@return string
function EngineAPIAudio.ResolveSoundPath(keyOrPath) end

function EngineAPIAudio.ReloadSoundRegistry() end
function EngineAPIAudio.StopAll() end

---@class EngineAPIUI
local EngineAPIUI = {}

---@param screenId string
---@param canvasId? string
---@return boolean
function EngineAPIUI.ShowScreen(screenId, canvasId) end

---@param screenId string
---@param widgetId string
---@param x number
---@param y number
---@param w number
---@param h number
---@param parentId? string
---@return boolean
function EngineAPIUI.CreatePanel(screenId, widgetId, x, y, w, h, parentId) end

---@param screenId string
---@param widgetId string
---@param text string
---@param x number
---@param y number
---@param w number
---@param h number
---@param parentId? string
---@return boolean
function EngineAPIUI.CreateText(screenId, widgetId, text, x, y, w, h, parentId) end

---@param screenId string
---@param widgetId string
---@param text string
---@param eventName string
---@param x number
---@param y number
---@param w number
---@param h number
---@param parentId? string
---@return boolean
function EngineAPIUI.CreateButton(screenId, widgetId, text, eventName, x, y, w, h, parentId) end

---@param screenId string
---@param widgetId string
---@param imagePath string
---@param x number
---@param y number
---@param w number
---@param h number
---@param parentId? string
---@return boolean
function EngineAPIUI.CreateImage(screenId, widgetId, imagePath, x, y, w, h, parentId) end

---@param screenId string
---@param widgetId string
---@param value number
---@param x number
---@param y number
---@param w number
---@param h number
---@param parentId? string
---@return boolean
function EngineAPIUI.CreateProgressBar(screenId, widgetId, value, x, y, w, h, parentId) end

---@param widgetId string
---@return boolean
function EngineAPIUI.RemoveWidget(widgetId) end

---@param widgetId string
---@param text string
---@return boolean
function EngineAPIUI.SetText(widgetId, text) end

---@param widgetId string
---@param imagePath string
---@return boolean
function EngineAPIUI.SetImage(widgetId, imagePath) end

---@param widgetId string
---@param value number
---@return boolean
function EngineAPIUI.SetProgress(widgetId, value) end

---@param widgetId string
---@param visible boolean
---@return boolean
function EngineAPIUI.SetVisible(widgetId, visible) end

---@param widgetId string
---@param enabled boolean
---@return boolean
function EngineAPIUI.SetEnabled(widgetId, enabled) end

---@param widgetId string
---@param eventName string
---@return boolean
function EngineAPIUI.SetActionEvent(widgetId, eventName) end

---@param widgetId string
---@param zOrder integer
---@return boolean
function EngineAPIUI.SetZOrder(widgetId, zOrder) end

---@param widgetId string
---@param r number
---@param g number
---@param b number
---@param a number
---@return boolean
function EngineAPIUI.SetTint(widgetId, r, g, b, a) end

---@param widgetId string
---@param r number
---@param g number
---@param b number
---@param a number
---@return boolean
function EngineAPIUI.SetBackgroundColor(widgetId, r, g, b, a) end

---@param widgetId string
---@param r number
---@param g number
---@param b number
---@param a number
---@return boolean
function EngineAPIUI.SetTextColor(widgetId, r, g, b, a) end

---@param widgetId string
---@param alpha number
---@return boolean
function EngineAPIUI.SetAlpha(widgetId, alpha) end

---@param widgetId string
---@param rounding number
---@return boolean
function EngineAPIUI.SetRounding(widgetId, rounding) end

---@param widgetId string
---@param fontScale number
---@return boolean
function EngineAPIUI.SetFontScale(widgetId, fontScale) end

---@param widgetId string
---@param x number
---@param y number
---@param w number
---@param h number
---@return boolean
function EngineAPIUI.SetWidgetTransform(widgetId, x, y, w, h) end

---@param widgetId string
---@param minX number
---@param minY number
---@param maxX number
---@param maxY number
---@param pivotX number
---@param pivotY number
---@return boolean
function EngineAPIUI.SetWidgetAnchors(widgetId, minX, minY, maxX, maxY, pivotX, pivotY) end

---@param widgetId string
---@param uMinX number
---@param uMinY number
---@param uMaxX number
---@param uMaxY number
---@param drawMode "'Simple'"|"'NineSlice'"|string
---@param left? number
---@param top? number
---@param right? number
---@param bottom? number
---@return boolean
function EngineAPIUI.SetImageOptions(widgetId, uMinX, uMinY, uMaxX, uMaxY, drawMode, left, top, right, bottom) end

---@param widgetId string
---@param normal string
---@param hover string
---@param pressed string
---@param disabled string
---@return boolean
function EngineAPIUI.SetButtonImages(widgetId, normal, hover, pressed, disabled) end

---@param widgetId string
---@param columns integer
---@param rows integer
---@param frameIndex integer
---@return boolean
function EngineAPIUI.SetSpriteFrame(widgetId, columns, rows, frameIndex) end

---@param widgetId string
---@param particleNameOrPath string
---@param frameIndex integer
---@return boolean
function EngineAPIUI.SetSpriteFrameByMeta(widgetId, particleNameOrPath, frameIndex) end

---@param widgetId string
---@param background string
---@param fill string
---@param frame string
---@param direction? "'LeftToRight'"|"'RightToLeft'"|"'BottomToTop'"|"'TopToBottom'"|string
---@return boolean
function EngineAPIUI.SetProgressImages(widgetId, background, fill, frame, direction) end

---@param widgetId string
---@param x number
---@param y number
---@param w number
---@param h number
---@param duration number
---@param loop? boolean
---@param pingPong? boolean
---@param easing? UIEasing
---@return boolean
function EngineAPIUI.AnimateTransform(widgetId, x, y, w, h, duration, loop, pingPong, easing) end

---@param widgetId string
---@param x0 number
---@param y0 number
---@param x1 number
---@param y1 number
---@param duration number
---@param loop? boolean
---@param pingPong? boolean
---@param easing? UIEasing
---@return boolean
function EngineAPIUI.AnimatePatrol(widgetId, x0, y0, x1, y1, duration, loop, pingPong, easing) end

---@param widgetId string
---@param x0 number
---@param y0 number
---@param x1 number
---@param y1 number
---@param speed number
---@param loop? boolean
---@param pingPong? boolean
---@param easing? UIEasing
---@return boolean
function EngineAPIUI.AnimatePatrolBySpeed(widgetId, x0, y0, x1, y1, speed, loop, pingPong, easing) end

---@param widgetId string
---@return boolean
function EngineAPIUI.StopAnimation(widgetId) end

---@return string[]
function EngineAPIUI.PollActionEvents() end

---@class EngineAPISave
local EngineAPISave = {}

---@param relativePath string
---@param text string
---@return boolean
function EngineAPISave.WriteText(relativePath, text) end

---@param relativePath string
---@return string|nil
function EngineAPISave.ReadText(relativePath) end

---@param relativePath string
---@return boolean
function EngineAPISave.Exists(relativePath) end

---@param relativePath string
---@return boolean
function EngineAPISave.Delete(relativePath) end

---@class EngineAPIDebug
local EngineAPIDebug = {}

---@param message string
function EngineAPIDebug.Log(message) end

---@param message string
function EngineAPIDebug.Warn(message) end

---@param message string
function EngineAPIDebug.Error(message) end

---@class EngineAPIAsset
local EngineAPIAsset = {}

---@param path string
---@return string|nil
function EngineAPIAsset.NormalizePath(path) end

---@param path string
---@return boolean
function EngineAPIAsset.Exists(path) end

---@return string[]
function EngineAPIAsset.GetTexturePaths() end

---@return string[]
function EngineAPIAsset.GetStaticMeshPaths() end

---@return string[]
function EngineAPIAsset.GetMaterialPaths() end

---@return string[]
function EngineAPIAsset.GetScenePaths() end

---@return string[]
function EngineAPIAsset.GetSoundPaths() end

---@class EngineAPIRandom
local EngineAPIRandom = {}

---@param seed integer
function EngineAPIRandom.SetSeed(seed) end

---@return number
function EngineAPIRandom.RandomFloat01() end

---@param min number
---@param max number
---@return number
function EngineAPIRandom.RandomFloat(min, max) end

---@param min integer
---@param max integer
---@return integer
function EngineAPIRandom.RandomInt(min, max) end

---@param probability number
---@return boolean
function EngineAPIRandom.RandomBool(probability) end

---@class EngineAPIApplication
local EngineAPIApplication = {}

---@return boolean
function EngineAPIApplication.QuitGame() end

---@class EngineAPIEffect
local EngineAPIEffect = {}

---@class EngineAPI
---@field Time EngineAPITime
---@field Input EngineAPIInput
---@field World EngineAPIWorld
---@field Audio EngineAPIAudio
---@field UI EngineAPIUI
---@field Save EngineAPISave
---@field Debug EngineAPIDebug
---@field Asset EngineAPIAsset
---@field Random EngineAPIRandom
---@field Application EngineAPIApplication
---@field Effect EngineAPIEffect
local EngineAPI = {}

---@return PlayerController|nil
function EngineAPI.GetPlayerController() end

---@return AActor|nil
function EngineAPI.GetPossessedActor() end

---@return AActor|nil
function EngineAPI.GetViewTargetActor() end

---@return CameraComponent|nil
function EngineAPI.GetViewTargetCamera() end

---@class EngineGlobal
---@field API EngineAPI
Engine = {}

---@param message string
function Log(message) end

---@param message string
function LogWarning(message) end

---@param message string
function LogError(message) end
