local movement = nil

function BeginPlay()
    movement = obj:GetCarMovement()
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    local throttle = 0
    if Input.GetKey(Key.W) then throttle = throttle + 1 end
    if Input.GetKey(Key.S) then throttle = throttle - 1 end

    local steering = 0
    if Input.GetKey(Key.A) then steering = steering - 1 end
    if Input.GetKey(Key.D) then steering = steering + 1 end

    movement:SetThrottleInput(throttle)
    movement:SetSteeringInput(steering)
end

