local LifeTime = 1.0
local SubUVComponent = nil

local function ensureSubUVComponent()
    if SubUVComponent ~= nil and SubUVComponent:IsValid() then
        return SubUVComponent
    end

    SubUVComponent = obj:GetComponent("SubUVComponent", 0)
    if SubUVComponent ~= nil and SubUVComponent:IsValid() then
        SubUVComponent:SetCastShadow(false)
    end
    return SubUVComponent
end

function BeginPlay()
    ensureSubUVComponent()
end

function Tick(dt)
    LifeTime = LifeTime - dt
    if LifeTime <= 0.0 then
        World.DestroyActor(obj)
    end
end

function SetEffectLifeTime(lifeTime)
    LifeTime = lifeTime
end

function SetLifeTime(lifeTime)
    SetEffectLifeTime(lifeTime)
end

function SetLocation(location)
    obj.Location = location
end

function SetMaterial(materialPath)
    local comp = ensureSubUVComponent()
    if comp ~= nil and comp:IsValid() then
        comp:SetMaterial(0, materialPath)
    else
    end
end

function SetRowColumn(row, column)
    local comp = ensureSubUVComponent()
    if comp ~= nil and comp:IsValid() then
        comp:SetSubUVGrid(row, column)
    else
    end
end

function SetLowColoum(row, column)
    SetRowColumn(row, column)
end

function SetLowColumn(row, column)
    SetRowColumn(row, column)
end

function InitEffect(location, lifeTime, materialPath, row, column)
    SetLocation(location)
    SetEffectLifeTime(lifeTime)
    SetMaterial(materialPath)
    SetRowColumn(row, column)
end
