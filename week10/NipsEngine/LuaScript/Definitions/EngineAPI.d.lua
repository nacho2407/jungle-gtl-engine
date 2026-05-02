---@meta

---@class Vector
---@field X number
---@field Y number
---@field Z number
Vector = {}

---@return Vector
function Vector.new() end

---@param x number
---@param y number
---@param z number
---@return Vector
function Vector.new(x, y, z) end

---@class Quat
---@field X number
---@field Y number
---@field Z number
---@field W number
Quat = {}

---@class Transform
---@field Location Vector
---@field Rotation Quat
---@field Scale Vector
Transform = {}

-------------------------------Component
---@class UObject

---@class UActorComponent: UObject

---@class USceneComponent: UActorComponent

---@class UMeshComponent: UPrimitiveComponent

---@class UPrimitiveComponent: USceneComponent
---@field Visible boolean
---@field EnableCull boolean
---@field GenerateOverlapEvents boolean
---@field NumMaterials integer
---@field SupportsOutline boolean
UPrimitiveComponent = {}

---@param actor AActor
---@return boolean
function UPrimitiveComponent:is_overlapping_actor(actor) end

function UPrimitiveComponent:clear_overlaps() end


---@class StaticMesh: UObject
StaticMesh = {}

---@return string
function StaticMesh:GetAssetPath() end

---@return boolean
function StaticMesh:HasValidMesh() end

---@return integer
function StaticMesh:GetValidLODCount() end


---@class StaticMeshComponent: UMeshComponent
StaticMeshComponent = {}

---@return StaticMesh
function StaticMeshComponent:GetStaticMesh() end

---@param mesh StaticMesh
function StaticMeshComponent:SetStaticMesh(mesh) end

---@return boolean
function StaticMeshComponent:HasValidMesh() end

---@return unknown
function StaticMeshComponent:GetPrimitiveType() end


---@class BillboardComponent: USceneComponent
BillboardComponent = {}

---@param enabled boolean
function BillboardComponent:SetBillboardEnabled(enabled) end

---@param textureName string
function BillboardComponent:SetTextureName(textureName) end

---@return unknown
function BillboardComponent:GetTexture() end


---@class CameraComponent: USceneComponent
---@field FOV number
---@field OrthoWidth number
---@field Orthographic boolean
---@field version string
---@field FarPlane number
---@field Forward Vector
---@field Right Vector
---@field Up Vector
CameraComponent = {}

---@param target Vector
function CameraComponent:look_at(target) end

---@param distance number
function CameraComponent:move_forward(distance) end

---@param distance number
function CameraComponent:move_right(distance) end

---@param distance number
function CameraComponent:move_up(distance) end

---@param deltaYawDegrees number
function CameraComponent:add_yaw_input(deltaYawDegrees) end

---@param deltaPitchDegrees number
function CameraComponent:add_pitch_input(deltaPitchDegrees) end

-----------AActor
---@class AActor
---@field Name string
---@field Location Vector
---@field Rotation Vector
---@field Scale Vector
---@field UUID string
---@field RootComponent USceneComponent
---@field Active boolean
---@field Visible boolean
---@field TickInEditor boolean
AActor = {}

---@return AActor
function AActor:Duplicate() end

---@return Vector
function AActor:GetActorForwardVector() end

---@param delta Vector
function AActor:Add_Actor_World_Offset(delta) end

---@return Vector
function AActor:Get_Actor_Forward() end

---@return table
function AActor:Get_Components() end

---@param message string
function Log(message) end

---@type AActor
Actor = nil
