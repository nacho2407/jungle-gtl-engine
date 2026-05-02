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
---@field Owner AActor
---@field Active boolean
---@field AutoActivate boolean
---@field TickEnabled boolean
---@field EditorOnly boolean
local ActorComponent = {}

---@return AActor
function ActorComponent:GetOwner() end

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

---@class SoundComponent: SceneComponent
---@field Sound string
local SoundComponent = {}

function SoundComponent:Play() end
function SoundComponent:Stop() end

---@return boolean
function SoundComponent:IsPlaying() end

---@param soundPath string
function SoundComponent:SetSound(soundPath) end

---@return string
function SoundComponent:GetSound() end

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

---@class AActor: Object
---@field Name string
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

---@return ActorComponent[]
function AActor:Get_Components() end

---@return string[]
function AActor:GetTags() end

---@param typeName string
---@return ActorComponent|nil
function AActor:Get_Component_By_Type(typeName) end

---@return StaticMeshComponent|nil
function AActor:Get_Static_Mesh_Component() end

---@class ScriptSelf: ActorComponent
local ScriptSelf = {}

---@type ScriptSelf
Self = Self

---@type AActor
Actor = Actor

