local Script = {}
Script.__index = Script

Script.Properties = {
    SkeletalMeshComponentName = {
        Type = "String",
        Default = "",
        Category = "Controller"
    },
    SkeletalMeshComponentTag = {
        Type = "String",
        Default = "",
        Category = "Controller"
    },
    InitialFacingYaw = {
        Type = "Float",
        Default = 0.0,
        Category = "Controller"
    },
    MeshForwardYawOffset = {
        Type = "Float",
        Default = -90.0,
        Category = "Controller"
    },
    MoveSpeed = {
        Type = "Float",
        Default = 260.0,
        Min = 0.0,
        Max = 2000.0,
        Category = "Controller"
    },
    TurnSpeed = {
        Type = "Float",
        Default = 180.0,
        Min = -720.0,
        Max = 720.0,
        Category = "Controller"
    },
    InvertTurn = {
        Type = "Bool",
        Default = false,
        Category = "Controller"
    },
    AttackLockSeconds = {
        Type = "Float",
        Default = 0.55,
        Min = 0.0,
        Max = 3.0,
        Category = "Attack"
    },
    AttackCooldown = {
        Type = "Float",
        Default = 0.35,
        Min = 0.0,
        Max = 5.0,
        Category = "Attack"
    },
    ProjectileSpeed = {
        Type = "Float",
        Default = 700.0,
        Min = 0.0,
        Max = 5000.0,
        Category = "Attack"
    },
    ProjectileRange = {
        Type = "Float",
        Default = 1000.0,
        Min = 0.0,
        Max = 10000.0,
        Category = "Attack"
    },
    ProjectileSpawnForwardOffset = {
        Type = "Float",
        Default = 35.0,
        Min = -500.0,
        Max = 500.0,
        Category = "Attack"
    },
    ProjectileSpawnUpOffset = {
        Type = "Float",
        Default = 10.0,
        Min = -500.0,
        Max = 500.0,
        Category = "Attack"
    },
    ProjectileScale = {
        Type = "Float",
        Default = 8.0,
        Min = 0.01,
        Max = 10.0,
        Category = "Attack"
    },
    ProjectileYawOffset = {
        Type = "Float",
        Default = -90.0,
        Category = "Attack"
    },
    GongMeshPath = {
        Type = "String",
        Default = "Asset/Mesh/gong.fbx",
        Category = "Attack"
    },
    IsMovingVariableName = {
        Type = "String",
        Default = "IsMoving",
        Category = "Animation"
    },
    MoveSpeedVariableName = {
        Type = "String",
        Default = "MoveSpeed",
        Category = "Animation"
    },
    AttackTriggerName = {
        Type = "String",
        Default = "Attack",
        Category = "Animation"
    },
    AttackDoneVariableName = {
        Type = "String",
        Default = "AttackDone",
        Category = "Animation"
    },
    AttackFireNotifyName = {
        Type = "String",
        Default = "AttackFire",
        Category = "Animation"
    },
    FootstepEventId = {
        Type = "String",
        Default = "Footstep",
        Category = "Footstep"
    },
    FootstepNotifyName = {
        Type = "String",
        Default = "Footstep",
        Category = "Footstep"
    },
    LeftFootstepNotifyName = {
        Type = "String",
        Default = "LeftFootstep",
        Category = "Footstep"
    },
    RightFootstepNotifyName = {
        Type = "String",
        Default = "RightFootstep",
        Category = "Footstep"
    },
    FootstepDecalMaterialPath = {
        Type = "String",
        Default = "Asset/Material/DecalMat_Inst_4.matinst",
        Category = "Footstep"
    },
    FootstepSideOffset = {
        Type = "Float",
        Default = 1.5,
        Min = 0.0,
        Max = 100.0,
        Category = "Footstep"
    },
    FootstepLocationOffset = {
        Type = "Vector",
        Default = { X = 0.0, Y = 0.0, Z = 0.0 },
        Category = "Footstep"
    },
    FootstepRotationOffset = {
        Type = "Vector",
        Default = { X = 90.0, Y = 0.0, Z = 90.0 },
        Category = "Footstep"
    },
    FootstepScale = {
        Type = "Vector",
        Default = { X = 3.5, Y = 3.5, Z = 3.5 },
        Category = "Footstep"
    },
    FootstepDecalSize = {
        Type = "Vector",
        Default = { X = 1.0, Y = 1.0, Z = 1.0 },
        Category = "Footstep"
    }
}

local function applyProperties(target, properties)
    properties = properties or {}

    for key, desc in pairs(Script.Properties) do
        local value = properties[key]
        if value == nil then
            value = desc.Default
        end

        if desc.Type == "Vector" and type(value) == "table" then
            value = Vector(
                value.X or value.x or value[1] or 0.0,
                value.Y or value.y or value[2] or 0.0,
                value.Z or value.z or value[3] or 0.0)
        end

        target[key] = value
    end
end

local function isBlank(value)
    return value == nil or value == ""
end

local function matchesName(value, expected)
    return not isBlank(value) and not isBlank(expected) and value == expected
end

local function asSkeletalMeshComponent(component)
    if component == nil then
        return nil
    end
    if component.AsSkeletalMeshComponent ~= nil then
        return component:AsSkeletalMeshComponent()
    end
    return component
end

local function asDecalComponent(component)
    if component == nil then
        return nil
    end
    if component.AsDecalComponent ~= nil then
        return component:AsDecalComponent()
    end
    return component
end

local function asStaticMeshComponent(component)
    if component == nil then
        return nil
    end
    if component.AsStaticMeshComponent ~= nil then
        return component:AsStaticMeshComponent()
    end
    return component
end

local function findSkeletalMeshComponent(owner, name, tag)
    if owner == nil then
        return nil
    end

    if not isBlank(name) then
        local byName = owner:GetComponentByName(name)
        local mesh = asSkeletalMeshComponent(byName)
        if mesh ~= nil then
            return mesh
        end
    end

    if not isBlank(tag) then
        local byTag = owner:GetComponentByTag(tag)
        local mesh = asSkeletalMeshComponent(byTag)
        if mesh ~= nil then
            return mesh
        end
    end

    return asSkeletalMeshComponent(owner:GetComponentByType("SkeletalMeshComponent"))
end

local function yawToDirection(yawDegrees)
    local radians = math.rad(yawDegrees)
    return Vector(math.cos(radians), math.sin(radians), 0.0)
end

local function getFlatActorRightVector(actor)
    if actor ~= nil and actor.GetActorRightVector ~= nil then
        local right = actor:GetActorRightVector()
        right.Z = 0.0
        if right:Normalize() then
            return right
        end
    end

    return Vector(0.0, 1.0, 0.0)
end

local function readAxis(positiveKey, negativeKey)
    local input = Engine.API.Input
    local value = 0.0

    if input.IsRawKeyDown(positiveKey) then
        value = value + 1.0
    end
    if input.IsRawKeyDown(negativeKey) then
        value = value - 1.0
    end

    return value
end

function Script.new(component, properties)
    local self = setmetatable({}, Script)

    self.component = component
    self.owner = component:GetOwner()
    self.mesh = nil
    self.facingYaw = 0.0
    self.attackCooldownRemaining = 0.0
    self.attackLockRemaining = 0.0
    self.attackProjectilePending = false
    self.projectiles = {}

    applyProperties(self, properties)
    self.facingYaw = self.InitialFacingYaw

    return self
end

function Script:ResolveMesh()
    self.mesh = findSkeletalMeshComponent(
        self.owner,
        self.SkeletalMeshComponentName,
        self.SkeletalMeshComponentTag)

    return self.mesh
end

function Script:ApplyMeshFacing()
    local mesh = self.mesh or self:ResolveMesh()
    if mesh == nil then
        return
    end

    mesh.Rotation = Vector(0.0, 0.0, self.facingYaw + self.MeshForwardYawOffset)
end

function Script:SetAttackDone(value)
    local mesh = self.mesh or self:ResolveMesh()
    if mesh ~= nil then
        mesh:SetAnimVariableBool(self.AttackDoneVariableName, value)
    end
end

function Script:SetLocomotionVariables(isMoving, moveSpeed)
    local mesh = self.mesh or self:ResolveMesh()
    if mesh ~= nil then
        mesh:SetAnimVariableBool(self.IsMovingVariableName, isMoving)
        mesh:SetAnimVariableFloat(self.MoveSpeedVariableName, moveSpeed)
    end
end

function Script:IsAttackLocked()
    return self.attackLockRemaining > 0.0
end

function Script:BeginPlay()
    self:ResolveMesh()
    self:ApplyMeshFacing()
    self:SetLocomotionVariables(false, 0.0)
    self:SetAttackDone(true)
end

function Script:UpdateMovement(deltaTime)
    if self:IsAttackLocked() then
        -- 공격 모션 중에는 위치 이동과 매시 회전을 모두 막고, 블렌딩 입력도 정지 상태로 고정합니다.
        self:SetLocomotionVariables(false, 0.0)
        return
    end

    local moveAxis = readAxis("W", "S")
    local turnAxis = readAxis("D", "A")

    if self.InvertTurn then
        turnAxis = -turnAxis
    end

    if turnAxis ~= 0.0 then
        self.facingYaw = self.facingYaw + turnAxis * self.TurnSpeed * deltaTime
        self:ApplyMeshFacing()
    end

    local isMoving = moveAxis ~= 0.0
    local currentMoveSpeed = 0.0

    if isMoving then
        local direction = yawToDirection(self.facingYaw)
        local step = self.MoveSpeed * moveAxis * deltaTime

        -- Pawn 자체는 회전시키지 않고 위치만 이동시켜 SpringArm 방향을 고정합니다.
        self.owner.Location = self.owner.Location + direction * step
        currentMoveSpeed = math.abs(self.MoveSpeed * moveAxis)
    end

    self:SetLocomotionVariables(isMoving, currentMoveSpeed)
end

function Script:SpawnProjectile()
    if Engine == nil or Engine.API == nil then
        return
    end

    local asset = Engine.API.Asset
    local world = Engine.API.World
    if asset == nil or world == nil then
        return
    end

    local meshAsset = asset.LoadStaticMesh(self.GongMeshPath)
    if meshAsset == nil then
        LogWarning("[SpikeTopDownController] Failed to load projectile mesh: " .. self.GongMeshPath)
        return
    end

    local actor = world.SpawnActor("ASceneActor")
    if actor == nil then
        return
    end

    local component = actor:AddComponent("StaticMeshComponent", true)
    local mesh = asStaticMeshComponent(component)
    if mesh == nil then
        world.DestroyActor(actor)
        return
    end

    mesh:SetStaticMesh(meshAsset)
    mesh.Location = Vector(0.0, 0.0, 0.0)
    mesh.Rotation = Vector(0.0, 0.0, self.facingYaw + self.ProjectileYawOffset)
    mesh.Scale = Vector(self.ProjectileScale, self.ProjectileScale, self.ProjectileScale)

    local direction = yawToDirection(self.facingYaw)
    local spawnLocation =
        self.owner.Location +
        direction * self.ProjectileSpawnForwardOffset +
        Vector(0.0, 0.0, self.ProjectileSpawnUpOffset)

    actor.Location = spawnLocation

    table.insert(self.projectiles, {
        Actor = actor,
        Direction = direction,
        Distance = 0.0
    })
end

function Script:StartAttack()
    local mesh = self.mesh or self:ResolveMesh()
    if mesh ~= nil then
        mesh:SetAnimVariableBool(self.AttackDoneVariableName, false)
        mesh:SetAnimTrigger(self.AttackTriggerName)
    end

    self.attackLockRemaining = self.AttackLockSeconds
    self.attackCooldownRemaining = self.AttackCooldown
    self.attackProjectilePending = true
    self:SetLocomotionVariables(false, 0.0)

    if self.attackLockRemaining <= 0.0 then
        self:SetAttackDone(true)
    end
end

function Script:FireAttackProjectile()
    if not self.attackProjectilePending then
        return
    end

    self.attackProjectilePending = false
    self:SpawnProjectile()
end

function Script:SpawnFootstepDecal(payload, sourceComponent, sideOffset)
    if Engine == nil or Engine.API == nil then
        return
    end

    local world = Engine.API.World
    if world == nil or self.owner == nil then
        return
    end

    local materialPath = self.FootstepDecalMaterialPath

    local actor = world.SpawnActor("ASceneActor")
    if actor == nil then
        return
    end

    local component = actor:AddComponent("DecalComponent", true)
    local decal = asDecalComponent(component)
    if decal == nil then
        world.DestroyActor(actor)
        return
    end

    local footstepLocation = self.owner.Location + self.FootstepLocationOffset
    if sourceComponent ~= nil and sourceComponent.AsPrimitiveComponent ~= nil then
        local primitive = sourceComponent:AsPrimitiveComponent()
        if primitive ~= nil and
            primitive.IsWorldAABBValid ~= nil and
            primitive:IsWorldAABBValid() and
            primitive.GetWorldAABBCenter ~= nil and
            primitive.GetWorldAABBMin ~= nil then
            local center = primitive:GetWorldAABBCenter()
            local min = primitive:GetWorldAABBMin()
            footstepLocation = Vector(center.X, center.Y, min.Z) + self.FootstepLocationOffset
        end
    end

    sideOffset = sideOffset or 0.0
    if sideOffset ~= 0.0 then
        footstepLocation = footstepLocation + getFlatActorRightVector(self.owner) * sideOffset
    end

    actor.Location = footstepLocation
    actor.Rotation = self.FootstepRotationOffset
    actor.Scale = self.FootstepScale

    decal:SetSize(self.FootstepDecalSize)

    if decal.SetMaterialByPath ~= nil then
        if not decal:SetMaterialByPath(materialPath) then
            LogWarning("[SpikeTopDownController] Failed to load footstep decal material: " .. materialPath)
        end
    end
end

function Script:OnAnimNotify(notifyName, phase, sourceStateName, triggerWeight, sourceComponent, eventId, payload)
    local footstepSideOffset = nil

    if matchesName(eventId, self.LeftFootstepNotifyName) or matchesName(notifyName, self.LeftFootstepNotifyName) then
        footstepSideOffset = -self.FootstepSideOffset
    elseif matchesName(eventId, self.RightFootstepNotifyName) or matchesName(notifyName, self.RightFootstepNotifyName) then
        footstepSideOffset = self.FootstepSideOffset
    end

    if footstepSideOffset ~= nil and (phase == "Instant" or phase == "Begin") then
        self:SpawnFootstepDecal(payload, sourceComponent, footstepSideOffset)
        return
    end

    if sourceStateName ~= "Attack" then
        return
    end

    if notifyName ~= self.AttackFireNotifyName then
        return
    end

    if phase ~= "Instant" and phase ~= "Begin" then
        return
    end

    -- 같은 공격 애니메이션에서 notify가 여러 번 들어와도 투사체는 한 번만 생성합니다.
    self:FireAttackProjectile()
end

function Script:UpdateAttack(deltaTime)
    if self.attackCooldownRemaining > 0.0 then
        self.attackCooldownRemaining = math.max(0.0, self.attackCooldownRemaining - deltaTime)
    end

    if self.attackLockRemaining > 0.0 then
        self.attackLockRemaining = self.attackLockRemaining - deltaTime
        if self.attackLockRemaining <= 0.0 then
            self.attackLockRemaining = 0.0
            self:SetAttackDone(true)
        end
    end

    local canStartAttack =
        Engine.API.Input.IsRawMousePressed("LeftMouse") and
        self.attackCooldownRemaining <= 0.0 and
        self.attackLockRemaining <= 0.0

    if canStartAttack then
        self:StartAttack()
    end
end

function Script:UpdateProjectiles(deltaTime)
    local world = Engine.API.World
    if world == nil then
        return
    end

    for index = #self.projectiles, 1, -1 do
        local projectile = self.projectiles[index]
        local actor = projectile.Actor

        if actor == nil then
            table.remove(self.projectiles, index)
        else
            local step = self.ProjectileSpeed * deltaTime
            actor.Location = actor.Location + projectile.Direction * step
            projectile.Distance = projectile.Distance + math.abs(step)

            if projectile.Distance >= self.ProjectileRange then
                world.DestroyActor(actor)
                table.remove(self.projectiles, index)
            end
        end
    end
end

function Script:Tick(deltaTime)
    if Engine == nil or Engine.API == nil or Engine.API.Input == nil then
        return
    end

    self:UpdateAttack(deltaTime)
    self:UpdateMovement(deltaTime)
    self:UpdateProjectiles(deltaTime)
end

function Script:EndPlay()
    if Engine == nil or Engine.API == nil or Engine.API.World == nil then
        return
    end

    for _, projectile in ipairs(self.projectiles) do
        if projectile.Actor ~= nil then
            Engine.API.World.DestroyActor(projectile.Actor)
        end
    end

    self.projectiles = {}
end

return Script
