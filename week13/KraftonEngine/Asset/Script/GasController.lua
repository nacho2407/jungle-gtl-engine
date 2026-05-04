local ObjRegistry = require("ObjRegistry")
local UIManager = require("UIManager")

local GAS_CONSUMPTION_PER_SPEED = 0.02
local MIN_CONSUME_SPEED = 0.1

local car = nil
local movement = nil
local gas = nil
local fuelOutGameOverStarted = false

local function StopCar()
    if movement == nil then
        return
    end

    movement:StopImmediately()
    movement:SetThrottleInput(0)
    movement:SetSteeringInput(0)
end

local function TriggerFuelOutGameOver()
    StopCar()

    if fuelOutGameOverStarted then
        return
    end
    fuelOutGameOverStarted = true

    UIManager.ShowCriticalWarning(
        "FUEL EMPTY",
        "연료가 모두 떨어져 차량이 멈췄습니다.",
        2.0,
        function()
            local gm = GetGameMode()
            if gm ~= nil then
                gm:GameOver()
            end
        end)
end

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
    fuelOutGameOverStarted = false
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
        TriggerFuelOutGameOver()
        return
    end

    local speed = math.abs(movement:GetForwardSpeed())
    if speed < MIN_CONSUME_SPEED then
        return
    end

    local consumed = gas:ConsumeGas(speed * GAS_CONSUMPTION_PER_SPEED * dt)
    if not consumed then
        TriggerFuelOutGameOver()
    end
end
