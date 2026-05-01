function BeginPlay()
    print([BeginPlay]  .. obj.UUID)
    objPrintLocation()
end

function EndPlay()
    print([EndPlay]  .. obj.UUID)
    objPrintLocation()
end

function OnOverlap(OtherActor)
    OtherActorPrintLocation();
end

function Tick(dt)
    obj.Location = obj.Location + obj.Velocity  dt
    objPrintLocation()
end