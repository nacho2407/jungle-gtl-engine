---@meta

---@class FName
local FName = {}

---@return string
function FName:ToString() end

---@class Vector
---@field X number
---@field Y number
---@field Z number
---@field x number
---@field y number
---@field z number
Vector = {}

---@param x? number
---@param y? number
---@param z? number
---@return Vector
function Vector(x, y, z) end

---@return number
function Vector:Size() end

---@return number
function Vector:Length() end

---@return number
function Vector:SizeSquared() end

---@return number
function Vector:LengthSquared() end

---@return number
function Vector:Size2D() end

---@return number
function Vector:SizeSquared2D() end

---@param tolerance? number
---@return boolean
function Vector:Normalize(tolerance) end

---@param tolerance? number
---@return Vector
function Vector:GetSafeNormal(tolerance) end

---@param tolerance? number
---@return Vector
function Vector:Normalized(tolerance) end

---@param tolerance? number
---@return Vector
function Vector:GetSafeNormal2D(tolerance) end

---@param tolerance? number
---@return Vector
function Vector:Normalized2D(tolerance) end

---@param other Vector
---@return number
function Vector:Dot(other) end

---@param other Vector
---@return number
function Vector:DotProduct(other) end

---@param other Vector
---@return Vector
function Vector:Cross(other) end

---@param other Vector
---@return Vector
function Vector:CrossProduct(other) end

---@param other Vector
---@return number
function Vector:DistanceTo(other) end

---@param other Vector
---@return number
function Vector:DistanceSquaredTo(other) end

---@param a Vector
---@param b Vector
---@return number
function Vector.Distance(a, b) end

---@param a Vector
---@param b Vector
---@return number
function Vector.Dist(a, b) end

---@param a Vector
---@param b Vector
---@return number
function Vector.DistSquared(a, b) end

---@param a Vector
---@param b Vector
---@param t number
---@return Vector
function Vector.Lerp(a, b, t) end

---@return Vector
function Vector.Zero() end

---@return Vector
function Vector.One() end

---@return Vector
function Vector.Forward() end

---@return Vector
function Vector.Backward() end

---@return Vector
function Vector.Right() end

---@return Vector
function Vector.Left() end

---@return Vector
function Vector.Up() end

---@return Vector
function Vector.Down() end

---@class Quat
---@field X number
---@field Y number
---@field Z number
---@field W number
---@field x number
---@field y number
---@field z number
---@field w number
Quat = {}

---@param x? number
---@param y? number
---@param z? number
---@param w? number
---@return Quat
function Quat(x, y, z, w) end

---@class Transform
---@field Location Vector
---@field Translation Vector
---@field Rotation Quat
---@field Scale Vector
Transform = {}

---@param translation? Vector
---@param rotation? Quat
---@param scale? Vector
---@return Transform
function Transform(translation, rotation, scale) end

---@class Object
---@field UUID integer
---@field Name string
---@field Type string
local Object = {}

---@return integer
function Object:GetUUID() end

---@return string
function Object:GetName() end

---@return string
function Object:GetType() end

---@class ActorComponent: Object
---@field TypeName string
---@field Owner AActor
---@field Active boolean
---@field AutoActivate boolean
---@field TickEnabled boolean
---@field EditorOnly boolean
local ActorComponent = {}

---@return AActor
function ActorComponent:GetOwner() end

---@return AActor
function ActorComponent:GetActor() end

---@return boolean
function ActorComponent:IsActive() end

---@param active boolean
function ActorComponent:SetActive(active) end

---@return boolean
function ActorComponent:IsAutoActivate() end

---@param autoActivate boolean
function ActorComponent:SetAutoActivate(autoActivate) end

---@return boolean
function ActorComponent:IsComponentTickEnabled() end

---@param tickEnabled boolean
function ActorComponent:SetComponentTickEnabled(tickEnabled) end

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

---@return SceneComponent|nil
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

---@class SoundComponent: SceneComponent
---@field Sound string
---@field Looping boolean
---@field Spatialized boolean
---@field VolumeScale number
local SoundComponent = {}

function SoundComponent:Play() end
function SoundComponent:Stop() end

---@return boolean
function SoundComponent:IsPlaying() end

---@param soundPath string
function SoundComponent:SetSound(soundPath) end

---@return string
function SoundComponent:GetSound() end

---@param enabled boolean
function SoundComponent:SetPlayOnBeginPlay(enabled) end

---@return boolean
function SoundComponent:IsPlayOnBeginPlay() end

---@param enabled boolean
function SoundComponent:SetLoop(enabled) end

---@return boolean
function SoundComponent:IsLooping() end

---@param enabled boolean
function SoundComponent:SetSpatialized(enabled) end

---@return boolean
function SoundComponent:IsSpatialized() end

---@param volumeScale number
function SoundComponent:SetVolumeScale(volumeScale) end

---@return number
function SoundComponent:GetVolumeScale() end

---@param minDistance number
---@param maxDistance number
function SoundComponent:Set3DMinMaxDistance(minDistance, maxDistance) end

---@return number
function SoundComponent:Get3DMinDistance() end

---@return number
function SoundComponent:Get3DMaxDistance() end

---attenuationModel: 0=None, 1=Inverse, 2=Linear, 3=Exponential.
---@param attenuationModel integer
---@param rolloffFactor number
function SoundComponent:Set3DAttenuation(attenuationModel, rolloffFactor) end

---@return integer
function SoundComponent:Get3DAttenuationModel() end

---@return number
function SoundComponent:Get3DRolloffFactor() end

---@class StaticMesh: Object
local StaticMesh = {}

---@return string
function StaticMesh:GetAssetPath() end

---@return boolean
function StaticMesh:HasValidMesh() end

---@return integer
function StaticMesh:GetValidLODCount() end

---@class StaticMeshComponent: ActorComponent
local StaticMeshComponent = {}

---@return StaticMesh|nil
function StaticMeshComponent:GetStaticMesh() end

---@param mesh StaticMesh
function StaticMeshComponent:SetStaticMesh(mesh) end

---@return boolean
function StaticMeshComponent:HasValidMesh() end

---@return integer
function StaticMeshComponent:GetPrimitiveType() end

---@class BillboardComponent: SceneComponent
local BillboardComponent = {}

---@param enabled boolean
function BillboardComponent:SetBillboardEnabled(enabled) end

---@param textureName string
function BillboardComponent:SetTextureName(textureName) end

---@return string
function BillboardComponent:GetTexture() end

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

---@param distance number
function CameraComponent:move_forward(distance) end

---@param distance number
function CameraComponent:move_right(distance) end

---@param distance number
function CameraComponent:move_up(distance) end

---@param yaw number
function CameraComponent:add_yaw_input(yaw) end

---@param pitch number
function CameraComponent:add_pitch_input(pitch) end

---@class PrimitiveComponent: SceneComponent
---@field Visible boolean
---@field EnableCull boolean
---@field GenerateOverlapEvents boolean
---@field NumMaterials integer
---@field SupportsOutline boolean
local PrimitiveComponent = {}

---@param other AActor
---@return boolean
function PrimitiveComponent:is_overlapping_actor(other) end

function PrimitiveComponent:clear_overlaps() end

---@class ShapeComponent: PrimitiveComponent
local ShapeComponent = {}

---@class BoxComponent: ShapeComponent
local BoxComponent = {}

---@class SphereComponent: ShapeComponent
---@field SphereRadius number
local SphereComponent = {}

---@return number
function SphereComponent:GetSphereRadius() end

---@class CapsuleComponent: ShapeComponent
---@field CapsuleHalfHeight number
---@field CapsuleRadius number
local CapsuleComponent = {}

---@return number
function CapsuleComponent:GetCapsuleHalfHeight() end
---@return number
function CapsuleComponent:GetCapsuleRadius() end

---@class FireballComponent: PrimitiveComponent
---@field Intensity number
---@field Radius number
---@field RadiusFallOff number
local FireballComponent = {}

---@class HeightFogComponent: PrimitiveComponent
---@field FogDensity number
---@field HeightFalloff number
---@field FogHeight number
---@field FogStartDistance number
---@field FogCutoffDistance number
---@field FogMaxOpacity number
local HeightFogComponent = {}

---@class SubUVComponent: BillboardComponent
---@field FrameIndex integer
---@field Loop boolean
local SubUVComponent = {}

---@param particleName string
function SubUVComponent:SetParticle(particleName) end
---@param fps number
function SubUVComponent:SetFrameRate(fps) end
function SubUVComponent:Play() end

---@class TextRenderComponent: PrimitiveComponent
---@field Text string
---@field FontSize number
local TextRenderComponent = {}

---@param fontName string
function TextRenderComponent:SetFont(fontName) end
---@param x number
---@param y number
function TextRenderComponent:SetScreenPosition(x, y) end

---@class LightComponentBase: SceneComponent
---@field Intensity number
---@field CastShadows boolean
local LightComponentBase = {}

---@class LightComponent: LightComponentBase
local LightComponent = {}

---@class AmbientLightComponent: LightComponent
local AmbientLightComponent = {}

---@class DirectionalLightComponent: LightComponent
local DirectionalLightComponent = {}

---@class PointLightComponent: LightComponent
---@field AttenuationRadius number
---@field LightFalloffExponent number
local PointLightComponent = {}

---@class SpotlightComponent: PointLightComponent
---@field InnerConeAngle number
---@field OuterConeAngle number
local SpotlightComponent = {}

---@class AActor: Object
---@field Name string
---@field TypeName string
---@field Location Vector
---@field Rotation Vector
---@field Scale Vector
---@field UID integer
---@field RootComponent SceneComponent
---@field Active boolean
---@field Visible boolean
---@field TickInEditor boolean
local AActor = {}

---@return AActor
function AActor:Duplicate() end

---@return Vector
function AActor:GetActorForwardVector() end

---@return Vector
function AActor:GetActorRightVector() end

---@return Vector
function AActor:GetActorUpVector() end

---@param tag string
function AActor:AddTag(tag) end

---@param tag string
function AActor:RemoveTag(tag) end

---@param tag string
---@return boolean
function AActor:HasTag(tag) end

function AActor:ClearTags() end

---@param offset Vector
function AActor:Add_Actor_World_Offset(offset) end

---@return Vector
function AActor:Get_Actor_Forward() end

---@return Vector
function AActor:Get_Actor_Right() end

---@return Vector
function AActor:Get_Actor_Up() end

---@return ActorComponent[]
function AActor:Get_Components() end

---@return ActorComponent[]
function AActor:GetComponents() end

---@return string[]
function AActor:GetTags() end

---@param typeName string
---@return ActorComponent|nil
function AActor:Get_Component_By_Type(typeName) end

---@param typeName string
---@return ActorComponent|nil
function AActor:GetComponent(typeName) end

---@param typeName string
---@return ActorComponent|nil
function AActor:GetComponentByType(typeName) end

---@param typeName string
---@return ActorComponent[]
function AActor:GetComponentsByType(typeName) end

---@param name string
---@return ActorComponent|nil
function AActor:FindComponentByName(name) end

---@param name string
---@return ActorComponent|nil
function AActor:GetComponentByName(name) end

---@param name string
---@return ActorComponent[]
function AActor:FindComponentsByName(name) end

---@param name string
---@return ActorComponent[]
function AActor:GetComponentsByName(name) end

---@param tag string
---@return ActorComponent|nil
function AActor:FindComponentByTag(tag) end

---@param tag string
---@return ActorComponent|nil
function AActor:GetComponentByTag(tag) end

---@param tag string
---@return ActorComponent[]
function AActor:FindComponentsByTag(tag) end

---@param tag string
---@return ActorComponent[]
function AActor:GetComponentsByTag(tag) end

---@param tags string[]
---@return ActorComponent[]
function AActor:FindComponentsByTags(tags) end

---@param tags string[]
---@return ActorComponent[]
function AActor:GetComponentsByTags(tags) end

---@return StaticMeshComponent|nil
function AActor:Get_Static_Mesh_Component() end

---@class PlayerController: AActor
---@field PossessedActor AActor|nil
---@field ViewTargetActor AActor|nil
---@field ViewTargetCamera CameraComponent|nil
local PlayerController = {}

---@param actor AActor
function PlayerController:Possess(actor) end
function PlayerController:UnPossess() end
---@param actor AActor
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
---@return AActor|nil
function PlayerController:GetPossessedActor() end
---@return AActor|nil
function PlayerController:GetViewTargetActor() end
---@return CameraComponent|nil
function PlayerController:GetViewTargetCamera() end

---@class ScriptSelf: ActorComponent
local ScriptSelf = {}

---@type ScriptSelf
Self = Self

---@type AActor
Actor = Actor

