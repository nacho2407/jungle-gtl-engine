local ObjRegistry = require("ObjRegistry")

local previousPhase = nil

local function SetDirtyCarVisibleByPhase(phase)
    --obj:SetVisible(phase == ECarGamePhase.CarWash)
end

function BeginPlay()
    ObjRegistry.RegisterDirtyCar(obj)
    SetDirtyCarVisibleByPhase(ECarGamePhase.None)
end

function Tick(dt)
    local gs = GetGameState()
    if gs == nil then return false end

    local phase = gs:GetPhase()
    if previousPhase ~= phase then
        previousPhase = phase
        SetDirtyCarVisibleByPhase(phase)
    end
end
