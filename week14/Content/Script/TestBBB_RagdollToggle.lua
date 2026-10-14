local mesh = nil
local ragdollEnabled = false
local wasRagdollKeyDown = false
local printedFirstTick = false

local RAGDOLL_KEY = Key.R
local FULL_PHYSICS_WEIGHT = 1.0
local MOVE_SPEED = 2.5

local function apply_ragdoll_state()
    if mesh == nil then
        return false
    end

    local ok, err = pcall(function()
        mesh:SetPhysicsBlendWeight(FULL_PHYSICS_WEIGHT)
        mesh:SetSimulatePhysics(ragdollEnabled)
    end)

    if not ok then
        print("[RagdollTest] Failed to call ragdoll API: " .. tostring(err))
        return false
    end

    if ragdollEnabled and not mesh:IsSimulatingPhysics() then
        print("[RagdollTest] Ragdoll request was not activated")
        return false
    end

    return true
end

local function get_move_input()
    local input = Vector.Zero()

    if Input.GetKey(Key.W) then
        input = input + Vector.new(1, 0, 0)
    end
    if Input.GetKey(Key.S) then
        input = input + Vector.new(-1, 0, 0)
    end
    if Input.GetKey(Key.D) then
        input = input + Vector.new(0, 1, 0)
    end
    if Input.GetKey(Key.A) then
        input = input + Vector.new(0, -1, 0)
    end

    if input:Length() > 0.0 then
        input:Normalize()
    end

    return input
end

local function tick_walk_movement(dt)
    if ragdollEnabled then
        return
    end

    local input = get_move_input()
    if input:Length() <= 0.0 then
        return
    end

    obj:AddWorldOffset(input * MOVE_SPEED * dt)
end

local function set_ragdoll_enabled(enabled)
    if mesh == nil then
        return
    end

    local previousState = ragdollEnabled
    ragdollEnabled = enabled

    if not apply_ragdoll_state() then
        ragdollEnabled = previousState
        return
    end

    if ragdollEnabled then
        print("[RagdollTest] Ragdoll ON")
    else
        print("[RagdollTest] Ragdoll OFF")
    end
end

function BeginPlay()
    mesh = obj:GetSkeletalMesh()
    ragdollEnabled = false
    wasRagdollKeyDown = false
    printedFirstTick = false

    if mesh ~= nil then
        print("[RagdollTest] BeginPlay: skeletal mesh found")
        apply_ragdoll_state()
    else
        print("[RagdollTest] BeginPlay: skeletal mesh not found")
    end

    print("[RagdollTest] Press R to toggle ragdoll")
end

function EndPlay()
    if mesh ~= nil then
        ragdollEnabled = false
        apply_ragdoll_state()
    end
end

function Tick(dt)
    if mesh == nil then
        return
    end

    if not printedFirstTick then
        printedFirstTick = true
        print("[RagdollTest] Tick active")
    end

    tick_walk_movement(dt)

    local isRagdollKeyDown = Input.GetKey(RAGDOLL_KEY)
    if isRagdollKeyDown and not wasRagdollKeyDown then
        print("[RagdollTest] R pressed")
        set_ragdoll_enabled(not ragdollEnabled)
    end
    wasRagdollKeyDown = isRagdollKeyDown
end
