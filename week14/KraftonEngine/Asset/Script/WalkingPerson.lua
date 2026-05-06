local ObjRegistry = require("ObjRegistry")

local root = nil
local isMoving = true
local hasPlayedHitAction = false

local SPEED = 2.5
local TURN_INTERVAL = 30.0

local MIN_ACTION_CAR_SPEED = 50.0

local HIT_STOP_DURATION = 0.08
local HIT_STOP_TIME_DILATION = 0.0

local HIT_SQUASH_SCALE = Vector.new(1.25, 1.25, 0.65)
local HIT_SQUASH_IN_DURATION = 0.04
local HIT_SQUASH_RECOVER_DURATION = 0.16

local KNOCKBACK_DISTANCE = 150.0
local KNOCKBACK_DURATION = 0.18

local SLOMO_DURATION = 0.35
local SLOMO_TIME_DILATION = 0.35

local function GetFlatForward()
    local forward = obj.Forward
    forward.Z = 0

    if forward:Length() <= 0.001 then
        return Vector.Forward()
    end

    return forward:Normalized()
end

local function GetKnockbackDirection(OtherActor, Hit)
    if OtherActor ~= nil then
        local direction = obj.Location - OtherActor.Location
        direction.Z = 0

        if direction:Length() > 0.001 then
            return direction:Normalized()
        end
    end

    if Hit ~= nil and Hit.WorldNormal ~= nil then
        local normal = Hit.WorldNormal
        normal.Z = 0

        if normal:Length() > 0.001 then
            return normal:Normalized()
        end
    end

    return GetFlatForward()
end

-- 기존(stale) 회전 coroutine 이 Wait(30) 도중에 reset 을 만나면, reset 후 0~30초 사이에
-- 깨어나서 복원된 rotation 에 또 +180° 를 얹어버린다. generation 카운터로 stale coroutine
-- 이 자기 차례에서 자체 종료되도록 만든다.
local function GetCarCollisionSpeed(CarActor, CarComp)
    if CarActor ~= nil then
        local movement = CarActor:GetCarMovement()
        if movement ~= nil then
            return math.abs(movement:GetForwardSpeed())
        end
    end

    if CarComp ~= nil then
        return CarComp:GetLinearVelocity():Length()
    end

    return 0.0
end

local function ShouldPlayHitAction(CarActor, CarComp, NormalImpulse)
    local carSpeed = GetCarCollisionSpeed(CarActor, CarComp)

    return carSpeed >= MIN_ACTION_CAR_SPEED 
end

local turnGen = 0
-- 가장 최근에 시작된 회전 coroutine 핸들 — EndPlay 에서 명시 stop 해야 옛 핸들이
-- 새 월드 lua tick 에 끼어들지 않는다.
local turnRoutine = nil

local function StopTurnCoroutine()
    if turnRoutine ~= nil then
        StopCoroutine(turnRoutine)
        turnRoutine = nil
    end
end

local function StartTurnCoroutine()
    StopTurnCoroutine()
    turnGen = turnGen + 1
    local myGen = turnGen
    turnRoutine = StartCoroutine(function()
        while isMoving and turnGen == myGen do
            Wait(TURN_INTERVAL)
            if isMoving and turnGen == myGen then
                local rotation = obj.Rotation
                obj.Rotation = Vector.new(rotation.X, rotation.Y, rotation.Z + 180.0)
            end
        end
        turnRoutine = nil
    end)
end

function BeginPlay()
    hasPlayedHitAction = false

    root = obj:GetRootPrimitiveComponent()
    if root == nil then
        root = obj:GetPrimitiveComponent()
    end

    if root ~= nil then
        root:SetSimulatePhysics(true)
    end

    StartTurnCoroutine()
end

-- C++ AWalkingPersonActor::ResetToInitialTransform 가 phase 전환 시 호출.
-- 정지/걷는 중 어느 쪽이든 회전 cycle 을 0 부터 다시 시작 (기존 coroutine 은 generation
-- mismatch 로 다음 Wait 종료 시 알아서 빠짐).
function ResetWalkingState()
    isMoving = true
    StartTurnCoroutine()
end

function EndPlay()
    StopTurnCoroutine()
end

function OnOverlap(OtherActor)
end

function OnHit(OtherActor, HitComponent, OtherComp, NormalImpulse, Hit)
    if ObjRegistry.car == nil or OtherActor == nil or OtherActor.UUID ~= ObjRegistry.car.UUID then
        return
    end

    if hasPlayedHitAction then
        return
    end

    hasPlayedHitAction = true
    isMoving = false

    -- 첫 hit 시 점수 +100 (같은 보행자가 이미 hit 됐으면 hasPlayedHitAction 가드로 추가 가산 X).
    -- Bonus 카테고리 — Finished 때 GameState.GetScore() 가 누적값 반환해 Scoreboard 에 반영됨.
    local gs = GetGameState()
    if gs ~= nil then
        gs:AddScore(100, EScoreCategory.Bonus, "HitPerson", ECarGamePhase.EscapePolice)
    end

    local action = obj:GetActionComponent()
    if action ~= nil and ShouldPlayHitAction(OtherActor, OtherComp, NormalImpulse) then
        action:HitStop(HIT_STOP_DURATION, HIT_STOP_TIME_DILATION)
        --action:HitSquash(HIT_SQUASH_SCALE, HIT_SQUASH_IN_DURATION, HIT_SQUASH_RECOVER_DURATION)
        --action:Knockback(GetKnockbackDirection(OtherActor, Hit), KNOCKBACK_DISTANCE, KNOCKBACK_DURATION)
        action:Slomo(SLOMO_DURATION, SLOMO_TIME_DILATION)
    end

    if root ~= nil then
        root:SetLinearVelocity(Vector.Zero())
        root:SetAngularVelocity(Vector.Zero())
    end
end

function OnEndHit(OtherActor, HitComponent, OtherComp)
    if ObjRegistry.car == nil or OtherActor == nil or OtherActor.UUID ~= ObjRegistry.car.UUID then
        return
    end

    hasPlayedHitAction = false
end

function Tick(dt)
    if root == nil or not isMoving then
        return
    end

    local forward = GetFlatForward()

    local velocity = root:GetLinearVelocity()
    root:SetLinearVelocity(Vector.new(forward.X * SPEED, forward.Y * SPEED, velocity.Z))
    root:SetAngularVelocity(Vector.Zero())
end
