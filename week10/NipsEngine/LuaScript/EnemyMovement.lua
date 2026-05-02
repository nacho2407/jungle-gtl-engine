local target = Vector(0, 0, 0)
local Speed = 10

function BeginPlay()
    Log("[BeginPlay]"  .. Actor.UUID)
end

function EndPlay()
    Log("[EndPlay]"  .. Actor.UUID)
end

function OnOverlap(OtherActor)
    Log("[OnOverlap]"  .. Actor.UUID)
end

function OnHit(HitComponent, OtherActor, OtherComp, NormalImpulse, Hit)
    Log("[OnHit] " .. Actor.UUID)

    if OtherActor ~= nil then
        Log("OtherActor: " .. OtherActor.Name)
    end
end

function OnBeginOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult)
    Log("[OnBeginOverlap] " .. Actor.UUID)

    if OtherActor ~= nil then
        Log("OtherActor: " .. OtherActor.Name)
    end

    if bFromSweep then
        Log("BeginOverlap from sweep")
    end
end

function OnEndOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult)
    Log("[OnEndOverlap] " .. Actor.UUID)

    if OtherActor ~= nil then
        Log("OtherActor: " .. OtherActor.Name)
    end
end

function Tick(dt)
    local Delta = target - Actor.Location
    local Distance = Delta:Size()

    if Distance > 0.1 then
        local Direction = Delta:Normalized()
        Actor.Location = Actor.Location + Direction * dt * Speed
    end
end
