local ObjRegistry = require("ObjRegistry")

local car = nil
local movement = nil

function BeginPlay()
    car = obj:AsCarPawn()
    if car == nil then
        return
    end

    ObjRegistry.RegisterCar(car)
    movement = car:GetCarMovement()
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    if car == nil or movement == nil then
        return
    end

    local gs = GetGameState()
    if gs == nil then return false end

    if gs:GetPhase() == ECarGamePhase.CarWash or gs:GetPhase() == ECarGamePhase.CarGas then
        movement:StopImmediately()
        movement:SetThrottleInput(0)
        movement:SetSteeringInput(0)
        return
    end

    local throttle = 0
    if Input.GetKey(Key.W) then throttle = throttle + 1 end
    if Input.GetKey(Key.S) then throttle = throttle - 1 end

    local steering = 0
    if Input.GetKey(Key.A) then steering = steering - 1 end
    if Input.GetKey(Key.D) then steering = steering + 1 end

    movement:SetThrottleInput(throttle)
    movement:SetSteeringInput(steering)
end

