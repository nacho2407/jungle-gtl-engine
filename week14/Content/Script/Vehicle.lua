local vehicle = nil

local currentThrottle = 0.0
local currentBrake = 0.0
local currentSteer = 0.0

local function approach(current, target, speed, dt)
    if current < target then
        return math.min(current + speed * dt, target)
    else
        return math.max(current - speed * dt, target)
    end
end

function BeginPlay()
    vehicle = obj:GetVehicleMovement()
end

function EndPlay()
    print("[EndPlay] " .. obj.UUID)
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    if vehicle == nil then return end

    local throttle = 0.0
    local brake = 0.0
    local steer = 0.0
    local reverse = false

    if Input.GetKey(Key.W) then
        throttle = 1.0
    end

    if Input.GetKey(Key.S) then
        throttle = 1.0
        reverse = true
    end

    if Input.GetKey(Key.Space) then
        brake = 1.0
    end

    if Input.GetKey(Key.A) then
        steer = steer - 1.0
    end

    if Input.GetKey(Key.D) then
        steer = steer + 1.0
    end

    currentThrottle = approach(currentThrottle, throttle, 3.0, dt)
    currentBrake = approach(currentBrake, brake, 8.0, dt)
    currentSteer = approach(currentSteer, steer, 5.0, dt)

    vehicle:SetDriveInput(currentThrottle, currentBrake, currentSteer, reverse)
end
