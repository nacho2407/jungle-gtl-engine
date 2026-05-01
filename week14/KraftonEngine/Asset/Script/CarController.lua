local car = {
    speed = 0,
    slideSpeed = 0,
    steerAngle = 0,

    maxSpeed = 20,
    reverseMaxSpeed = -15,

    accelPower = 15,
    reverseAccelPower = 10,
    brakePower = 40,
    friction = 5,
    lateralGrip = 10
}

local function lerp(a, b, t)
    return a + (b - a) * t
end

local function Move(dt)
    local moveInput = 0
    if Input.GetKey(Key.W) then moveInput = moveInput + 1 end
    if Input.GetKey(Key.S) then moveInput = moveInput - 1 end

    if moveInput > 0 then
        if car.speed < 0 then
            car.speed = car.speed + car.brakePower * dt
        else
            car.speed = car.speed + car.accelPower * dt
        end
    elseif moveInput < -0.1 then
        if car.speed > 0 then
            car.speed = car.speed - car.brakePower * dt
        else
            car.speed = car.speed - car.reverseAccelPower * dt
        end
    else
        if car.speed > 0.1 then
            car.speed = math.max(0, car.speed - car.friction * dt)
        elseif car.speed < -0.1 then
            car.speed = math.min(0, car.speed + car.friction * dt)
        else
            car.speed = 0
        end
    end

    car.speed = math.max(car.reverseMaxSpeed, math.min(car.maxSpeed, car.speed))
end

local function Steer(dt)
    local steerInput = 0
    if Input.GetKey(Key.A) then steerInput = steerInput - 1 end
    if Input.GetKey(Key.D) then steerInput = steerInput + 1 end

    local targetAngle = steerInput * 45
    car.steerAngle = lerp(car.steerAngle, targetAngle, dt * 5)

    local turnSpeed = car.steerAngle * (math.abs(car.speed) / 7.5)
    
    local rotation = obj.Rotation
    rotation.Z = rotation.Z + turnSpeed * dt
    obj.Rotation = rotation
end

local function UpdateLocation(dt)
    local forward = obj.Forward
    local right = obj.Right

    local lateralForce = car.steerAngle * car.speed * dt * 0.1
    car.slideSpeed = car.slideSpeed + lateralForce

    car.slideSpeed = lerp(car.slideSpeed, 0, dt * car.lateralGrip)

    local finalMove = forward * car.speed + right * car.slideSpeed
    obj:AddWorldOffset(finalMove * dt)
end

function BeginPlay()
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    Move(dt)
    Steer(dt)
    UpdateLocation(dt)
end

