-- PoliceCarAI.lua
-- APoliceCar의 추적 AI. obj는 APoliceCar 인스턴스로 spawn된다.
-- 게임모드가 EscapePolice 페이즈 진입 시 동적 spawn, 이탈/잡힘 시 destroy 한다.
--
-- 동작:
--   1) 매 Tick 페이즈 가드 — EscapePolice 가 아니면 정지
--   2) target = obj:GetTarget() 가져와서 위치/방향 계산
--   3) 차량 right 벡터와 (target - self) 방향의 dot 으로 steering 합성
--   4) throttle 은 풀 가속 (cone 각도/시야 가림 판정 없음 — v1)
-- 잡힘 판정과 게임오버는 C++ 의 OnComponentHit 핸들러가 담당.

local CHASE_THROTTLE = 1.0
local STEER_GAIN     = 2.0    -- 방향 차이를 steering input(-1..1) 으로 변환

local police = nil

function BeginPlay()
    police = obj:AsPoliceCar()
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
        return
    end

    if police == nil then
        police = obj:AsPoliceCar()
        if police == nil then return end
    end

    local target = police:GetTarget()
    if target == nil or not target:IsValid() then return end

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

    local mv = obj:GetCarMovement()
    if mv ~= nil then
        mv:SetThrottleInput(CHASE_THROTTLE)
        mv:SetSteeringInput(steer)
    end
end
