local PlayerMovement = {}

local function clamp01(value)
    if value < 0.0 then
        return 0.0
    elseif value > 1.0 then
        return 1.0
    end

    return value
end

local function interpAlpha(speed, dt)
    return clamp01(1.0 - math.exp(-speed * dt))
end

local function normalizePlanar(vector)
    local planar = Vector.new(vector.X, vector.Y, 0.0)

    if planar:LengthSquared() > 0.0001 then
        return planar:Normalized()
    end

    return Vector.new(0.0, 0.0, 0.0)
end

local function shortestAngleDelta(fromDegrees, toDegrees)
    local delta = (toDegrees - fromDegrees + 180.0) % 360.0 - 180.0
    return delta
end

function PlayerMovement.CreateState(options)
    options = options or {}

    local forward = normalizePlanar(options.forward or Vector.new(1.0, 0.0, 0.0))
    local right = normalizePlanar(options.right or Vector.new(-forward.Y, forward.X, 0.0))
    local yaw = PlayerMovement.ForwardToYaw(forward)

    return {
        moveForward = forward,
        moveRight = right,
        visualForward = forward,
        velocity = Vector.new(0.0, 0.0, 0.0),
        visualYaw = yaw,
        meshYawOffset = options.meshYawOffset or 0.0,
        meshBaseRotation = options.meshBaseRotation or Vector.new(0.0, 0.0, 0.0),
        moveSpeed = options.moveSpeed or 10.0,
        velocityInterpSpeed = options.velocityInterpSpeed or 12.0,
        turnInterpSpeed = options.turnInterpSpeed or 14.0,
    }
end

function PlayerMovement.GetDesiredMoveDirection(pressedKeys, moveForward, moveRight)
    local worldForward = normalizePlanar(moveForward or Vector.new(1.0, 0.0, 0.0))
    local worldRight = normalizePlanar(moveRight or Vector.new(0.0, 1.0, 0.0))
    local direction = Vector.new(0.0, 0.0, 0.0)

    if pressedKeys.W or pressedKeys.Up then
        direction = direction + worldForward
    end
    if pressedKeys.S or pressedKeys.Down then
        direction = direction - worldForward
    end
    if pressedKeys.D or pressedKeys.Right then
        direction = direction + worldRight
    end
    if pressedKeys.A or pressedKeys.Left then
        direction = direction - worldRight
    end

    return normalizePlanar(direction)
end

function PlayerMovement.GetDesiredVisualForward(currentForward, desiredDirection)
    if desiredDirection ~= nil and desiredDirection:LengthSquared() > 0.0001 then
        return desiredDirection
    end

    return currentForward
end

function PlayerMovement.ForwardToYaw(forward)
    return math.deg(math.atan2(forward.Y, forward.X))
end

function PlayerMovement.Update(state, pressedKeys, dt)
    local desiredDirection = PlayerMovement.GetDesiredMoveDirection(pressedKeys, state.moveForward, state.moveRight)
    local desiredVelocity = desiredDirection * state.moveSpeed
    local moveAlpha = interpAlpha(state.velocityInterpSpeed, dt)

    state.velocity = state.velocity + (desiredVelocity - state.velocity) * moveAlpha

    state.visualForward = PlayerMovement.GetDesiredVisualForward(state.visualForward, desiredDirection)

    local targetYaw = PlayerMovement.ForwardToYaw(state.visualForward)
    local turnAlpha = interpAlpha(state.turnInterpSpeed, dt)
    state.visualYaw = state.visualYaw + shortestAngleDelta(state.visualYaw, targetYaw) * turnAlpha

    return state.velocity * dt
end

function PlayerMovement.MakeYawRotation(state)
    return Vector.new(
        state.meshBaseRotation.X,
        state.meshBaseRotation.Y,
        state.meshBaseRotation.Z + state.visualYaw + state.meshYawOffset
    )
end

return PlayerMovement
