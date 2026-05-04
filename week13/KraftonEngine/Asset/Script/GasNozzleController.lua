local ObjRegistry = require("ObjRegistry")
local UIManager = require("UIManager")
local bIsOverlapping = false
local previousPhase = nil

function BeginPlay()
    ObjRegistry.RegisterGasNozzle(obj)
    obj:SetVisible(false)
end

function EndPlay()
end

function OnOverlap(OtherActor)
    if ObjRegistry.car ~= nil and OtherActor.UUID == ObjRegistry.car.UUID then
        bIsOverlapping = true
        UIManager.SetGasFeedbackActive(true)
    end
end

function OnEndOverlap(OtherActor)
    if ObjRegistry.car ~= nil and OtherActor.UUID == ObjRegistry.car.UUID then
        bIsOverlapping = false
        UIManager.SetGasFeedbackActive(false)
    end
end

function Tick(dt)
    local gs = GetGameState()
    if gs == nil then return false end

    local phase = gs:GetPhase()
    local bIsCarGasPhase = phase == ECarGamePhase.CarGas

    if previousPhase ~= phase then
        previousPhase = phase
        obj:SetVisible(bIsCarGasPhase)

        if not bIsCarGasPhase then
            bIsOverlapping = false
            UIManager.SetGasFeedbackActive(false)
        end
    end

    if bIsCarGasPhase then
        local man = ObjRegistry.manObj
        local manCamera = ObjRegistry.manCamera
        if man ~= nil and manCamera ~= nil then
            obj.Location = man.Location + manCamera.Forward * 1.0 + manCamera.Right * 0.2
            obj.Rotation = manCamera.Rotation
        end

        UIManager.SetGasFeedbackActive(bIsOverlapping)

        if bIsOverlapping and ObjRegistry.car ~= nil then
            ObjRegistry.car:GetCarGas():AddGas(5.0 * dt)

            if ObjRegistry.car:GetCarGas():GetGas() >= 100 then
                AudioManager.Play("Complete", 1.0)
                GetGameMode():SuccessPhase()
            end
        end
    else
        UIManager.SetGasFeedbackActive(false)
    end
end
