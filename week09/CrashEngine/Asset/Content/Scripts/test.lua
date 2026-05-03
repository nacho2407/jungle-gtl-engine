local moveSpeed = 10.0
local pressedKeys = {}

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
    local forward = getSafeDirection(World.GetActiveCameraForward())
    local right = getSafeDirection(World.GetActiveCameraRight())
    local up = getSafeDirection(World.GetActiveCameraUp())
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
    if pressedKeys.E or pressedKeys.Space then
        direction = direction + up
    end
    if pressedKeys.Q or pressedKeys.Shift or pressedKeys.Ctrl then
        direction = direction - up
    end

    if direction:LengthSquared() > 0.0 then
        return direction:Normalized()
    end

    return Vector.new(0.0, 0.0, 0.0)
end

local function updateVelocity()
    obj.Velocity = getCameraMoveDirection() * moveSpeed
end

function OnOverlapBegin(other)
    print("Lua OnOverlapBegin", other.UUID);
    obj:ApplyDamage(10.0, obj);
end

function OnOverlapEnd(other)
    print("Lua OnOverlapEnd", other.UUID);
end

function OnTakeDamage(damage, instigator)
    print("Lua OnTakeDamage", damage, instigator.UUID);
end

function BeginPlay()
    pressedKeys = {}
    obj.Velocity = Vector.new(0.0, 0.0, 0.0)
end

function Tick(dt)
    updateVelocity()

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
