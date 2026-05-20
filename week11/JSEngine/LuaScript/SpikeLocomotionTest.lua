local Script = {}
Script.__index = Script

Script.Properties = {
    bCanTick = {
        Type = "Bool",
        Default = true,
        Category = "Script"
    },
    AttackDuration = {
        Type = "Float",
        Default = 0.8,
        Category = "Animation"
    },
    LogInterval = {
        Type = "Float",
        Default = 0.35,
        Category = "Debug"
    }
}

local StateMachinePath = "Asset/Animation/StateMachine/SpikeLocomotion.animsm"

local function FindSkeletalMeshComponent(owner)
    local meshComponent = owner:GetComponentByType("SkeletalMeshComponent")
    local mesh = meshComponent and meshComponent:AsSkeletalMeshComponent() or nil
    if mesh ~= nil then
        return mesh
    end

    local components = owner:GetComponents()
    for _, component in pairs(components) do
        mesh = component and component:AsSkeletalMeshComponent() or nil
        if mesh ~= nil then
            return mesh
        end
    end

    return nil
end

local function FindMovementComponent(owner)
    local movement = owner:GetComponentByType("LocomotionTestMovementComponent")
    if movement ~= nil then
        return movement
    end

    return owner:GetComponentByType("MovementComponent")
end

local function GetInputAPI()
    if Engine == nil or Engine.API == nil then
        return nil
    end
    return Engine.API.Input
end

local function GetMovementSpeed(movement)
    if movement == nil then
        return 0.0
    end

    local velocity = movement:GetProperty("Velocity")
    if velocity == nil then
        return 0.0
    end

    return velocity:Size()
end

function Script.new(component, properties)
    local self = setmetatable({}, Script)

    self.component = component
    self.owner = component:GetOwner()

    properties = properties or {}
    for key, desc in pairs(Script.Properties) do
        if properties[key] ~= nil then
            self[key] = properties[key]
        else
            self[key] = desc.Default
        end
    end

    self.mesh = nil
    self.movement = nil
    self.attackActive = false
    self.attackRequested = false
    self.attackTimer = 0.0
    self.wasMoving = false
    self.lastState = ""
    self.logTimer = 0.0

    return self
end

function Script:BeginPlay()
    self.mesh = FindSkeletalMeshComponent(self.owner)
    self.movement = FindMovementComponent(self.owner)

    if self.movement == nil then
        Log("[SpikeLocomotionTest] LocomotionTestMovementComponent not found")
    end

    if self.mesh == nil then
        Log("[SpikeLocomotionTest] SkeletalMeshComponent not found")
        return
    end

    local loaded = self.mesh:SetAnimStateMachine(StateMachinePath)
    self.mesh:SetAnimVariableFloat("Speed", 0.0)
    self.mesh:SetAnimVariableBool("IsMoving", false)

    Log("[SpikeLocomotionTest] SetAnimStateMachine(" .. StateMachinePath .. ") = " .. tostring(loaded))

    local Input = GetInputAPI()
    if Input ~= nil then
        Input.SetInputModeGameOnly()
        Input.SetCursorVisible(false)
        Input.SetMouseCapture(true)
    end
end

function Script:Tick(deltaTime)
    if self.mesh == nil then
        return
    end

    local speed = GetMovementSpeed(self.movement)
    local isMoving = speed > 0.1

    self.mesh:SetAnimVariableFloat("Speed", speed)
    self.mesh:SetAnimVariableBool("IsMoving", isMoving)
    self.wasMoving = isMoving

    local Input = GetInputAPI()
    if Input ~= nil and Input.IsRawMousePressed("LeftMouse") and not self.attackActive then
        self.attackActive = true
        self.attackRequested = true
        self.attackTimer = self.AttackDuration
    end

    if self.attackActive then
        self.attackTimer = self.attackTimer - deltaTime
    end

    local currentState = self.mesh:GetCurrentAnimStateName()

    if currentState == "Attack" then
        self.attackRequested = false
    elseif self.attackActive and self.attackTimer <= 0.0 then
        self.attackActive = false
    end

    self:LogStateIfNeeded(deltaTime, currentState, speed)
end

function Script:ShouldEnterAttack()
    return self.attackRequested
end

function Script:ShouldExitAttackToWalk()
    return self.attackActive and self.attackTimer <= 0.0 and self.wasMoving
end

function Script:ShouldExitAttackToIdle()
    return self.attackActive and self.attackTimer <= 0.0 and not self.wasMoving
end

function Script:LogStateIfNeeded(deltaTime, currentState, speed)
    self.logTimer = self.logTimer - deltaTime

    local stateChanged = currentState ~= self.lastState
    if not stateChanged and self.logTimer > 0.0 then
        return
    end

    self.lastState = currentState
    self.logTimer = self.LogInterval

    local targetState = self.mesh:GetTargetAnimStateName()
    local alpha = self.mesh:GetAnimTransitionAlpha()
    local transitioning = self.mesh:IsAnimTransitioning()

    Log(string.format(
        "[SpikeLocomotionTest] State=%s Target=%s Transitioning=%s Alpha=%.2f Speed=%.2f AttackTimer=%.2f",
        tostring(currentState),
        tostring(targetState),
        tostring(transitioning),
        alpha,
        speed,
        self.attackTimer))
end

function Script:EndPlay()
    local Input = GetInputAPI()
    if Input ~= nil then
        Input.ReleaseMouseCapture()
        Input.SetCursorVisible(true)
    end
end

return Script

