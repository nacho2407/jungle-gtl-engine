function BeginPlay()
    obj:SetCarWashStreamVisible(false)
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    local isSpraying = Input.GetKey(Key.Space)
    obj:SetCarWashStreamVisible(isSpraying)

    if isSpraying then
        obj:FireCarWashRay()
    end
end
