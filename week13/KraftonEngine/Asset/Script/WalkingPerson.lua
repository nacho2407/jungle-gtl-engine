local ObjRegistry = require("ObjRegistry")

local root = nil
local isMoving = true

local SPEED = 2.5
local TURN_INTERVAL = 30.0

local function GetFlatForward()
    local forward = obj.Forward
    forward.Z = 0

    if forward:Length() <= 0.001 then
        return Vector.Forward()
    end

    return forward:Normalized()
end

-- 기존(stale) 회전 coroutine 이 Wait(30) 도중에 reset 을 만나면, reset 후 0~30초 사이에
-- 깨어나서 복원된 rotation 에 또 +180° 를 얹어버린다. generation 카운터로 stale coroutine
-- 이 자기 차례에서 자체 종료되도록 만든다.
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

    isMoving = false

    if root ~= nil then
        root:SetLinearVelocity(Vector.Zero())
        root:SetAngularVelocity(Vector.Zero())
    end
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
