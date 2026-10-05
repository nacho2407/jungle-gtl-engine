local TopDownSupport = {}

function TopDownSupport.Log(message)
    if Engine and Engine.API and Engine.API.Debug and Engine.API.Debug.Log then
        Engine.API.Debug.Log(message)
    elseif Log then
        Log(message)
    end
end

function TopDownSupport.VectorToString(v)
    if not v then
        return "nil"
    end

    return string.format(
        "(X=%.3f, Y=%.3f, Z=%.3f)",
        v.X or 0.0,
        v.Y or 0.0,
        v.Z or 0.0
    )
end

function TopDownSupport.GetDeltaTime(fallback)
    if Engine and Engine.API and Engine.API.World and Engine.API.World.GetDeltaTime then
        local ok, dt = pcall(function()
            return Engine.API.World.GetDeltaTime()
        end)

        if ok and dt and dt > 0.0 then
            return dt
        end
    end

    if fallback and fallback > 0.0 then
        return fallback
    end

    return 0.016
end

function TopDownSupport.IsKeyDown(key)
    local input = Engine and Engine.API and Engine.API.Input
    if not input then
        return false
    end

    if input.IsRawKeyDown then
        local ok, value = pcall(function()
            return input.IsRawKeyDown(key)
        end)

        if ok then
            return value == true
        end
    end

    if input.IsKeyDown then
        local ok, value = pcall(function()
            return input.IsKeyDown(key)
        end)

        if ok then
            return value == true
        end
    end

    return false
end

function TopDownSupport.GetPlayerController()
    local world = Engine and Engine.API and Engine.API.World
    if not world then
        return nil
    end

    if world.GetPlayerController then
        local ok, controller = pcall(function()
            return world.GetPlayerController()
        end)

        if ok then
            return controller
        end
    end

    return nil
end

function TopDownSupport.GetPossessedPawn()
    local world = Engine and Engine.API and Engine.API.World

    if world and world.GetPossessedActor then
        local ok, pawn = pcall(function()
            return world.GetPossessedActor()
        end)

        if ok and pawn then
            return pawn
        end
    end

    local controller = TopDownSupport.GetPlayerController()
    if not controller then
        return nil
    end

    if controller.GetPossessedActor then
        local ok, pawn = pcall(function()
            return controller:GetPossessedActor()
        end)

        if ok and pawn then
            return pawn
        end
    end

    if controller.GetPawn then
        local ok, pawn = pcall(function()
            return controller:GetPawn()
        end)

        if ok and pawn then
            return pawn
        end
    end

    return nil
end

function TopDownSupport.FindSkeletalMesh(owner)
    if not owner then
        return nil
    end

    if owner.GetSkeletalMeshComponent then
        local ok, result = pcall(function()
            return owner:GetSkeletalMeshComponent()
        end)

        if ok and result then
            return result
        end
    end

    if owner.GetComponent then
        local ok, result = pcall(function()
            return owner:GetComponent("SkeletalMeshComponent")
        end)

        if ok and result then
            return result
        end
    end

    return nil
end

function TopDownSupport.Atan2(y, x)
    if math.atan2 then
        return math.atan2(y, x)
    end

    if x > 0.0 then
        return math.atan(y / x)
    end

    if x < 0.0 and y >= 0.0 then
        return math.atan(y / x) + math.pi
    end

    if x < 0.0 and y < 0.0 then
        return math.atan(y / x) - math.pi
    end

    if x == 0.0 and y > 0.0 then
        return math.pi * 0.5
    end

    if x == 0.0 and y < 0.0 then
        return -math.pi * 0.5
    end

    return 0.0
end

function TopDownSupport.Clamp(value, minValue, maxValue)
    if value < minValue then
        return minValue
    end

    if value > maxValue then
        return maxValue
    end

    return value
end

function TopDownSupport.NormalizeAngleDegrees(angle)
    angle = angle % 360.0

    if angle > 180.0 then
        angle = angle - 360.0
    end

    if angle < -180.0 then
        angle = angle + 360.0
    end

    return angle
end

function TopDownSupport.InterpAngleDegrees(current, target, dt, speed)
    if current == nil then
        return target
    end

    if not speed or speed <= 0.0 then
        return target
    end

    local alpha = TopDownSupport.Clamp(dt * speed, 0.0, 1.0)
    local delta = TopDownSupport.NormalizeAngleDegrees(target - current)

    return current + delta * alpha
end

function TopDownSupport.YawFromDirection2D(dir)
    return math.deg(TopDownSupport.Atan2(dir.Y, dir.X))
end

return TopDownSupport