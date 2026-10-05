local TopDownSupport = require("TopDownSupport")

local TopDownCharacterMotor = {}

function TopDownCharacterMotor.MakeMoveDirection()
    local x = 0.0
    local y = 0.0

    -- 월드 기준 이동.
    -- Z-up / X-forward / Y-right 기준:
    -- Up    = +X
    -- Down  = -X
    -- Right = +Y
    -- Left  = -Y
    if TopDownSupport.IsKeyDown("Up") or TopDownSupport.IsKeyDown("UpArrow") then
        x = x + 1.0
    end

    if TopDownSupport.IsKeyDown("Down") or TopDownSupport.IsKeyDown("DownArrow") then
        x = x - 1.0
    end

    if TopDownSupport.IsKeyDown("Right") or TopDownSupport.IsKeyDown("RightArrow") then
        y = y + 1.0
    end

    if TopDownSupport.IsKeyDown("Left") or TopDownSupport.IsKeyDown("LeftArrow") then
        y = y - 1.0
    end

    local dir = Vector(x, y, 0.0)

    if dir:SizeSquared2D() > 0.0001 then
        return dir:GetSafeNormal2D()
    end

    return Vector(0.0, 0.0, 0.0)
end

function TopDownCharacterMotor.Move(actor, dir, dt, config)
    if not actor then
        return
    end

    config = config or {}

    local speed = config.MoveSpeed or 3.0
    local delta = dir * speed * dt

    if actor.Add_Actor_World_Offset then
        local ok = pcall(function()
            actor:Add_Actor_World_Offset(delta)
        end)

        if not ok then
            actor.Location = actor.Location + delta
        end
    else
        actor.Location = actor.Location + delta
    end
end

function TopDownCharacterMotor.RotateToDirection(actor, dir, dt, config)
    if not actor then
        return
    end

    if not dir or dir:SizeSquared2D() <= 0.0001 then
        return
    end

    config = config or {}

    local actorYawOffset = config.ActorYawOffset or 0.0
    local interpSpeed = config.RotationInterpSpeed or 12.0

    -- X-forward 기준:
    -- dir = (1, 0, 0)  -> yaw 0도
    -- dir = (0, 1, 0)  -> yaw 90도
    local targetYaw = TopDownSupport.YawFromDirection2D(dir) + actorYawOffset

    local currentYaw = targetYaw
    if actor.Rotation then
        currentYaw = actor.Rotation.Z or targetYaw
    end

    local newYaw = TopDownSupport.InterpAngleDegrees(
        currentYaw,
        targetYaw,
        dt,
        interpSpeed
    )

    pcall(function()
        actor.Rotation = Vector(0.0, 0.0, newYaw)
    end)
end

function TopDownCharacterMotor.MoveAndRotate(actor, dir, dt, config)
    TopDownCharacterMotor.Move(actor, dir, dt, config)
    TopDownCharacterMotor.RotateToDirection(actor, dir, dt, config)
end

function TopDownCharacterMotor.SetAnimMoveParam(mesh, paramName, isMoving)
    if not mesh then
        return
    end

    if not mesh.SetAnimGraphBool then
        return
    end

    pcall(function()
        mesh:SetAnimGraphBool(paramName, isMoving)
    end)
end

return TopDownCharacterMotor