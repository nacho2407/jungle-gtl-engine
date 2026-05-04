local Player = nil
local CameraConfig = dofile("Asset/Content/Scripts/CameraConfig.lua")
local IsZooming = false
local ZoomAlpha = 0.0
local ZoomSpeed = 25.0
local AimYaw = 0.0
local MouseSensitivity = 0.15
local AimActor = nil
local AimText = nil
local AimDistance = 30.0

local function ensureCameraState()
    _G.CameraState = _G.CameraState or {}
    return _G.CameraState
end

local function syncCameraState(aimOrigin, aimPoint)
    local cameraState = ensureCameraState()
    local direction = Vector.new(1.0, 0.0, 0.0)

    if aimOrigin ~= nil and aimPoint ~= nil then
        direction = aimPoint - aimOrigin
        if direction:LengthSquared() > 0.0001 then
            direction = direction:Normalized()
        end
    end

    cameraState.IsZooming = IsZooming
    cameraState.AimOrigin = aimOrigin
    cameraState.AimPoint = aimPoint
    cameraState.AimDirection = direction
end

local function getPlayerForward()
    if _G.PlayerAimState ~= nil and _G.PlayerAimState.Forward ~= nil then
        return _G.PlayerAimState.Forward
    end

    return Player:GetForwardVector()
end

local function getPlayerUp()
    if _G.PlayerAimState ~= nil and _G.PlayerAimState.Up ~= nil then
        return _G.PlayerAimState.Up
    end

    return Player:GetUpVector()
end

local function createAim()
    AimActor = World.SpawnActor("Actor")
    if AimActor == nil or not AimActor:IsValid() then
        return
    end

    AimText = AimActor:AddComponent("TextRenderComponent")
    if AimText:IsValid() then
        AimText:SetText("+")
        AimText:SetFontSize(20.0)
        AimText:SetBillboard(true)
        AimText:SetVisible(false)
        AimText:SetVisibleInEditor(false)
        AimText:SetVisibleInGame(false)
    end
end

local function updateAim(aimOrigin, aimPoint, aimDirection)
    if AimActor == nil or not AimActor:IsValid() then
        return
    end

    AimActor.Location = aimPoint

    if AimText ~= nil and AimText:IsValid() then
        AimText:SetVisible(IsZooming)
        AimText:SetVisibleInGame(IsZooming)
    end
end

function BeginPlay()
    Player = World.FindPlayer();
    ZoomAlpha = 0.0
    AimYaw = 0.0
    syncCameraState()
    createAim()
end

function EndPlay()
    ZoomAlpha = 0.0
    AimYaw = 0.0
    syncCameraState()

    AimActor = nil
    AimText = nil
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
    local Forward = getPlayerForward()
    local Up = getPlayerUp()
    if IsZooming == true then
        AimYaw = AimYaw + Input:GetAxis("MouseX") * MouseSensitivity
        local RadYaw = math.rad(AimYaw)
        local Right = Vector.new(-Forward.Y, Forward.X, 0.0)
        if Right:LengthSquared() > 0.0001 then
            Right = Right:Normalized()
        else
            Right = Player:GetRightVector()
        end
        Forward = Forward * math.cos(RadYaw) + Right * math.sin(RadYaw)
    end
    if Forward:LengthSquared() > 0.0001 then
        Forward = Forward:Normalized()
    end

    local NormalCameraLocation = PlayerPos + CameraConfig.NormalOffset
    local NormalLookAtLocation = PlayerPos
    local ZoomCameraLocation = PlayerPos + Up * 1.2
    local ZoomLookAtLocation = ZoomCameraLocation + Forward * AimDistance
    local AimDirection = ZoomLookAtLocation - ZoomCameraLocation
    if AimDirection:LengthSquared() > 0.0001 then
        AimDirection = AimDirection:Normalized()
    end
    updateAim(ZoomCameraLocation, ZoomLookAtLocation, AimDirection)

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

    syncCameraState(ZoomCameraLocation, ZoomLookAtLocation)
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
