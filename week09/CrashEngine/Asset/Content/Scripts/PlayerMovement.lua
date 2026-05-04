local PlayerMovement = {}

-- 값을 0~1로 보간하는 함수
local function clamp01(value)
    if value < 0.0 then
        return 0.0
    elseif value > 1.0 then
        return 1.0
    end

    return value
end

-- 보간용 alpha값을 dt에 맞춰서 계산하기 ( current = current (target-current)*alpha )
local function interpAlpha(speed, dt)
    return clamp01(1.0 - math.exp(-speed * dt))
end

-- Z값을 뺀 값만 Return
local function normalizePlanar(vector)
    local planar = Vector.new(vector.X, vector.Y, 0.0)

    if planar:LengthSquared() > 0.0001 then
        return planar:Normalized()
    end

    return Vector.new(0.0, 0.0, 0.0)
end

local function clampPlanarMagnitude(vector)
    local planar = Vector.new(vector.X, vector.Y, 0.0)
    if planar:LengthSquared() > 1.0 then
        return planar:Normalized()
    end

    return planar
end

-- 두 각 사이의 가장 작은 차이를 구하기
local function shortestAngleDelta(fromDegrees, toDegrees)
    local delta = (toDegrees - fromDegrees + 180.0) % 360.0 - 180.0
    return delta
end

function PlayerMovement.CreateState(options)
    options = options or {}
    --Forward의 z값제외
    local forward = normalizePlanar(options.forward or Vector.new(1.0, 0.0, 0.0))
    -- Right의 z값제외
    local right = normalizePlanar(options.right or Vector.new(-forward.Y, forward.X, 0.0))

    local yaw = PlayerMovement.ForwardToYaw(forward)

    return {
        moveForward = forward, -- W / S 카메라 기준 앞/뒤
        moveRight = right, -- A / D 카메라 기준 왼/오
        visualForward = forward, -- Mesh가 현재 바라보는 방향
        velocity = Vector.new(0.0, 0.0, 0.0), -- 현재 속도
        visualYaw = yaw, -- Mesh의 현재 yaw
        meshYawOffset = options.meshYawOffset or 0.0, --Mesh의 Yaw방향이 다르면 Offset 보정값
        moveSpeed = options.moveSpeed or 10.0, -- 최대 이동속도
        velocityInterpSpeed = options.velocityInterpSpeed or 12.0, -- 가감속 정도
        turnInterpSpeed = options.turnInterpSpeed or 14.0, -- 회전 속도
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

function PlayerMovement.GetDesiredMoveVector(pressedKeys, moveForward, moveRight, analogMove)
    local worldForward = normalizePlanar(moveForward or Vector.new(1.0, 0.0, 0.0))
    local worldRight = normalizePlanar(moveRight or Vector.new(0.0, 1.0, 0.0))
    local moveVector = PlayerMovement.GetDesiredMoveDirection(pressedKeys, worldForward, worldRight)

    if analogMove ~= nil then
        local analogX = analogMove.X or 0.0
        local analogY = analogMove.Y or 0.0
        local analogVector = worldRight * analogX + worldForward * analogY
        moveVector = moveVector + clampPlanarMagnitude(analogVector)
    end

    return clampPlanarMagnitude(moveVector)
end

function PlayerMovement.GetDesiredVisualForward(currentForward, desiredDirection)
    if desiredDirection ~= nil and desiredDirection:LengthSquared() > 0.0001 then
        return desiredDirection:Normalized()
    end

    return currentForward
end

function PlayerMovement.ForwardToYaw(forward)
    return math.deg(math.atan2(forward.Y, forward.X))
end

function PlayerMovement.Update(state, pressedKeys, dt, analogMove)
    local desiredMove = PlayerMovement.GetDesiredMoveVector(pressedKeys, state.moveForward, state.moveRight, analogMove)
    local desiredVelocity = desiredMove * state.moveSpeed
    local moveAlpha = interpAlpha(state.velocityInterpSpeed, dt)

    state.velocity = state.velocity + (desiredVelocity - state.velocity) * moveAlpha

    state.visualForward = PlayerMovement.GetDesiredVisualForward(state.visualForward, desiredMove)

    local targetYaw = PlayerMovement.ForwardToYaw(state.visualForward)
    local turnAlpha = interpAlpha(state.turnInterpSpeed, dt)
    state.visualYaw = state.visualYaw + shortestAngleDelta(state.visualYaw, targetYaw) * turnAlpha

    return state.velocity * dt
end

function PlayerMovement.MakeYawRotation(state)
    return Vector.new(0.0, 0.0, state.visualYaw + state.meshYawOffset)
end

return PlayerMovement
