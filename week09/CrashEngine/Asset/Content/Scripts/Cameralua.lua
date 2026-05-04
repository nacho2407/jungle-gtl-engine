-- Cameralua는 실제 카메라 위치/FOV/조준 상태를 관리한다.
-- PlayerController는 여기서 _G.CameraState로 공유한 줌/조준 정보를 읽어서 발사한다.
local Player = nil
local CameraConfig = dofile("Asset/Content/Scripts/CameraConfig.lua")

-- 줌 보간과 조준 회전 상태.
local IsZooming = false
local ZoomAlpha = 0.0
local ZoomSpeed = 25.0
local AimYaw = 0.0
local MouseSensitivity = 0.15

-- 조준선은 TextRenderComponent를 가진 별도 Actor로 만든다.
local AimActor = nil
local AimText = nil
local AimDistance = 30.0

-- 카메라 상태는 PlayerController가 읽을 수 있게 _G.CameraState에 공유한다.
local function ensureCameraState()
    _G.CameraState = _G.CameraState or {}
    return _G.CameraState
end

-- 현재 줌 여부, 조준 시작점/끝점/방향을 전역 상태에 반영한다.
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

-- Player mesh가 바라보는 방향을 PlayerController가 _G.PlayerAimState로 공유한다.
local function getPlayerForward()
    if _G.PlayerAimState ~= nil and _G.PlayerAimState.Forward ~= nil then
        return _G.PlayerAimState.Forward
    end

    return Player:GetForwardVector()
end

-- Player의 위쪽 방향. 없으면 Actor의 UpVector를 사용한다.
local function getPlayerUp()
    if _G.PlayerAimState ~= nil and _G.PlayerAimState.Up ~= nil then
        return _G.PlayerAimState.Up
    end

    return Player:GetUpVector()
end

-- 줌 중 화면에 보일 조준선 Actor를 만든다.
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

-- 조준선을 aimPoint 위치로 옮기고, 줌 중일 때만 보이게 한다.
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
    -- Camera Actor가 시작될 때 Player를 찾아 캐싱한다.
    Player = World.FindPlayer();
    ZoomAlpha = 0.0
    AimYaw = 0.0
    syncCameraState()
    createAim()
end

function EndPlay()
    -- World가 끝날 때 전역 카메라 상태와 조준선 참조를 정리한다.
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
    -- Player가 아직 없거나 삭제되었으면 다시 찾는다.
    if Player == nil or not Player:IsValid() then
        Player = World.FindPlayer()
        return
    end

    local TargetZoomAlpha = IsZooming and 1.0 or 0.0
    -- ZoomAlpha는 0=쿼터뷰, 1=줌뷰이며 dt 기반으로 부드럽게 따라간다.
    ZoomAlpha = ZoomAlpha + (TargetZoomAlpha - ZoomAlpha) * math.min(dt * ZoomSpeed, 1.0)

    local PlayerPos = Player.Location
    local Forward = getPlayerForward()
    local Up = getPlayerUp()
    if IsZooming == true then
        -- 줌 중에만 마우스 X 입력으로 조준 방향을 좌우 회전한다.
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

    -- 줌뷰는 Player 위쪽 가까운 위치에서 AimDistance만큼 앞을 바라본다.
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

    -- 최종 카메라는 Normal/Zoom 위치를 ZoomAlpha로 보간한 값이다.
    CameraLocation = NormalCameraLocation + (ZoomCameraLocation - NormalCameraLocation) * ZoomAlpha
    LookAtLocation = NormalLookAtLocation + (ZoomLookAtLocation - NormalLookAtLocation) * ZoomAlpha
    local Fov = 60.0 + (60.0 - 60.0) * ZoomAlpha

    syncCameraState(ZoomCameraLocation, ZoomLookAtLocation)
    World.SetCameraView(CameraLocation,LookAtLocation,Fov)
end

function OnMouseButtonPressed(button)
    -- 우클릭을 누르면 줌 상태로 진입하고, 이전 줌에서 쌓인 회전은 초기화한다.
    if button == "RightMouseButton" then
        IsZooming = true
        AimYaw = 0.0
    end
end

function OnMouseButtonReleased(button)
    -- 우클릭을 떼면 쿼터뷰로 돌아가며 조준 회전도 다음 줌을 위해 초기화한다.
    if button == "RightMouseButton" then
        IsZooming = false
        AimYaw = 0.0
    end
end
