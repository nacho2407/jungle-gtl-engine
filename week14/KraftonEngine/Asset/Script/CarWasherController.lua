local ObjRegistry = require("ObjRegistry")
local bCarWashSucceeded = false
local previousPhase = nil
local gunForwardOffset = 0.5
local gunRightOffset = 0.2

local function UpdateCarWasherTransform()
    local man = ObjRegistry.manObj
    local manCamera = ObjRegistry.manCamera
    if man == nil or manCamera == nil then
        return
    end

    obj.Location = man.Location + manCamera.Forward * gunForwardOffset + manCamera.Right * gunRightOffset
    obj.Rotation = manCamera.Rotation
end

function BeginPlay()
    ObjRegistry.RegisterCarWasher(obj)
    obj:SetVisible(false)
    obj:SetCarWashStreamVisible(false)
end

function Tick(dt)
    local gs = GetGameState()
    if gs == nil then return false end

    local phase = gs:GetPhase()
    local bIsCarWashPhase = phase == ECarGamePhase.CarWash

    if previousPhase ~= phase then
        previousPhase = phase
        if bIsCarWashPhase then
            bCarWashSucceeded = false
            obj:SetVisible(true)
        else
            obj:SetVisible(false)
            obj:SetCarWashStreamVisible(false)
        end
    end

    if bIsCarWashPhase then
        UpdateCarWasherTransform()

        local isSpraying = Input.GetKey(Key.Space)
        obj:SetCarWashStreamVisible(isSpraying)

        if obj:IsCarWashStreamVisible() then
            obj:FireCarWashRay()
        end

        local car = ObjRegistry.dirtyCar
        if not bCarWashSucceeded and car ~= nil and car:AreAllDirtComponentsWashed() then
            bCarWashSucceeded = true
            obj:SetCarWashStreamVisible(false)
            local gameMode = GetGameMode()
            if gameMode ~= nil then
                gameMode:SuccessPhase()
            end
        end
    else
        obj:SetCarWashStreamVisible(false)
    end
end
