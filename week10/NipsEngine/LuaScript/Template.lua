function BeginPlay()
    Log("[BeginPlay]"  .. Actor.UUID)
end

function EndPlay()
    Log("[EndPlay]"  .. Actor.UUID)
end

function OnOverlap(OtherActor)
    Log("[OnOverlap]"  .. Actor.UUID)
end

function Tick(dt)
    --Log("[OnOverlap]"  .. Actor.UUID)
end
