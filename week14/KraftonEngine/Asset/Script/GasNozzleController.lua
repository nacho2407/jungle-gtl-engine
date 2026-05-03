local ObjRegistry = require("ObjRegistry")
local bIsOverlapping = false

function BeginPlay()
    ObjRegistry.RegisterGasNozzle(obj)
end

function EndPlay()
end

function OnOverlap(OtherActor)
    if ObjRegistry.car ~= nil and OtherActor.UUID == ObjRegistry.car.UUID then
        bIsOverlapping = true
    end
end

function OnEndOverlap(OtherActor)
    if ObjRegistry.car ~= nil and OtherActor.UUID == ObjRegistry.car.UUID then
        bIsOverlapping = false
    end
end

function Tick(dt)
    local gs = GetGameState()
    if gs == nil then return false end

    if gs:GetPhase() == ECarGamePhase.CarGas then
        local man = ObjRegistry.manObj
        local manCamera = ObjRegistry.manCamera
        if man ~= nil and manCamera ~= nil then
            obj.Location = man.Location + manCamera.Forward * 0.5 + manCamera.Right * 0.2
            obj.Rotation = manCamera.Rotation
        end

        if bIsOverlapping and ObjRegistry.carGas ~= nil then
            ObjRegistry.carGas:AddGas(5.0 * dt)

            if ObjRegistry.carGas:GetGas() >= 100 then
                GetGameMode():SuccessPhase()
            end
        end
    end
end
