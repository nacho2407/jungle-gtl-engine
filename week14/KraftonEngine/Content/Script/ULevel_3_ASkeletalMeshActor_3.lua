local mesh = nil
local sword = nil

function BeginPlay()
    mesh = obj:GetSkeletalMesh()
    sword = obj:GetComponentByName("UStaticMeshComponent_9")
end

function EndPlay()
    print("[EndPlay] " .. obj.UUID)
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    if mesh == nil or sword == nil then
        return
    end

    local pos = mesh:GetBoneSocketLocation("Bip001 R Hand", Vector.new(0, 0, 0))
    local rot = mesh:GetBoneSocketRotation("Bip001 R Hand", Vector.new(0, 90, 0))

    sword:SetLocation(pos)
    sword:SetRotation(rot)
end
