---@meta

---@alias InputKey string|integer
---@alias MouseButton string|integer

---@class Vector
---@field X number
---@field Y number
---@field Z number
---@field x number
---@field y number
---@field z number
---@param x? number
---@param y? number
---@param z? number
---@type fun(x?: number, y?: number, z?: number): Vector
Vector = {}
---@return number
function Vector:Size() end
---@return number
function Vector:SizeSquared() end
---@return number
function Vector:Size2D() end
---@return number
function Vector:SizeSquared2D() end
---@return number
function Vector:Length() end
---@return Vector
function Vector:GetSafeNormal() end
---@return Vector
function Vector:Normalized() end
---@param other Vector
---@return number
function Vector:Dot(other) end
---@param other Vector
---@return Vector
function Vector:Cross(other) end

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
---@field Active boolean
---@field AutoActivate boolean
---@field TickEnabled boolean
---@field EditorOnly boolean
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
function ActorComponent:IsAutoActivate() end
---@param active boolean
function ActorComponent:SetAutoActivate(active) end
---@return boolean
function ActorComponent:IsComponentTickEnabled() end
---@param enabled boolean
function ActorComponent:SetComponentTickEnabled(enabled) end
---@return boolean
function ActorComponent:IsEditorOnly() end
---@param tag string
function ActorComponent:AddTag(tag) end
---@param tag string
function ActorComponent:RemoveTag(tag) end
---@param tag string
---@return boolean
function ActorComponent:HasTag(tag) end
function ActorComponent:ClearTags() end
---@return string[]
function ActorComponent:GetTags() end

---@class SceneComponent: ActorComponent
---@field Location Vector
---@field Forward Vector
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
---@param velocity Vector
---@return boolean
function MovementComponent:IsExceedingMaxSpeed(velocity) end
function MovementComponent:StopMovementImmediately() end
---@param direction Vector
---@return Vector
function MovementComponent:ConstrainDirectionToPlane(direction) end
---@param location Vector
---@return Vector
function MovementComponent:ConstrainLocationToPlane(location) end

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
---@field FacingTargetDirection boolean
local InterpToMovementComponent = {}
---@param point Vector
function InterpToMovementComponent:AddControlPoint(point) end
---@param index integer
function InterpToMovementComponent:RemoveControlPoint(index) end
---@param index integer
---@param point Vector
function InterpToMovementComponent:SetControlPoint(index, point) end
function InterpToMovementComponent:Initiate() end
function InterpToMovementComponent:Reset() end
function InterpToMovementComponent:ResetAndHalt() end

---@class PursuitMovementComponent: MovementComponent
---@field FacingTargetDirection boolean
local PursuitMovementComponent = {}
function PursuitMovementComponent:ClearTarget() end
---@return boolean
function PursuitMovementComponent:IsInPursuit() end

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
---@field Name string
---@field Location Vector
---@field Rotation Vector
---@field Scale Vector
---@field RootComponent SceneComponent
---@field Active boolean
---@field Visible boolean
---@field TickInEditor boolean
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
---@param name string
---@return ActorComponent|nil
function Actor:FindComponentByName(name) end
---@param name string
---@return ActorComponent|nil
function Actor:GetComponentByName(name) end
---@param name string
---@return ActorComponent[]
function Actor:FindComponentsByName(name) end
---@param name string
---@return ActorComponent[]
function Actor:GetComponentsByName(name) end
---@param tag string
---@return ActorComponent|nil
function Actor:FindComponentByTag(tag) end
---@param tag string
---@return ActorComponent|nil
function Actor:GetComponentByTag(tag) end
---@param tag string
---@return ActorComponent[]
function Actor:FindComponentsByTag(tag) end
---@param tag string
---@return ActorComponent[]
function Actor:GetComponentsByTag(tag) end
---@param tags string[]
---@return ActorComponent[]
function Actor:FindComponentsByTags(tags) end
---@param tags string[]
---@return ActorComponent[]
function Actor:GetComponentsByTags(tags) end
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
function Actor:ClearTags() end
---@return Actor
function Actor:Duplicate() end
---@return Vector
function Actor:GetActorForwardVector() end
---@return Vector
function Actor:GetActorRightVector() end
---@return Vector
function Actor:GetActorUpVector() end
---@param delta Vector
function Actor:Add_Actor_World_Offset(delta) end
function Actor:MarkPendingKill() end

---@class CameraComponent: SceneComponent
---@field FOV number
---@field OrthoWidth number
---@field Orthographic boolean
---@field NearPlane number
---@field FarPlane number
---@field Forward Vector
---@field Right Vector
---@field Up Vector
local CameraComponent = {}
---@param target Vector
function CameraComponent:look_at(target) end
---@param amount number
function CameraComponent:move_forward(amount) end
---@param amount number
function CameraComponent:move_right(amount) end
---@param amount number
function CameraComponent:move_up(amount) end
---@param amount number
function CameraComponent:add_yaw_input(amount) end
---@param amount number
function CameraComponent:add_pitch_input(amount) end

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
---@param visible boolean
function PlayerController:SetCursorVisible(visible) end
---@return boolean
function PlayerController:IsCursorVisible() end
---@param locked boolean
function PlayerController:SetCursorLocked(locked) end
---@return boolean
function PlayerController:IsCursorLocked() end
---@param captured boolean
function PlayerController:SetMouseCapture(captured) end
function PlayerController:ReleaseMouseCapture() end
---@return boolean
function PlayerController:IsMouseCaptured() end
function PlayerController:SetInputModeGameOnly() end
function PlayerController:SetInputModeUIOnly() end
function PlayerController:SetInputModeGameAndUI() end
---@param intensity number 1.0 is a normal hit shake.
---@param duration number seconds
function PlayerController:PlayCameraShake(intensity, duration) end
---@param location_amplitude number world-unit camera offset
---@param rotation_amplitude_degrees number camera rotation offset in degrees
---@param frequency number shake cycles per second
---@param duration number seconds
function PlayerController:PlayCameraShakeDetailed(location_amplitude, rotation_amplitude_degrees, frequency, duration) end
---@param target_fov_degrees number final vertical FOV in degrees
---@param duration number seconds
function PlayerController:LerpCameraFOVDegrees(target_fov_degrees, duration) end
---@param duration number seconds
function PlayerController:ResetCameraFOV(duration) end
function PlayerController:StopCameraEffects() end
---@return Actor|nil
function PlayerController:GetPossessedActor() end
---@return Actor|nil
function PlayerController:GetViewTargetActor() end
---@return CameraComponent|nil
function PlayerController:GetViewTargetCamera() end

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

---@param locked boolean
function EngineAPIInput.SetCursorLocked(locked) end

---@return boolean
function EngineAPIInput.IsCursorLocked() end

---@return boolean
function EngineAPIInput.IsCursorVisible() end

---@param captured boolean
function EngineAPIInput.SetMouseCapture(captured) end

function EngineAPIInput.ReleaseMouseCapture() end

---@return boolean
function EngineAPIInput.IsMouseCaptured() end

---@class EngineAPIWorld
local EngineAPIWorld = {}

---@param name string
---@return Actor|nil
function EngineAPIWorld.FindActorByName(name) end

---@param name string
---@return Actor[]
function EngineAPIWorld.FindActorsByName(name) end

---@param tag string
---@return Actor|nil
function EngineAPIWorld.FindActorByTag(tag) end

---@param tag string
---@return Actor[]
function EngineAPIWorld.FindActorsByTag(tag) end

---@param tags string[]
---@return Actor[]
function EngineAPIWorld.FindActorsByTags(tags) end

---@param typeName string
---@return Actor[]
function EngineAPIWorld.FindActorsByType(typeName) end

---@return Actor[]
function EngineAPIWorld.GetAllActors() end

---@return integer
function EngineAPIWorld.GetActorCount() end

---@param actor Actor
---@return boolean
function EngineAPIWorld.IsValidActor(actor) end

---@return PlayerController|nil
function EngineAPIWorld.GetPlayerController() end

---@return Actor|nil
function EngineAPIWorld.GetPossessedActor() end

---@return Actor|nil
function EngineAPIWorld.GetViewTargetActor() end

---@return CameraComponent|nil
function EngineAPIWorld.GetViewTargetCamera() end

---@param location Vector
function EngineAPIWorld.SetViewTargetCameraLocation(location) end

---@param delta Vector
function EngineAPIWorld.AddViewTargetCameraLocation(delta) end

---@return Vector
function EngineAPIWorld.GetViewTargetCameraLocation() end

---@param typeName string
---@return Actor|nil
function EngineAPIWorld.SpawnActor(typeName) end

---@param relativePath string
---@return Actor|nil
function EngineAPIWorld.SpawnActorFromPrefab(relativePath) end

---@param actor Actor
function EngineAPIWorld.DestroyActor(actor) end

---@param scale number
function EngineAPIWorld.SetTimeScale(scale) end

---@return number
function EngineAPIWorld.GetTimeScale() end

---@return number
function EngineAPIWorld.GetDeltaTime() end

---@return number
function EngineAPIWorld.GetUnscaledDeltaTime() end

---@return number
function EngineAPIWorld.GetGameTime() end

---@return number
function EngineAPIWorld.GetRealTime() end

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
---@param path string
---@return boolean
function EngineAPIUI.LoadDocument(screenId, path) end

---@param screenId string
---@return boolean
function EngineAPIUI.UnloadDocument(screenId) end

---@param screenId string
---@return boolean
function EngineAPIUI.ReloadDocument(screenId) end

---@param screenId string
---@return boolean
function EngineAPIUI.ShowDocument(screenId) end

---@param screenId string
---@return boolean
function EngineAPIUI.HideDocument(screenId) end

---@param elementId string
---@return boolean
function EngineAPIUI.HasElement(elementId) end

---@param elementId string
---@param text string
---@return boolean
function EngineAPIUI.SetElementText(elementId, text) end

---@param elementId string
---@return string
function EngineAPIUI.GetElementText(elementId) end

---@param elementId string
---@return string
function EngineAPIUI.GetElementValue(elementId) end

---@param elementId string
---@param value string
---@return boolean
function EngineAPIUI.SetElementValue(elementId, value) end

---@param elementId string
---@param visible boolean
---@return boolean
function EngineAPIUI.SetElementVisible(elementId, visible) end

---@param elementId string
---@param enabled boolean
---@return boolean
function EngineAPIUI.SetElementEnabled(elementId, enabled) end

---@param elementId string
---@param className string
---@param enabled boolean
---@return boolean
function EngineAPIUI.SetElementClass(elementId, className, enabled) end

---@param elementId string
---@param className string
---@return boolean
function EngineAPIUI.HasElementClass(elementId, className) end

---@param elementId string
---@return string
function EngineAPIUI.GetElementClassNames(elementId) end

---@param elementId string
---@param classNames string
---@return boolean
function EngineAPIUI.SetElementClassNames(elementId, classNames) end

---@param elementId string
---@param name string
---@return boolean
function EngineAPIUI.HasElementAttribute(elementId, name) end

---@param elementId string
---@param name string
---@return string
function EngineAPIUI.GetElementAttribute(elementId, name) end

---@param elementId string
---@param name string
---@param value string
---@return boolean
function EngineAPIUI.SetElementAttribute(elementId, name, value) end

---@param elementId string
---@param name string
---@return boolean
function EngineAPIUI.RemoveElementAttribute(elementId, name) end

---@param elementId string
---@param name string
---@return string
function EngineAPIUI.GetElementStyle(elementId, name) end

---@param elementId string
---@param name string
---@param value string
---@return boolean
function EngineAPIUI.SetElementStyle(elementId, name, value) end

---@param elementId string
---@param name string
---@return boolean
function EngineAPIUI.RemoveElementStyle(elementId, name) end

---@param elementId string
---@param focusVisible? boolean
---@return boolean
function EngineAPIUI.FocusElement(elementId, focusVisible) end

---@param elementId string
---@return boolean
function EngineAPIUI.BlurElement(elementId) end

---@param elementId string
---@return boolean
function EngineAPIUI.ClickElement(elementId) end

---@param elementId string
---@param text string
---@return boolean
function EngineAPIUI.SetText(elementId, text) end

---@param elementId string
---@return string
function EngineAPIUI.GetValue(elementId) end

---@param elementId string
---@param value string
---@return boolean
function EngineAPIUI.SetValue(elementId, value) end

---@param elementId string
---@param imagePath string
---@return boolean
function EngineAPIUI.SetImage(elementId, imagePath) end

---@param elementId string
---@param value number
---@return boolean
function EngineAPIUI.SetProgress(elementId, value) end

---@param elementId string
---@param visible boolean
---@return boolean
function EngineAPIUI.SetVisible(elementId, visible) end

---@param elementId string
---@param enabled boolean
---@return boolean
function EngineAPIUI.SetEnabled(elementId, enabled) end

---@param elementId string
---@param eventName string
---@return boolean
function EngineAPIUI.SetActionEvent(elementId, eventName) end

---@param elementId string
---@return boolean
function EngineAPIUI.RemoveElement(elementId) end

---@param elementId string
---@param zOrder integer
---@return boolean
function EngineAPIUI.SetZOrder(elementId, zOrder) end

---@param elementId string
---@param r number
---@param g number
---@param b number
---@param a number
---@return boolean
function EngineAPIUI.SetTint(elementId, r, g, b, a) end

---@param elementId string
---@param r number
---@param g number
---@param b number
---@param a number
---@return boolean
function EngineAPIUI.SetBackgroundColor(elementId, r, g, b, a) end

---@param elementId string
---@param r number
---@param g number
---@param b number
---@param a number
---@return boolean
function EngineAPIUI.SetTextColor(elementId, r, g, b, a) end

---@param elementId string
---@param alpha number
---@return boolean
function EngineAPIUI.SetAlpha(elementId, alpha) end

---@param elementId string
---@param rounding number
---@return boolean
function EngineAPIUI.SetRounding(elementId, rounding) end

---@param elementId string
---@param fontScale number
---@return boolean
function EngineAPIUI.SetFontScale(elementId, fontScale) end

---@param elementId string
---@param x number
---@param y number
---@param w number
---@param h number
---@return boolean
function EngineAPIUI.SetElementTransform(elementId, x, y, w, h) end

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

---@class EngineAPIJson
local EngineAPIJson = {}

---@param value any
---@return string
function EngineAPIJson.Encode(value) end

---@param text string
---@return any
function EngineAPIJson.Decode(text) end

---@class EngineAPIScene
local EngineAPIScene = {}

---@param scenePath string
---@return boolean
function EngineAPIScene.Open(scenePath) end

---@return boolean
function EngineAPIScene.Reload() end

---@return boolean
function EngineAPIScene.IsOpenPending() end

---@return string
function EngineAPIScene.GetCurrentPath() end

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
---@field Input EngineAPIInput
---@field World EngineAPIWorld
---@field Audio EngineAPIAudio
---@field UI EngineAPIUI
---@field Save EngineAPISave
---@field Json EngineAPIJson
---@field Scene EngineAPIScene
---@field Debug EngineAPIDebug
---@field Asset EngineAPIAsset
---@field Random EngineAPIRandom
---@field Application EngineAPIApplication
---@field Effect EngineAPIEffect
local EngineAPI = {}

---@return PlayerController|nil
function EngineAPI.GetPlayerController() end

---@return Actor|nil
function EngineAPI.GetPossessedActor() end

---@return Actor|nil
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

---@param seconds number
---@return table
function WaitForSeconds(seconds) end

---@param frames integer
---@return table
function WaitForFrames(frames) end
