-- PoliceCarAI.lua
local ObjRegistry = require("ObjRegistry")
-- APoliceCar의 추적 AI. obj는 APoliceCar 인스턴스로 spawn된다.
-- 게임모드가 EscapePolice 페이즈 진입 시 동적 spawn, 이탈/잡힘 시 destroy 한다.
--
-- 동작:
--   1) 페이즈 가드 — EscapePolice 가 아니면 throttle/steer 0 으로 정지
--   2) state == "chase":  target 방향으로 풀 throttle + right·dir dot 으로 steering
--   3) state == "reverse": throttle=-1 + 한 번 추첨한 randomSteer 로 후진. 타이머 만료 시 chase 복귀
--   4) chase 중 forward speed 가 STUCK_SPEED_EPS 미만으로 STUCK_TIME_THRESHOLD 이상 유지되면
--      벽에 박혀 못 움직이는 것으로 판정 → reverse 로 전환
-- 잡힘 판정과 게임오버는 C++ 의 OnComponentHit 핸들러가 담당.

-- 추적 파라미터
local CHASE_THROTTLE        = 1.0
local STEER_GAIN            = 2.0    -- 방향 차이를 steering input(-1..1) 으로 변환

-- stuck 감지 + 복구
local STUCK_SPEED_EPS       = 0.5    -- m/s — 이보다 느리면 not_moving
local STUCK_TIME_THRESHOLD  = 1.2    -- 초 — 이만큼 안 움직이면 stuck
local REVERSE_DURATION      = 1.5    -- 초 — 후진 + 회전 지속 시간

local police       = nil
local state        = "chase"
local stuckTimer   = 0
local reverseTimer = 0
local reverseSteer = 0

local function resetAIState()
    state = "chase"
    stuckTimer = 0
    reverseTimer = 0
    reverseSteer = 0
end

local function tickChase(mv, target, dt)
    -- stuck 감지 — 입력은 throttle 풀로 주는데 실제 forward speed 가 너무 낮으면
    -- 벽 / 다른 액터 / 지형 끼임으로 본다. coner 잠깐 감속 false-trigger 방지를 위해
    -- 임계 시간 STUCK_TIME_THRESHOLD 동안 지속돼야 reverse 로 전환.
    local speed = math.abs(mv:GetForwardSpeed())
    if speed < STUCK_SPEED_EPS then
        stuckTimer = stuckTimer + dt
        if stuckTimer >= STUCK_TIME_THRESHOLD then
            state = "reverse"
            reverseTimer = REVERSE_DURATION
            -- reverseSteer 를 매 frame 다시 뽑으면 핸들이 떨려 제자리. 한 번 추첨 후 고정.
            reverseSteer = (math.random() < 0.5) and -1.0 or 1.0
            stuckTimer = 0
            return
        end
    else
        stuckTimer = 0
    end

    -- 평면(XY)상에서 self → target 방향
    local dir = target.Location - obj.Location
    dir.Z = 0
    local dist = dir:Length()
    if dist < 0.01 then return end
    dir = dir:Normalized()

    -- right 벡터와의 dot — 양수면 target이 우측, 음수면 좌측 → steering 부호로 매핑
    local right = obj.Right
    local steer = right:Dot(dir) * STEER_GAIN
    if steer >  1.0 then steer =  1.0 end
    if steer < -1.0 then steer = -1.0 end

    mv:SetThrottleInput(CHASE_THROTTLE)
    mv:SetSteeringInput(steer)
end

local function tickReverse(mv, dt)
    mv:SetThrottleInput(-1.0)
    mv:SetSteeringInput(reverseSteer)
    reverseTimer = reverseTimer - dt
    if reverseTimer <= 0 then
        -- chase 복귀 — stuckTimer 를 0 으로 리셋해서 즉시 또 reverse 들어가는 것 방지.
        state = "chase"
        stuckTimer = 0
    end
end

function BeginPlay()
    police = obj:AsPoliceCar()
    ObjRegistry.RegisterPoliceCar(police)
    resetAIState()
end

function EndPlay()
end

function Tick(dt)
    -- 페이즈 가드
    local gs = GetGameState()
    if gs == nil then return end
    if gs:GetPhase() ~= ECarGamePhase.EscapePolice then
        local mv = obj:GetCarMovement()
        if mv ~= nil then
            mv:SetThrottleInput(0)
            mv:SetSteeringInput(0)
        end
        resetAIState()
        return
    end

    if police == nil then
        police = obj:AsPoliceCar()
        if police == nil then return end
    end

    local target = police:GetTarget()
    if target == nil or not target:IsValid() then return end

    local mv = obj:GetCarMovement()
    if mv == nil then return end

    if state == "chase" then
        tickChase(mv, target, dt)
    else
        tickReverse(mv, dt)
    end
end
