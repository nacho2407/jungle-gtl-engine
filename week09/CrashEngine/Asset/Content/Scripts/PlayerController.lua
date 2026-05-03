local moveSpeed = 10.0
local pressedKeys = {}

local HP = 100.0
local HP_reduction  = 10;

local LightComponet = nil
local Initial_Light_Intensity = 0.0

local DocumentCount = 0



local movementKeys = {
    W = true,
    S = true,
    A = true,
    D = true,
    Up = true,
    Down = true,
    Left = true,
    Right = true,
    Q = true,
    E = true,
    Space = true,
    Shift = true,
    Ctrl = true,
}

local function getSafeDirection(direction)
    if direction:LengthSquared() > 0.0001 then
        return direction:Normalized()
    end

    return Vector.new(0.0, 0.0, 0.0)
end

local function getCameraMoveDirection()
    local forward =  Vector.new(-1.0, 0.0, 0.0)
    local right =  Vector.new(0.0, -1.0, 0.0)
    local direction = Vector.new(0.0, 0.0, 0.0)

    if pressedKeys.W or pressedKeys.Up then
        direction = direction + forward
    end
    if pressedKeys.S or pressedKeys.Down then
        direction = direction - forward
    end
    if pressedKeys.D or pressedKeys.Right then
        direction = direction + right
    end
    if pressedKeys.A or pressedKeys.Left then
        direction = direction - right
    end

    if direction:LengthSquared() > 0.0 then
        return direction:Normalized()
    end

    return Vector.new(0.0, 0.0, 0.0)
end

local function updateVelocity()
    obj.Velocity = getCameraMoveDirection() * moveSpeed
end

function OnOverlapEnd(other)
    print("Lua OnOverlapEnd", other.UUID);
end

function OnOverlapBegin(other)
    print("Lua OnOverlapBegin", other.UUID);
    if other:HasTag("Document") then
        DocumentCount = DocumentCount + 1;
        print("Has Document : ", DocumentCount);
        World.DestroyActor(other)
    elseif other:HasTag("Battery") then
        HP = HP + 10
        print("Player HP : ", HP);
        World.DestroyActor(other)
    elseif other:HasTag("Destination") then
        print("Game Finish!");
    end
end

function BeginPlay()
    pressedKeys = {}
    obj.Velocity = Vector.new(0.0, 0.0, 0.0)

    LightComponet = obj:GetComponent("PointLightComponent", 0)
    LightComponet:SetIntensity(HP)
end

function Tick(dt)
    updateVelocity()

    if(HP > 0) then
        HP = HP - dt * HP_reduction
    else 
        HP = 0
    end

    LightComponet:SetIntensity(HP)

    if obj.Velocity:LengthSquared() > 0.0 then
        obj:AddWorldOffset(obj.Velocity * dt)
    end


end

function EndPlay()
    obj.Velocity = Vector.new(0.0, 0.0, 0.0)
end

function OnKeyPressed(key)
    if movementKeys[key] == nil then
        return
    end

    pressedKeys[key] = true
    updateVelocity()
end

function OnKeyReleased(key)
    if movementKeys[key] == nil then
        return
    end

    pressedKeys[key] = nil
    updateVelocity()
end
