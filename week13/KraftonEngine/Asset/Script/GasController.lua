local ObjRegistry = require("ObjRegistry")

local GAS_CONSUMPTION_PER_SPEED = 0.02
local MIN_CONSUME_SPEED = 0.1

local car = nil
local movement = nil
local gas = nil

local function CacheCarComponents()
    if car == nil then
        car = obj:AsCarPawn()
    end

    if car == nil then
        car = ObjRegistry.car
    end

    if car == nil then
        return false
    end

    if movement == nil then
        movement = car:GetCarMovement()
    end

    if gas == nil then
        gas = car:GetCarGas()
    end

    return movement ~= nil and gas ~= nil
end

function BeginPlay()
    CacheCarComponents()
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    if dt == nil or dt <= 0 then
        return
    end

    if not CacheCarComponents() then
        return
    end

    if not gas:HasGas() then
        movement:StopImmediately()
        movement:SetThrottleInput(0)
        movement:SetSteeringInput(0)
        return
    end

    local speed = math.abs(movement:GetForwardSpeed())
    if speed < MIN_CONSUME_SPEED then
        return
    end

    local consumed = gas:ConsumeGas(speed * GAS_CONSUMPTION_PER_SPEED * dt)
    if not consumed then
        movement:StopImmediately()
        movement:SetThrottleInput(0)
        movement:SetSteeringInput(0)
    end
end
