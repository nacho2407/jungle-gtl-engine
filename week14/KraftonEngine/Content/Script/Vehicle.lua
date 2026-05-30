local vehicle = nil

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

    if Input.GetKey(Key.W) then
        throttle = 1.0
    end

    if Input.GetKey(Key.S) then
        brake = 1.0
    end

    if Input.GetKey(Key.A) then
        steer = steer - 1.0
    end

    if Input.GetKey(Key.D) then
        steer = steer + 1.0
    end

    vehicle:SetDriveInput(throttle, brake, steer)
end
