function BeginPlay()
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    if Input.GetKey(Key.Space) then
        obj:FireCarWashRay()
    end
end
