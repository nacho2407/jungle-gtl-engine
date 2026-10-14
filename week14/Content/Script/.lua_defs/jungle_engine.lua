---@meta

---@class Vector
---@field X number
---@field Y number
---@field Z number
local Vector = {}

---@return Vector
function Vector.new(x, y, z) end

---@return number
function Vector:Length() end

---@return Vector
function Vector:Normalized() end

---@param other Vector
---@return number
function Vector:Dot(other) end

---@param other Vector
---@return Vector
function Vector:Cross(other) end

---@class SceneComponent
---@field Location Vector
---@field Rotation Vector
---@field RelativeLocation Vector
---@field Forward Vector
---@field Right Vector
---@field Up Vector
local SceneComponent = {}

---@return Vector
function SceneComponent:GetLocation() end

---@param location Vector
function SceneComponent:SetLocation(location) end

---@return Vector
function SceneComponent:GetRotation() end

---@param rotation Vector
function SceneComponent:SetRotation(rotation) end

---@class PrimitiveComponent: SceneComponent
local PrimitiveComponent = {}

---@param enabled boolean
function PrimitiveComponent:SetSimulatePhysics(enabled) end

---@return boolean
function PrimitiveComponent:GetSimulatePhysics() end

---@param force Vector
function PrimitiveComponent:AddForce(force) end

---@class StaticMeshComponent: PrimitiveComponent
---@field MeshPath string
local StaticMeshComponent = {}

---@return string
function StaticMeshComponent:GetMeshPath() end

---@class SkeletalMeshComponent: PrimitiveComponent
local SkeletalMeshComponent = {}

---@param boneName string
---@param localOffset Vector
---@return Vector
function SkeletalMeshComponent:GetBoneSocketLocation(boneName, localOffset) end

---@param boneName string
---@param localOffset Vector
---@return Vector
function SkeletalMeshComponent:GetBoneSocketRotation(boneName, localOffset) end

---@class Actor
---@field Location Vector
---@field Rotation Vector
---@field Scale Vector
---@field Forward Vector
---@field Right Vector
---@field UUID integer
---@field Name string
local Actor = {}

---@param offset Vector
function Actor:AddWorldOffset(offset) end

function Actor:Destroy() end

---@return boolean
function Actor:IsValid() end

---@param tag string
---@return boolean
function Actor:HasTag(tag) end

---@param tag string
function Actor:AddTag(tag) end

---@param name string
---@return SceneComponent?
function Actor:GetComponentByName(name) end

---@param name string
---@return PrimitiveComponent?
function Actor:GetPrimitiveComponentByName(name) end

---@return SkeletalMeshComponent?
function Actor:GetSkeletalMesh() end

---@return PrimitiveComponent?
function Actor:GetPrimitiveComponent() end

---@type Actor
obj = obj

---@class WorldLib
World = {}

---@param name string
---@return Actor?
function World.FindActorByName(name) end

---@param tag string
---@return Actor?
function World.FindFirstActorByTag(tag) end

---@param tag string
---@return Actor[]
function World.FindActorsByTag(tag) end