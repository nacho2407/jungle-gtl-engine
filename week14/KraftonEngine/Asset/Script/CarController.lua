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
local wheelComp0 = nil
local wheelComp1 = nil
local HANDLE_MAX_ROTATION_X = 35.0
local HANDLE_ROTATION_SPEED = 14.0
local WHEEL_MAX_ROTATION_Z = 28.0
local WHEEL_ROTATION_SPEED = 14.0

function BeginPlay()
    car = obj:AsCarPawn()
    if car == nil then
        return
    end

    ObjRegistry.RegisterCar(car)
    movement = car:GetCarMovement()

    handleComp = car:GetComponentByName("UStaticMeshComponent_41")
    wheelComp0 = car:GetComponentByName("UStaticMeshComponent_42")
    wheelComp1 = car:GetComponentByName("UStaticMeshComponent_43")

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

local function UpdateWheelSteeringRotation(steering, dt)
    dt = dt or 0

    local targetZ = steering * WHEEL_MAX_ROTATION_Z
    local alpha = math.min(dt * WHEEL_ROTATION_SPEED, 1.0)

    if wheelComp0 ~= nil then
        local currentRotation = wheelComp0.Rotation
        local nextZ = currentRotation.Z + (targetZ - currentRotation.Z) * alpha
        wheelComp0.Rotation = Vector.new(currentRotation.X, currentRotation.Y, nextZ)
    end

    if wheelComp1 ~= nil then
        local currentRotation = wheelComp1.Rotation
        local nextZ = currentRotation.Z + (targetZ - currentRotation.Z) * alpha
        wheelComp1.Rotation = Vector.new(currentRotation.X, currentRotation.Y, nextZ)
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

    local speed = math.abs(movement:GetForwardSpeed())
    if speed < CRASH_MIN_SPEED then
        return
    end

    local crashRatio = math.min((speed - CRASH_MIN_SPEED) / (CRASH_MAX_SPEED - CRASH_MIN_SPEED), 1.0)
    local volume = 0.35 + 0.65 * crashRatio
    AudioManager.Play("Crash", volume)
    lastCrashSoundTime = elapsedTime
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
end
