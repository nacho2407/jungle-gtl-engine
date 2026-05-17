local StateMachineTest = {}
StateMachineTest.__index = StateMachineTest

StateMachineTest.Properties = {
    AutoCycle = { Type = "Bool", Default = true, Category = "StateMachine Test" },
    CycleInterval = { Type = "Float", Default = 2.0, Min = 0.1, Category = "StateMachine Test" },
    BlendTime = { Type = "Float", Default = 0.2, Min = 0.0, Category = "StateMachine Test" },
    EntryState = { Type = "String", Default = "Idle", Category = "StateMachine Test" },
}

local AnimPaths = {
    Idle = "Asset/Animation/Ahri_Skeleton_Skeleton_Skeleton_Idle1_Base.animseq",
    Run = "Asset/Animation/Ahri_Skeleton_Skeleton_Skeleton_Run_Base.animseq",
    Attack = "Asset/Animation/Ahri_Skeleton_Skeleton_Skeleton_Attack1.animseq",
    Spell = "Asset/Animation/Ahri_Skeleton_Skeleton_Skeleton_Spell4.animseq",
    Recall = "Asset/Animation/Ahri_Skeleton_Skeleton_Skeleton_Recall.animseq",
}

local CycleStates = { "Idle", "Run", "Attack", "Spell", "Recall" }

local function log(message)
    if Engine and Engine.API and Engine.API.Debug then
        Engine.API.Debug.Log(message)
    elseif Log then
        Log(message)
    end
end

function StateMachineTest.new(scriptComponent, properties)
    local self = setmetatable({}, StateMachineTest)

    self.Component = scriptComponent
    self.Actor = scriptComponent and scriptComponent:GetOwner() or Actor
    self.Properties = properties or {}
    self.Mesh = nil
    self.StateMachine = nil
    self.Elapsed = 0.0
    self.CurrentIndex = 1
    self.bInitialized = false

    return self
end

function StateMachineTest:BeginPlay()
    self:InitializeStateMachine()
end

function StateMachineTest:InitializeStateMachine()
    if self.bInitialized then
        return true
    end

    local owner = self.Actor or Actor or Owner
    if not owner then
        log("[StateMachineTest] Owner actor is missing.")
        return false
    end

    local mesh = nil
    if owner.GetSkeletalMeshComponent then
        mesh = owner:GetSkeletalMeshComponent()
    end
    if not mesh and owner.GetComponent then
        mesh = owner:GetComponent("SkeletalMeshComponent")
    end

    if not mesh then
        log("[StateMachineTest] Attach this script to an actor with a SkeletalMeshComponent.")
        return false
    end

    local stateMachine = mesh:CreateAnimationStateMachine()
    if not stateMachine then
        log("[StateMachineTest] Failed to create AnimationStateMachine.")
        return false
    end

    for stateName, path in pairs(AnimPaths) do
        stateMachine:AddStateFromPath(stateName, path)
    end

    local entryState = self.Properties.EntryState or "Idle"
    stateMachine:SetEntryStateByName(entryState)

    self.Mesh = mesh
    self.StateMachine = stateMachine
    self.CurrentIndex = self:FindStateIndex(entryState)
    self.bInitialized = true

    log("[StateMachineTest] StateMachine ready. CurrentState=" .. stateMachine:GetCurrentStateName())
    return true
end

function StateMachineTest:Tick(deltaTime)
    if not self.bInitialized and not self:InitializeStateMachine() then
        return
    end

    if self.Properties.AutoCycle == false then
        return
    end

    self.Elapsed = self.Elapsed + deltaTime
    local interval = self.Properties.CycleInterval or 2.0
    if self.Elapsed >= interval then
        self.Elapsed = 0.0
        self:CycleNextState()
    end
end

function StateMachineTest:CycleNextState()
    self.CurrentIndex = self.CurrentIndex + 1
    if self.CurrentIndex > #CycleStates then
        self.CurrentIndex = 1
    end

    self:SetState(CycleStates[self.CurrentIndex])
end

function StateMachineTest:SetState(stateName)
    if not self.StateMachine then
        return
    end

    self.CurrentIndex = self:FindStateIndex(stateName)
    self.StateMachine:SetStateByName(stateName, self.Properties.BlendTime or 0.2)
    log("[StateMachineTest] SetState=" .. stateName)
end

function StateMachineTest:FindStateIndex(stateName)
    for index, value in ipairs(CycleStates) do
        if value == stateName then
            return index
        end
    end
    return 1
end

return StateMachineTest
