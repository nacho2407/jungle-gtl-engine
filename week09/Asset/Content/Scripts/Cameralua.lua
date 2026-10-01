-- Cameralua는 실제 카메라 위치/FOV/조준 상태를 관리한다.
-- PlayerController는 여기서 _G.CameraState로 공유한 줌/조준 정보를 읽어서 발사한다.
local Player = nil
local CameraConfig = dofile("Asset/Content/Scripts/CameraConfig.lua")

-- 줌 보간과 조준 회전 상태.
local IsZooming = false
local AimYaw = 0.0
local AimPitch = 0.0
local ZoomBaseForward = nil
local MouseZoomHeld = false
local GamepadZoomHeld = false
local MouseSensitivity = 0.15
local GamepadControllerId = 0
local GamepadZoomPressThreshold = 0.5
local GamepadZoomReleaseThreshold = 0.25
local GamepadAimYawSpeed = 120.0
local GamepadAimPitchSpeed = 90.0
local MinAimPitch = -45.0
local MaxAimPitch = 45.0
local ZoomVignetteIntensity = 0.45
local ZoomVignetteFadeTime = 0.12
local ZoomVignetteRadius = 0.72
local ZoomVignetteSoftness = 0.35
local CameraTransitionDuration = 0.13
local CameraTransitionBlendType = "Cubic"
local LastCameraZooming = nil
local CameraTransitionElapsed = 0.0
local TransitionFromCameraLocation = nil
local TransitionFromLookAtLocation = nil
local TransitionToCameraLocation = nil
local TransitionToLookAtLocation = nil
local LastAppliedCameraLocation = nil
local LastAppliedLookAtLocation = nil


-- 조준선은 Aim.png를 머티리얼로 쓰는 BillboardComponent Actor로 만든다.
local AimActor = nil
local AimBillboard = nil
local AimDistance = 30.0
local AimMaterial = "Asset/Content/Materials/Aim.json"
local AimSize = 3.0

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

local function getMouseAimForward(playerPos, cameraLocation, fallbackForward)
    if World.GetMouseWorldPointOnPlane == nil then
        return fallbackForward
    end

    local mouseWorldPoint = World.GetMouseWorldPointOnPlane(playerPos.Z)
    local direction = mouseWorldPoint - cameraLocation
    if direction:LengthSquared() > 0.0001 then
        return direction:Normalized()
    end

    return fallbackForward
end

local function clamp01(value)
    if value < 0.0 then
        return 0.0
    elseif value > 1.0 then
        return 1.0
    end

    return value
end

local function evaluateCameraBlend(alpha)
    local t = clamp01(alpha)

    if CameraTransitionBlendType == "Linear" then
        return t
    elseif CameraTransitionBlendType == "EaseIn" then
        return t * t
    elseif CameraTransitionBlendType == "EaseOut" then
        return 1.0 - (1.0 - t) * (1.0 - t)
    end

    return t * t * (3.0 - 2.0 * t)
end

local function lerpVector(from, to, alpha)
    if from == nil then
        return to
    elseif to == nil then
        return from
    end

    return from + (to - from) * alpha
end

local function resetCameraTransition()
    LastCameraZooming = nil
    CameraTransitionElapsed = 0.0
    TransitionFromCameraLocation = nil
    TransitionFromLookAtLocation = nil
    TransitionToCameraLocation = nil
    TransitionToLookAtLocation = nil
    LastAppliedCameraLocation = nil
    LastAppliedLookAtLocation = nil
end

local function hasPlayerCameraBoom()
    if Player == nil or not Player:IsValid() or Player.GetComponent == nil then
        return false
    end

    local cameraBoom = Player:GetComponent("UCameraBoomComponent")
    return cameraBoom ~= nil and cameraBoom:IsValid()
end

local function getNormalCameraLocation(playerPos)
    if hasPlayerCameraBoom() and World.GetActiveCameraLocation ~= nil then
        return World.GetActiveCameraLocation()
    end

    return playerPos + CameraConfig.NormalOffset
end

local function applyCameraView(cameraLocation, lookAtLocation, fov, dt, releaseWhenStable)
    local cameraModeChanged = LastCameraZooming == nil or LastCameraZooming ~= IsZooming
    if cameraModeChanged then
        LastCameraZooming = IsZooming
        CameraTransitionElapsed = 0.0
        TransitionFromCameraLocation = LastAppliedCameraLocation or cameraLocation
        TransitionFromLookAtLocation = LastAppliedLookAtLocation or lookAtLocation
        TransitionToCameraLocation = cameraLocation
        TransitionToLookAtLocation = lookAtLocation
    end

    local transitionActive = CameraTransitionDuration > 0.0 and CameraTransitionElapsed < CameraTransitionDuration
    if transitionActive then
        TransitionToCameraLocation = cameraLocation
        TransitionToLookAtLocation = lookAtLocation
        CameraTransitionElapsed = math.min(CameraTransitionElapsed + dt, CameraTransitionDuration)

        local alpha = evaluateCameraBlend(CameraTransitionElapsed / CameraTransitionDuration)
        cameraLocation = lerpVector(TransitionFromCameraLocation, TransitionToCameraLocation, alpha)
        lookAtLocation = lerpVector(TransitionFromLookAtLocation, TransitionToLookAtLocation, alpha)
    end

    LastAppliedCameraLocation = cameraLocation
    LastAppliedLookAtLocation = lookAtLocation

    if releaseWhenStable and not transitionActive then
        return
    end

    if World.SetCameraView ~= nil then
        World.SetCameraView(cameraLocation, lookAtLocation, fov)
    end
end

local function setMouseCursorVisible(visible)
    if Input.SetCursorVisible ~= nil then
        Input:SetCursorVisible(visible)
    end
end

local function isGamepadConnected()
    if Input.IsGamepadConnected == nil then
        return false
    end

    return Input:IsGamepadConnected(GamepadControllerId)
end

local function setupZoomBaseForward(useMouseAim)
    if Player == nil or not Player:IsValid() then
        ZoomBaseForward = nil
        return
    end

    local playerPos = Player.Location
    local up = getPlayerUp()
    if up:LengthSquared() > 0.0001 then
        up = up:Normalized()
    end

    local cameraLocation = playerPos + up * 1.2
    if useMouseAim then
        ZoomBaseForward = getMouseAimForward(playerPos, cameraLocation, getPlayerForward())
    else
        ZoomBaseForward = getPlayerForward()
    end
end

local function beginZoom(useMouseAim)
    if IsZooming then
        return
    end

    IsZooming = true
    setMouseCursorVisible(false)
    AimYaw = 0.0
    AimPitch = 0.0
    setupZoomBaseForward(useMouseAim)
    World.PlayCameraEffectAsset("Asset/Content/CameraEffects/ZoomVignetteOn.ceffect")
    World.PlayCameraEffectAsset("Asset/Content/CameraEffects/GammaOn.ceffect")
end

local function endZoom()
    if not IsZooming then
        return
    end

    IsZooming = false
    setMouseCursorVisible(true)
    AimYaw = 0.0
    AimPitch = 0.0
    ZoomBaseForward = nil
    World.PlayCameraEffectAsset("Asset/Content/CameraEffects/ZoomVignetteOff.ceffect")
    World.PlayCameraEffectAsset("Asset/Content/CameraEffects/GammaOff.ceffect")
end

local function refreshZoomState(useMouseAim)
    if MouseZoomHeld or GamepadZoomHeld then
        beginZoom(useMouseAim)
    else
        endZoom()
    end
end

local function updateGamepadZoomInput()
    if not isGamepadConnected() then
        if GamepadZoomHeld then
            GamepadZoomHeld = false
            refreshZoomState(false)
        end
        return
    end

    local triggerValue = Input:GetAxis("GamepadLeftTrigger", GamepadControllerId)
    if GamepadZoomHeld then
        if triggerValue <= GamepadZoomReleaseThreshold then
            GamepadZoomHeld = false
            refreshZoomState(false)
        end
        return
    end

    if triggerValue >= GamepadZoomPressThreshold then
        GamepadZoomHeld = true
        refreshZoomState(false)
    end
end

local function updateAimInput(dt)
    AimYaw = AimYaw + Input:GetAxis("MouseX") * MouseSensitivity * World.GetTimeScale()
    AimPitch = AimPitch - Input:GetAxis("MouseY") * MouseSensitivity * World.GetTimeScale()

    if isGamepadConnected() then
        AimYaw = AimYaw + Input:GetAxis("GamepadRightX", GamepadControllerId) * GamepadAimYawSpeed * dt
        AimPitch = AimPitch + Input:GetAxis("GamepadRightY", GamepadControllerId) * GamepadAimPitchSpeed * dt
    end

    if AimPitch < MinAimPitch then
        AimPitch = MinAimPitch
    elseif AimPitch > MaxAimPitch then
        AimPitch = MaxAimPitch
    end
end

-- 줌 중 화면에 보일 조준선 Actor를 만든다.
local function createAim()
    AimActor = World.SpawnActor("Actor")
    if AimActor == nil or not AimActor:IsValid() then
        return
    end

    AimBillboard = AimActor:AddComponent("BillboardComponent")
    if AimBillboard:IsValid() then
        AimBillboard:SetMaterial(0, AimMaterial)
        AimBillboard:SetSpriteSize(AimSize, AimSize)
        AimBillboard:SetCastShadow(false)
        AimBillboard:SetVisible(false)
        AimBillboard:SetVisibleInEditor(false)
        AimBillboard:SetVisibleInGame(false)
    end
end

-- 조준선을 aimPoint 위치로 옮기고, 줌 중일 때만 보이게 한다.
local function updateAim(aimOrigin, aimPoint, aimDirection)
    if AimActor == nil or not AimActor:IsValid() then
        return
    end

    AimActor.Location = aimPoint

    if AimBillboard ~= nil and AimBillboard:IsValid() then
        AimBillboard:SetVisible(IsZooming)
        AimBillboard:SetVisibleInGame(IsZooming)
    end
end

function BeginPlay()
    -- Camera Actor가 시작될 때 Player를 찾아 캐싱한다.
    Player = World.FindPlayer();
    IsZooming = false
    AimYaw = 0.0
    AimPitch = 0.0
    ZoomBaseForward = nil
    MouseZoomHeld = false
    GamepadZoomHeld = false
    resetCameraTransition()
    syncCameraState()
    createAim()
    setMouseCursorVisible(true)
end

function EndPlay()
    -- World가 끝날 때 전역 카메라 상태와 조준선 참조를 정리한다.
    IsZooming = false
    AimYaw = 0.0
    AimPitch = 0.0
    ZoomBaseForward = nil
    MouseZoomHeld = false
    GamepadZoomHeld = false
    resetCameraTransition()
    syncCameraState()

    AimActor = nil
    AimBillboard = nil
    setMouseCursorVisible(true)
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    -- Player가 아직 없거나 삭제되었으면 다시 찾는다.
    if Player == nil or not Player:IsValid() then
        Player = World.FindPlayer()
        return
    end

    updateGamepadZoomInput()

    local PlayerPos = Player.Location
    local Forward = getPlayerForward()
    local Up = getPlayerUp()
    if IsZooming == true then
        if ZoomBaseForward ~= nil and ZoomBaseForward:LengthSquared() > 0.0001 then
            Forward = ZoomBaseForward
        end

        updateAimInput(dt)

        local RadYaw = math.rad(AimYaw)
        local RadPitch = math.rad(AimPitch)
        local Right = Vector.new(-Forward.Y, Forward.X, 0.0)
        if Right:LengthSquared() > 0.0001 then
            Right = Right:Normalized()
        else
            Right = Player:GetRightVector()
        end
        Forward = Forward * math.cos(RadYaw) + Right * math.sin(RadYaw)
        if Up:LengthSquared() > 0.0001 then
            Up = Up:Normalized()
        end
        Forward = Forward * math.cos(RadPitch) + Up * math.sin(RadPitch)
    end
    if Forward:LengthSquared() > 0.0001 then
        Forward = Forward:Normalized()
    end

    local bUseBoomNormalCamera = IsZooming == false and hasPlayerCameraBoom()
    local NormalCameraLocation = getNormalCameraLocation(PlayerPos)
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

    local Fov = 60.0

    syncCameraState(ZoomCameraLocation, ZoomLookAtLocation)
    applyCameraView(CameraLocation, LookAtLocation, Fov, dt, bUseBoomNormalCamera)
end

function OnMouseButtonPressed(button)
    -- 우클릭을 누르면 줌 상태로 진입하고, 이전 줌에서 쌓인 회전은 초기화한다.
    if button == "RightMouseButton" then
        MouseZoomHeld = true
        refreshZoomState(true)
    end
end

function OnMouseButtonReleased(button)
    -- 우클릭을 떼면 쿼터뷰로 돌아가며 조준 회전도 다음 줌을 위해 초기화한다.
    if button == "RightMouseButton" then
        MouseZoomHeld = false
        refreshZoomState(false)
    end
end
