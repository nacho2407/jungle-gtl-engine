local ribbons = {}
local isVisible = nil

local function SetRibbonVisibility(visible)
    if isVisible == visible then
        return
    end

    isVisible = visible
    for _, component in ipairs(ribbons) do
        component:SetVisibility(visible)
    end
end

function BeginPlay()
    for _, component in ipairs(obj:GetPrimitiveComponents()) do
        if component:GetClassName() == "UParticleSystemComponent" then
            table.insert(ribbons, component)
        end
    end

    SetRibbonVisibility(false)
end

function Tick(dt)
    SetRibbonVisibility(Input.GetKey(Key.Shift) or Input.GetKey(Key.Space))
end
