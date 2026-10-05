local AnimGraphStateMachineParamTest = {}
AnimGraphStateMachineParamTest.__index = AnimGraphStateMachineParamTest

AnimGraphStateMachineParamTest.Properties = {
    AnimGraphPath = {
        Type = "String",
        Default = "Asset/Animation Graph/AnimGraph_StateMachine_LuaTest.animgraph",
        Category = "AnimGraph Test"
    },
    CycleInterval = { Type = "Float", Default = 2.0, Min = 0.1, Category = "AnimGraph Test" },
    IdleSpeed = { Type = "Float", Default = 0.0, Category = "AnimGraph Test" },
    RunSpeed = { Type = "Float", Default = 1.0, Category = "AnimGraph Test" },
}

local function log(message)
    if Engine and Engine.API and Engine.API.Debug then
        Engine.API.Debug.Log(message)
    elseif Log then
        Log(message)
    end
end

function AnimGraphStateMachineParamTest.new(scriptComponent, properties)
    local self = setmetatable({}, AnimGraphStateMachineParamTest)

    self.Component = scriptComponent
    self.Actor = scriptComponent and scriptComponent:GetOwner() or Actor
    self.Properties = properties or {}
    self.Mesh = nil
    self.Elapsed = 0.0
    self.bRunning = false
    self.bInitialized = false

    return self
end

function AnimGraphStateMachineParamTest:BeginPlay()
    self:InitializeAnimGraph()
end

function AnimGraphStateMachineParamTest:InitializeAnimGraph()
    if self.bInitialized then
        return true
    end

    local owner = self.Actor or Actor or Owner
    if not owner then
        log("[AnimGraphParamTest] Owner actor is missing.")
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
        log("[AnimGraphParamTest] Attach this script to an actor with a SkeletalMeshComponent.")
        return false
    end

    local graphPath = self.Properties.AnimGraphPath
        or "Asset/Animation Graph/AnimGraph_StateMachine_LuaTest.animgraph"

    mesh:SetAnimGraphAssetPath(graphPath)
    mesh:SetAnimGraphFloat("Speed", self.Properties.IdleSpeed or 0.0)
    mesh:SetAnimGraphBool("IsRunning", false)

    self.Mesh = mesh
    self.bInitialized = true

    log("[AnimGraphParamTest] AnimGraph applied: " .. graphPath)
    return true
end

function AnimGraphStateMachineParamTest:Tick(deltaTime)
    if not self.bInitialized and not self:InitializeAnimGraph() then
        return
    end

    self.Elapsed = self.Elapsed + deltaTime

    local interval = self.Properties.CycleInterval or 2.0
    if self.Elapsed < interval then
        return
    end

    self.Elapsed = 0.0
    self.bRunning = not self.bRunning

    local speed = self.bRunning
        and (self.Properties.RunSpeed or 1.0)
        or (self.Properties.IdleSpeed or 0.0)

    self.Mesh:SetAnimGraphFloat("Speed", speed)
    self.Mesh:SetAnimGraphBool("IsRunning", self.bRunning)

    log("[AnimGraphParamTest] Speed=" .. tostring(speed) .. ", IsRunning=" .. tostring(self.bRunning))
end

return AnimGraphStateMachineParamTest
