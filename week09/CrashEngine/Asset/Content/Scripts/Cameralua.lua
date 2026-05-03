local Player = nil
local IsZooming = false
local ZoomAlpha = 0.0
local ZoomSpeed = 16.0
local AimYaw = 0.0
local MouseSensitivity = 0.15
function BeginPlay()
    Player = World.FindPlayer();
    ZoomAlpha = 0.0
    AimYaw = 0.0
end

function EndPlay()
    ZoomAlpha = 0.0
    AimYaw = 0.0
end

function OnOverlap(OtherActor)
    OtherActor:PrintLocation();
end

function Tick(dt)
    if Player == nil or not Player:IsValid() then
        Player = World.FindPlayer()
        return
    end

    local TargetZoomAlpha = IsZooming and 1.0 or 0.0
    ZoomAlpha = ZoomAlpha + (TargetZoomAlpha - ZoomAlpha) * math.min(dt * ZoomSpeed, 1.0)

    local PlayerPos = Player.Location
    local Forward = Player:GetForwardVector()
    local Up = Player:GetUpVector()
    if IsZooming == true then
        AimYaw = AimYaw + Input:GetAxis("MouseX") * MouseSensitivity
        local RadYaw = math.rad(AimYaw)
        local Right = Player:GetRightVector()
        Forward = Forward * math.cos(RadYaw) + Right * math.sin(RadYaw)
    end
    local NormalCameraLocation = PlayerPos + Vector.new(-25.0, -10.0, 20.0)
    local NormalLookAtLocation = PlayerPos
    local ZoomCameraLocation = PlayerPos
    local ZoomLookAtLocation = PlayerPos + Up * 1.2 + Forward * 20.0

    local CameraLocation = nil
    local LookAtLocation = nil
    if IsZooming == false then
        CameraLocation = NormalCameraLocation
        LookAtLocation = NormalLookAtLocation
    else
        CameraLocation = ZoomCameraLocation
        LookAtLocation = ZoomLookAtLocation
    end

    CameraLocation = NormalCameraLocation + (ZoomCameraLocation - NormalCameraLocation) * ZoomAlpha
    LookAtLocation = NormalLookAtLocation + (ZoomLookAtLocation - NormalLookAtLocation) * ZoomAlpha
    local Fov = 60.0 + (60.0 - 60.0) * ZoomAlpha

    World.SetCameraView(CameraLocation,LookAtLocation,Fov)
end

function OnMouseButtonPressed(button)
    if button == "RightMouseButton" then
        IsZooming = true
        AimYaw = 0.0
    end
end

function OnMouseButtonReleased(button)
    if button == "RightMouseButton" then
        IsZooming = false
        AimYaw = 0.0
    end
end
