local ObjRegistry = require("ObjRegistry")

local car = nil
local movement = nil
local startLocationActor = nil   -- Tag = "StartLocation" 인 액터 — R 키 리스폰 기준점.
local ENGINE_LOOP_NAME = "PlayerCarEngine"
local ENGINE_IDLE_PITCH = 0.8
local ENGINE_MAX_PITCH = 3.0
local ENGINE_MAX_PITCH_SPEED = 100.0
local CRASH_SOUND_COOLDOWN = 0.25
local CRASH_MIN_SPEED = 5.0
local CRASH_MAX_SPEED = 50.0
local SIREN_LOOP_SOUND_NAME = "Siren"
local SIREN_LOOP_NAME = "PoliceSirenLoop"
local SIREN_MIN_DISTANCE = 8.0
local SIREN_MAX_DISTANCE = 90.0
local SIREN_MAX_VOLUME = 0.85
local elapsedTime = 0.0
local lastCrashSoundTime = -999.0
local bSirenLoopPlaying = false

local handleComp = nil
local frontLeftWheel = nil
local frontRightWheel = nil
-- BeginPlay 에서 Tire 의 RelativeRotation 을 캐시 (예: (0, 0, -90) — Mesh Yaw 90 상쇄).
-- steering 은 이 base 위에 ±MAX 만큼 가산해 적용하므로, 시각상 base 회전이 유지된다.
local frontLeftWheelBaseRot = nil
local frontRightWheelBaseRot = nil
local HANDLE_MAX_ROTATION_X = 35.0
local HANDLE_ROTATION_SPEED = 14.0
local WHEEL_MAX_ROTATION_Z = 28.0
local WHEEL_ROTATION_SPEED = 14.0

local prevSpeed = 0.0

function BeginPlay()
    car = obj:AsCarPawn()
    if car == nil then
        return
    end

    ObjRegistry.RegisterCar(car)
    movement = car:GetCarMovement()

    -- 명시적 C++ getter — 좌표 휴리스틱 / FName 의존 없이 차량 좌표계 기준 4 코너로
    -- 분류된 시각 메시를 받는다. ACarPawn::ResolveCachedComponents 가 path + Box-local
    -- 좌표 부호로 분류하므로 PIE / scene-load 모두 동일 결과.
    handleComp = car:GetHandleMesh()
    frontLeftWheel  = car:GetFrontLeftTireMesh()
    frontRightWheel = car:GetFrontRightTireMesh()

    -- base rotation 캐시 — 시각 Tire 는 Mesh 의 Yaw 90 을 상쇄하기 위해 (0, 0, -90) 같은
    -- non-zero 회전을 가질 수 있다. steering 보간은 base 위에 가산해야 시각 회전이 깨지지 않는다.
    if frontLeftWheel ~= nil then
        frontLeftWheelBaseRot = frontLeftWheel.Rotation
    end
    if frontRightWheel ~= nil then
        frontRightWheelBaseRot = frontRightWheel.Rotation
    end

    -- 시작 위치 액터를 1회 lookup. 매 frame 재검색하지 않도록 캐시 — World.FindFirstActorByTag
    -- 가 actors 선형 스캔이라 비싸진 않지만, 주기적 호출은 피한다.
    startLocationActor = World.FindFirstActorByTag("StartLocation")

    AudioManager.PlayLoop("CarEngineLoop", ENGINE_LOOP_NAME, 0.5, ENGINE_IDLE_PITCH)
end

local function ResetCarToStart()
    if car == nil or movement == nil then return end
    if startLocationActor == nil or not startLocationActor:IsValid() then
        -- scene 에 StartLocation 태그 액터가 없거나 dangling — 무음 무시.
        return
    end

    -- transform 먼저 set, 그 다음 velocity/angular 0 클리어. PhysX pre-simulate teleport
    -- 임계 (1m²) 를 넘는 큰 위치 변화면 setGlobalPose 로 강제 동기화되므로 안전.
    car.Location = startLocationActor.Location
    car.Rotation = startLocationActor.Rotation
    movement:StopImmediately()
end

local function UpdateHandleRotation(steering, dt)
    if handleComp == nil then
        return
    end

    dt = dt or 0

    local currentRotation = handleComp.Rotation
    local targetX = -steering * HANDLE_MAX_ROTATION_X
    local alpha = math.min(dt * HANDLE_ROTATION_SPEED, 1.0)
    local nextX = currentRotation.X + (targetX - currentRotation.X) * alpha

    handleComp.Rotation = Vector.new(nextX, currentRotation.Y, currentRotation.Z)
end

local function EnsureWheelCache()
    -- hot-reload 후 BeginPlay 가 다시 호출되지 않은 경우에도 새 분류가 적용되도록
    -- 매 Tick 첫 패스에서 lazy 캐싱. 이미 잡힌 핸들/캐시는 그대로 유지 (idempotent).
    if car == nil then return end
    if frontLeftWheel == nil then frontLeftWheel = car:GetFrontLeftTireMesh() end
    if frontRightWheel == nil then frontRightWheel = car:GetFrontRightTireMesh() end
    if frontLeftWheel ~= nil and frontLeftWheelBaseRot == nil then
        frontLeftWheelBaseRot = frontLeftWheel.Rotation
    end
    if frontRightWheel ~= nil and frontRightWheelBaseRot == nil then
        frontRightWheelBaseRot = frontRightWheel.Rotation
    end
end

local function UpdateWheelSteeringRotation(steering, dt)
    EnsureWheelCache()
    dt = dt or 0
    local alpha = math.min(dt * WHEEL_ROTATION_SPEED, 1.0)

    if frontLeftWheel ~= nil and frontLeftWheelBaseRot ~= nil then
        local current = frontLeftWheel.Rotation
        local target = frontLeftWheelBaseRot.Z + steering * WHEEL_MAX_ROTATION_Z
        local nextZ = current.Z + (target - current.Z) * alpha
        frontLeftWheel.Rotation = Vector.new(current.X, current.Y, nextZ)
    end

    if frontRightWheel ~= nil and frontRightWheelBaseRot ~= nil then
        local current = frontRightWheel.Rotation
        local target = frontRightWheelBaseRot.Z + steering * WHEEL_MAX_ROTATION_Z
        local nextZ = current.Z + (target - current.Z) * alpha
        frontRightWheel.Rotation = Vector.new(current.X, current.Y, nextZ)
    end
end

local function SetSirenLoopPlaying(bShouldPlay)
    if bShouldPlay == bSirenLoopPlaying then
        return
    end

    bSirenLoopPlaying = bShouldPlay
    if bSirenLoopPlaying then
        AudioManager.PlayLoop(SIREN_LOOP_SOUND_NAME, SIREN_LOOP_NAME, 0.0, 1.0)
    else
        AudioManager.StopLoop(SIREN_LOOP_NAME)
    end
end

local function UpdatePoliceSiren(phase)
    if phase ~= ECarGamePhase.EscapePolice then
        SetSirenLoopPlaying(false)
        return
    end

    local nearestDistance = ObjRegistry.GetNearestPoliceDistance(car.Location)
    if nearestDistance == nil or nearestDistance >= SIREN_MAX_DISTANCE then
        SetSirenLoopPlaying(false)
        return
    end

    local distanceRatio = (nearestDistance - SIREN_MIN_DISTANCE) / (SIREN_MAX_DISTANCE - SIREN_MIN_DISTANCE)
    distanceRatio = math.max(math.min(distanceRatio, 1.0), 0.0)
    local volumeRatio = 1.0 - distanceRatio

    SetSirenLoopPlaying(true)
    AudioManager.SetLoopVolume(SIREN_LOOP_NAME, SIREN_MAX_VOLUME * volumeRatio)
end

function EndPlay()
    AudioManager.StopLoop(ENGINE_LOOP_NAME)
    SetSirenLoopPlaying(false)
    ObjRegistry.UnregisterCar(car)
end

function OnOverlap(OtherActor)
end

function OnHit(OtherActor, HitComponent, OtherComp, NormalImpulse, Hit)
    if car == nil or movement == nil then
        return
    end

    if elapsedTime - lastCrashSoundTime < CRASH_SOUND_COOLDOWN then
        return
    end

    local speed = math.abs(prevSpeed)
    if speed < CRASH_MIN_SPEED then
        return
    end

    local crashRatio = math.min((speed - CRASH_MIN_SPEED) / (CRASH_MAX_SPEED - CRASH_MIN_SPEED), 1.0)
    local volume = 0.35 + 0.65 * crashRatio
    AudioManager.Play("Crash", volume)
    lastCrashSoundTime = elapsedTime

    CameraManager.StartCameraShakeAsset("Asset/Test.shake", 1.0)
end

function Tick(dt)
    elapsedTime = elapsedTime + dt

    if car == nil or movement == nil then
        return
    end

    local gs = GetGameState()
    if gs == nil then return false end
    local phase = gs:GetPhase()
    UpdatePoliceSiren(phase)

    if phase == ECarGamePhase.CarWash or phase == ECarGamePhase.CarGas then
        movement:StopImmediately()
        movement:SetThrottleInput(0)
        movement:SetSteeringInput(0)
        UpdateHandleRotation(0, dt)
        UpdateWheelSteeringRotation(0, dt)
        AudioManager.SetLoopPitch(ENGINE_LOOP_NAME, ENGINE_IDLE_PITCH)
        return
    end

    -- R → 시작 위치/회전으로 리스폰 (차량이 뒤집혔을 때 복구용).
    if Input.GetKeyDown(Key.R) then
        ResetCarToStart()
    end

    local throttle = 0
    if Input.GetKey(Key.W) then throttle = throttle + 1 end
    if Input.GetKey(Key.S) then throttle = throttle - 1 end

    local steering = 0
    if Input.GetKey(Key.A) then steering = steering - 1 end
    if Input.GetKey(Key.D) then steering = steering + 1 end

    movement:SetThrottleInput(throttle)
    movement:SetSteeringInput(steering)
    UpdateHandleRotation(steering, dt)
    UpdateWheelSteeringRotation(steering, dt)

    local speedRatio = math.min(math.abs(movement:GetForwardSpeed()) / ENGINE_MAX_PITCH_SPEED, 1.0)
    local pitch = ENGINE_IDLE_PITCH + (ENGINE_MAX_PITCH - ENGINE_IDLE_PITCH) * speedRatio
    AudioManager.SetLoopPitch(ENGINE_LOOP_NAME, pitch)

    prevSpeed = movement:GetForwardSpeed()
end
