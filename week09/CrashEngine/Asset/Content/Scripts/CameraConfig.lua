local CameraConfig = {}

CameraConfig.NormalOffset = Vector.new(25.0, 20.0, 50.0)
CameraConfig.PlayerMeshBaseRotation = Vector.new(0.0, 0.0, 0.0)

local function normalizePlanar(vector)
    local planar = Vector.new(vector.X, vector.Y, 0.0)

    if planar:LengthSquared() > 0.0001 then
        return planar:Normalized()
    end

    return Vector.new(1.0, 0.0, 0.0)
end

function CameraConfig.GetMoveForward()
    return normalizePlanar(Vector.new(
        -CameraConfig.NormalOffset.X,
        -CameraConfig.NormalOffset.Y,
        0.0
    ))
end

function CameraConfig.GetMoveRight()
    local forward = CameraConfig.GetMoveForward()
    return Vector.new(-forward.Y, forward.X, 0.0)
end

function CameraConfig.GetMeshYawOffset()
    return 0.0
end

function CameraConfig.GetPlayerMeshBaseRotation()
    return CameraConfig.PlayerMeshBaseRotation
end

return CameraConfig
