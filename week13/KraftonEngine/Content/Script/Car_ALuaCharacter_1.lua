local VK_LEFT = 0x25
local VK_UP = 0x26
local VK_RIGHT = 0x27
local VK_DOWN = 0x28
local VK_SPACE = 0x20

local character = nil
local bound = false
local ragdolled = false

local function bind_axis(input, name, key_positive, key_negative, callback)
    input:AddAxisMapping(name, key_positive, 1.0)
    input:AddAxisMapping(name, key_negative, -1.0)
    input:BindAxis(name, function(value)
        if value ~= 0 then
            callback(value)
        end
    end)
end

local function try_bind_input()
    if ragdolled then
        return
    end

    if bound then
        return
    end

    if character == nil then
        character = obj:AsCharacter()
        if character == nil then
            return
        end
    end

    local input = character:GetInputComponent()
    if input == nil then
        return
    end

    input:ClearBindings()

    bind_axis(input, "ArrowMoveForward", VK_UP, VK_DOWN, function(value)
        character:AddMovementInput(obj.Forward, value)
    end)

    bind_axis(input, "ArrowMoveRight", VK_RIGHT, VK_LEFT, function(value)
        character:AddMovementInput(obj.Right, value)
    end)

    input:AddActionMapping("ArrowJump", VK_SPACE)
    input:BindAction("ArrowJump", "Pressed", function()
        character:Jump()
    end)

    bound = true
end

function BeginPlay()
    try_bind_input()
end

function Tick(delta_time)
    try_bind_input()
end

function OnHit(other_actor, hit_component, other_comp, normal_impulse, hit_result)
    if ragdolled then
        return
    end

    if other_actor == nil or other_actor:GetName() ~= "A4WVehicleActor_1" then
        return
    end

    if character == nil then
        character = obj:AsCharacter()
    end

    if character == nil then
        return
    end

    ragdolled = true
    bound = true
    character:EnterFullRagdoll()
end
