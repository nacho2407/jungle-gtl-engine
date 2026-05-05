function BeginPlay()
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    obj.Location = obj.Location + obj.Velocity * dt
end
