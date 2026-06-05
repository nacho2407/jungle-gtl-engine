---@meta

-- Auto-generated from C++ Lua binding metadata.

---@class Vector
---@field X number
---@field Y number
---@field Z number
Vector = {}

---@param x? number
---@param y? number
---@param z? number
---@return Vector
function Vector.new(x, y, z) end

---@return number
function Vector:Length() end

---@return nil
function Vector:Normalize() end

---@return Vector
function Vector:Normalized() end

---@param other Vector
---@return number
function Vector:Dot(other) end

---@param other Vector
---@return Vector
function Vector:Cross(other) end

---@param a Vector
---@param b Vector
---@return number
function Vector.Distance(a, b) end

---@param a Vector
---@param b Vector
---@return number
function Vector.DistSquared(a, b) end

---@param a Vector
---@param b Vector
---@param alpha number
---@return Vector
function Vector.Lerp(a, b, alpha) end

---@return Vector
function Vector.Zero() end

---@return Vector
function Vector.One() end

---@return Vector
function Vector.Up() end

---@return Vector
function Vector.Forward() end

---@return Vector
function Vector.Right() end

---@class Transform
---@field Location Vector
---@field Rotation Vector
---@field Scale Vector
Transform = {}

---@return Transform
function Transform.new() end

---@param location Vector
---@param rotation Vector
---@param scale Vector
---@return Transform
function Transform.New(location, rotation, scale) end

---@return Transform
function Transform.Identity() end

---@class ActionComponent
ActionComponent = {}

---@param duration number
function ActionComponent:HitStop(duration) end

---@param direction Vector
---@param distance number
---@param duration number
function ActionComponent:Knockback(direction, distance, duration) end

function ActionComponent:StopAllActions() end

---@class FloatingPawnMovementComponent
FloatingPawnMovementComponent = {}

---@param input Vector
function FloatingPawnMovementComponent:SetMoveInput(input) end

---@param input Vector
function FloatingPawnMovementComponent:SetLookInput(input) end

---@class VehicleMovementComponent4W
VehicleMovementComponent4W = {}

---@param throttle number
---@param brake number
---@param steer number
---@param reverse boolean
function VehicleMovementComponent4W:SetDriveInput(throttle, brake, steer, reverse) end

---@param Throttle number
---@param Brake number
---@param Steer number
---@param bReverse boolean
function VehicleMovementComponent4W:SetDriveInput(Throttle, Brake, Steer, bReverse) end

---@class SceneComponent
---@field Location Vector
---@field Rotation Vector
---@field RelativeLocation Vector
---@field Forward Vector
---@field Right Vector
---@field Up Vector
SceneComponent = {}

---@return Vector
function SceneComponent:GetLocation() end

---@param location Vector
function SceneComponent:SetLocation(location) end

---@return Vector
function SceneComponent:GetRotation() end

---@param rotation Vector
function SceneComponent:SetRotation(rotation) end

---@param NewLocation Vector
function SceneComponent:SetRelativeLocation(NewLocation) end

---@param NewRotation Vector
function SceneComponent:SetRelativeRotation(NewRotation) end

---@param NewWorldLocation Vector
function SceneComponent:SetWorldLocation(NewWorldLocation) end

---@return Vector
function SceneComponent:GetWorldLocation() end

---@return Vector
function SceneComponent:GetForwardVector() end

---@return Vector
function SceneComponent:GetUpVector() end

---@return Vector
function SceneComponent:GetRightVector() end

---@class StaticMeshComponent: PrimitiveComponent
---@field MeshPath string
StaticMeshComponent = {}

---@return string
function StaticMeshComponent:GetMeshPath() end

---@class ParticleSystemComponent: PrimitiveComponent
ParticleSystemComponent = {}

---@param parameterName string
---@param value Vector
function ParticleSystemComponent:SetVectorParameter(parameterName, value) end

function ParticleSystemComponent:Activate() end

function ParticleSystemComponent:Deactivate() end

function ParticleSystemComponent:ResetSystem() end

---@param enabled boolean
function ParticleSystemComponent:SetEmitterSpawningEnabled(enabled) end

---@class HitResult
---@field HitComponent PrimitiveComponent?
---@field HitActor Actor?
---@field Distance number
---@field PenetrationDepth number
---@field WorldHitLocation Vector
---@field WorldNormal Vector
---@field ImpactNormal Vector
---@field FaceIndex integer
---@field bHit boolean
HitResult = {}

---@class CameraComponent: SceneComponent
CameraComponent = {}

---@class SkinnedMeshComponent: PrimitiveComponent
SkinnedMeshComponent = {}

---@param BoneName string
---@return integer
function SkinnedMeshComponent:FindBoneIndex(BoneName) end

---@param BoneIndex integer
---@return boolean
---@return Transform
function SkinnedMeshComponent:GetBoneWorldTransformByIndex(BoneIndex) end

---@param BoneName string
---@return boolean
---@return Transform
function SkinnedMeshComponent:GetBoneWorldTransformByName(BoneName) end

---@param BoneName string
---@param LocalOffset Transform
---@return boolean
---@return Transform
function SkinnedMeshComponent:GetBoneSocketWorldTransform(BoneName, LocalOffset) end

---@class Actor
---@field UUID integer
---@field Name string
---@field Location Vector
---@field Rotation Vector
---@field Scale Vector
---@field Forward Vector
---@field Right Vector
Actor = {}

---@param offset Vector
function Actor:AddWorldOffset(offset) end

function Actor:Destroy() end

---@return boolean
function Actor:IsValid() end

---@return FloatingPawnMovementComponent?
function Actor:GetFloatingPawnMovement() end

---@return VehicleMovementComponent4W?
function Actor:GetVehicleMovement() end

---@return VehicleMovementComponentTank?
function Actor:GetTankVehicleMovement() end

---@return CameraComponent?
function Actor:GetCamera() end

---@return ActionComponent?
function Actor:GetActionComponent() end

---@return InputComponent?
function Actor:GetInputComponent() end

---@return PrimitiveComponent?
function Actor:GetRootPrimitiveComponent() end

---@return PrimitiveComponent?
function Actor:GetPrimitiveComponent() end

---@return SkeletalMeshComponent?
function Actor:GetSkeletalMesh() end

---@return ParticleSystemComponent?
function Actor:GetParticleSystem() end

---@param name string
---@return PrimitiveComponent?
function Actor:GetPrimitiveComponentByName(name) end

---@param name string
---@return SceneComponent?
function Actor:GetComponentByName(name) end

---@return Vector
function Actor:GetActorLocation() end

---@param Location Vector
function Actor:SetActorLocation(Location) end

---@param Delta Vector
function Actor:AddActorWorldOffset(Delta) end

---@return Vector
function Actor:GetActorRotation() end

---@param NewRotation Vector
function Actor:SetActorRotation(NewRotation) end

---@param EulerRotation Vector
function Actor:SetActorRotation(EulerRotation) end

---@return Vector
function Actor:GetActorScale() end

---@param NewScale Vector
function Actor:SetActorScale(NewScale) end

---@return Vector
function Actor:GetActorForward() end

---@return Vector
function Actor:GetActorRight() end

---@param Visible boolean
function Actor:SetVisible(Visible) end

---@param Tag string
---@return boolean
function Actor:HasTag(Tag) end

---@param Tag string
function Actor:AddTag(Tag) end

---@param Tag string
function Actor:RemoveTag(Tag) end

---@class Pawn: Actor
Pawn = {}

---@return boolean
function Pawn:IsPossessed() end

---@param enabled boolean
function Pawn:SetAutoPossessPlayer(enabled) end

---@return boolean
function Pawn:GetAutoPossessPlayer() end

---@return InputComponent?
function Pawn:GetInputComponent() end

---@class InputComponent
InputComponent = {}

---@param name string
---@param key integer
---@param scale? number
function InputComponent:AddAxisMapping(name, key, scale) end

---@param name string
---@param key integer
function InputComponent:AddActionMapping(name, key) end

---@param name string
---@param callback fun(value: number)
function InputComponent:BindAxis(name, callback) end

---@param name string
---@param event 'Pressed'|'Released'
---@param callback fun()
function InputComponent:BindAction(name, event, callback) end

function InputComponent:ClearBindings() end

function InputComponent:ClearMappings() end

function InputComponent:ClearAllMappingsAndBindings() end

---@class WorldLib
WorldLib = {}

---@param className string
---@return Actor?
function World.SpawnActor(className) end

---@param actorName string
---@return Actor?
function World.FindActorByName(actorName) end

---@param className string
---@return Actor?
function World.FindFirstActorByClass(className) end

---@param tag string
---@return Actor?
function World.FindFirstActorByTag(tag) end

---@param tag string
---@return Actor[]
function World.FindActorsByTag(tag) end

---@class SkeletalMeshComponent: PrimitiveComponent
SkeletalMeshComponent = {}

---@param animationPath string
---@param looping? boolean
---@return boolean
function SkeletalMeshComponent:PlayAnimationByPath(animationPath, looping) end

---@param enabled boolean
function SkeletalMeshComponent:SetSimulatePhysics(enabled) end

---@return boolean
function SkeletalMeshComponent:IsSimulatingPhysics() end

---@param weight number
function SkeletalMeshComponent:SetPhysicsBlendWeight(weight) end

---@return number
function SkeletalMeshComponent:GetPhysicsBlendWeight() end

---@param boneName string
---@param localOffset Vector
---@return Vector
function SkeletalMeshComponent:GetBoneSocketLocation(boneName, localOffset) end

---@param boneName string
---@param localOffset Vector
---@return Vector
function SkeletalMeshComponent:GetBoneSocketRotation(boneName, localOffset) end

---@class VehicleMovementComponentTank
VehicleMovementComponentTank = {}

---@param Throttle number
---@param Brake number
---@param Steer number
---@param bReverse boolean
function VehicleMovementComponentTank:SetDriveInput(Throttle, Brake, Steer, bReverse) end

---@param LeftThrust number
---@param RightThrust number
---@param LeftBrake number
---@param RightBrake number
function VehicleMovementComponentTank:SetTrackInput(LeftThrust, RightThrust, LeftBrake, RightBrake) end

---@param Impulse number
---@param LocalFirePoint Vector
---@param LocalDirection Vector
function VehicleMovementComponentTank:FireRecoil(Impulse, LocalFirePoint, LocalDirection) end

---@param YawInput number
function VehicleMovementComponentTank:SetTurretInput(YawInput) end

---@param YawDegrees number
function VehicleMovementComponentTank:SetTurretYaw(YawDegrees) end

---@return number
function VehicleMovementComponentTank:GetTurretYaw() end

---@return Vector
function VehicleMovementComponentTank:GetTurretForward() end

---@param Impulse number
---@param TurretLocalFirePoint Vector
---@param TurretLocalDirection Vector
function VehicleMovementComponentTank:FireTurretRecoil(Impulse, TurretLocalFirePoint, TurretLocalDirection) end

function VehicleMovementComponentTank:FireMainGun() end

---@return number
function VehicleMovementComponentTank:GetLeftTrackSpeed() end

---@return number
function VehicleMovementComponentTank:GetRightTrackSpeed() end

---@param WheelIndex integer
---@return number
function VehicleMovementComponentTank:GetWheelRotationAngle(WheelIndex) end

---@param WheelIndex integer
---@return number
function VehicleMovementComponentTank:GetWheelRotationSpeed(WheelIndex) end

---@return integer
function VehicleMovementComponentTank:GetWheelCount() end

---@class AnimNode
AnimNode = {}

---@class AnimLib
AnimLib = {}

---@return number
function Anim.get_owner_speed() end

---@return string
function Anim.get_owner_movement_mode() end

---@return boolean
function Anim.is_owner_falling() end

---@param path string
---@param section? string
---@param rate? number
---@param blendIn? number
---@param slotName? string
function Anim.play_montage(path, section, rate, blendIn, slotName) end

---@param blendOut? number
---@param slotName? string
function Anim.stop_montage(blendOut, slotName) end

---@param slotName? string
---@return boolean
function Anim.is_montage_playing(slotName) end

---@param sectionName string
---@param slotName? string
function Anim.jump_to_section(sectionName, slotName) end

---@return boolean
function Anim.is_left_mouse_pressed() end

---@return boolean
function Anim.is_left_mouse_down() end

---@return boolean
function Anim.is_right_mouse_pressed() end

---@param key integer
---@return boolean
function Anim.is_key_pressed(key) end

---@param name? string
---@return AnimNode
function Anim.create_state_machine(name) end

---@param path string
---@param rate number
---@param loop boolean
---@return AnimNode
function Anim.create_sequence_player(path, rate, loop) end

---@param stateMachine AnimNode
---@param name string
---@param subGraph AnimNode
function Anim.sm_add_state(stateMachine, name, subGraph) end

---@param stateMachine AnimNode
---@param from string
---@param to string
---@param condition fun(): boolean
---@param blendTime number
function Anim.sm_add_transition(stateMachine, from, to, condition, blendTime) end

---@param stateMachine AnimNode
---@param name string
function Anim.sm_set_initial_state(stateMachine, name) end

---@param root AnimNode
function Anim.set_root_node(root) end

---@param name string
---@param input AnimNode
---@return AnimNode
function Anim.create_slot(name, input) end

---@return AnimNode
function Anim.create_ref_pose() end

---@param base AnimNode
---@param blend AnimNode
---@param maskRootBone string
---@return AnimNode
function Anim.create_layered_blend_per_bone(base, blend, maskRootBone) end

---@param initialIndex? integer
---@param blendTime? number
---@return AnimNode
function Anim.create_blend_list_by_enum(initialIndex, blendTime) end

---@param blendList AnimNode
---@param pose AnimNode
function Anim.blend_list_add_pose(blendList, pose) end

---@param blendList AnimNode
---@param index integer
function Anim.blend_list_set_active(blendList, index) end

---@type WorldLib
World = World

---@type Actor
obj = obj

---@type any
this = this

---@type table
self = self

---@type AnimLib
Anim = Anim

