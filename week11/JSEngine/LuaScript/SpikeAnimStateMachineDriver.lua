local Script = {}
Script.__index = Script

Script.Properties = {
    StateMachinePath = {
        Type = "String",
        Default = "Asset/Animation/StateMachine/Spike_Locomotion_Attack.animsm",
        Category = "Animation"
    },
    SkeletalMeshComponentName = {
        Type = "String",
        Default = "",
        Category = "Animation"
    },
    SkeletalMeshComponentTag = {
        Type = "String",
        Default = "",
        Category = "Animation"
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
    AttackDoneVariableName = {
        Type = "String",
        Default = "AttackDone",
        Category = "Animation"
    }
}

local function applyProperties(target, properties)
    properties = properties or {}

    for key, desc in pairs(Script.Properties) do
        if properties[key] ~= nil then
            target[key] = properties[key]
        else
            target[key] = desc.Default
        end
    end
end

local function isBlank(value)
    return value == nil or value == ""
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

function Script.new(component, properties)
    local self = setmetatable({}, Script)

    self.component = component
    self.owner = component:GetOwner()
    self.mesh = nil

    applyProperties(self, properties)
    return self
end

function Script:ResolveMesh()
    self.mesh = findSkeletalMeshComponent(
        self.owner,
        self.SkeletalMeshComponentName,
        self.SkeletalMeshComponentTag)

    return self.mesh
end

function Script:BeginPlay()
    local mesh = self:ResolveMesh()
    if mesh == nil then
        LogWarning("[SpikeAnimStateMachineDriver] SkeletalMeshComponent not found.")
        return
    end

    if not isBlank(self.StateMachinePath) then
        if not mesh:SetAnimStateMachine(self.StateMachinePath) then
            LogWarning("[SpikeAnimStateMachineDriver] Failed to set state machine: " .. self.StateMachinePath)
        end
    end

    -- 상태 머신 조건이 첫 프레임부터 안정적으로 평가되도록 기본 변수를 초기화합니다.
    mesh:SetAnimVariableBool(self.IsMovingVariableName, false)
    mesh:SetAnimVariableFloat(self.MoveSpeedVariableName, 0.0)
    mesh:SetAnimVariableBool(self.AttackDoneVariableName, true)
end

function Script:CanExitAttack()
    local mesh = self.mesh or self:ResolveMesh()
    if mesh == nil then
        return true
    end

    return mesh:GetAnimVariableBool(self.AttackDoneVariableName, true)
end

return Script
