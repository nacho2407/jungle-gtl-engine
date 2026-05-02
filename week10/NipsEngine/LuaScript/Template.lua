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
    --Log("[OnOverlap]"  .. Actor.UUID)
end
