-- MeteorSpawner.lua
-- DodgeMeteor 페이즈 동안 차량(ObjRegistry.car) 위치 주변 반경에 운석을 떨어뜨린다.
-- spawner 액터의 위치는 무관 — 차량을 따라 다니며 spawn 한다.
-- 운석 자체의 충돌/lifetime/destroy는 cpp의 AMeteor가 담당.

-- 튜닝 파라미터 (디자이너가 lua에서 직접 수정 — 빌드 불필요)
local SPAWN_INTERVAL       = 0.2    -- 초마다 1마리
local SPAWN_RADIUS         = 30.0   -- 차량 위치 기준 XY 반경 — spawn 중심점 분포
local SPAWN_HEIGHT         = 30.0   -- spawn 시작 Z 오프셋 (위에서 떨어뜨림)
local MAX_CONCURRENT       = 60     -- 동시 활성 운석 한도 (Lifetime × spawn rate 보다 여유)

-- launch velocity — 중력만으로는 직낙하라 회피가 너무 쉬워서 spawn 시 추가 운동량 부여.
-- LATERAL 너무 크면 운석이 도시 외곽으로 날아가 콜리전 없는 영역에서 Lifetime 다 채울 때까지
-- 살아있어 spawn 한도 잠식 → 작게 유지.
local LAUNCH_LATERAL_MAX   = 8.0    -- 수평 방향 랜덤 속도 0~max (m/s) — 시각적으로 잘 보이도록 키움
local LAUNCH_DOWN_BASE     = 6.0    -- 추가 하강 속도 baseline (m/s)
local LAUNCH_DOWN_RANDOM   = 6.0    -- 추가 하강 속도 랜덤 추가량 (0~max)

local ObjRegistry = require("ObjRegistry")

-- 내부 상태
local timer = 0
local meteors = {}

local function isActivePhase()
    local gs = GetGameState()
    if gs == nil then return false end
    return gs:GetPhase() == ECarGamePhase.DodgeMeteor
end

local function pruneDead()
    for i = #meteors, 1, -1 do
        if not meteors[i]:IsValid() then
            table.remove(meteors, i)
        end
    end
end

local function spawnOne()
    -- 차량 위치 기준으로 spawn — spawner 액터가 어디 있든 무관하게 항상 player 머리 위.
    local target = ObjRegistry.car
    if target == nil or not target:IsValid() then return end

    local m = World.SpawnActor("AMeteor")
    if m == nil then return end

    -- target.Location 은 by-value getter — 복사본을 받아 mutate 후 m.Location 에 대입.
    -- Vector 글로벌 호출 없이 멤버 set 으로 새 위치 / 속도를 만든다 (sol::environment 회피).
    local center = target.Location
    local angle = math.random() * 2 * math.pi
    local r = math.random() * SPAWN_RADIUS
    local pos = m.Location
    pos.X = center.X + math.cos(angle) * r
    pos.Y = center.Y + math.sin(angle) * r
    pos.Z = center.Z + SPAWN_HEIGHT
    m.Location = pos

    -- 초기 launch velocity — 랜덤 lateral + 추가 down. 중력 + 이 운동량으로 player
    -- 주변에 산발적으로 도달.
    local vAngle = math.random() * 2 * math.pi
    local vSpeed = math.random() * LAUNCH_LATERAL_MAX
    local vel = m.Location
    vel.X = math.cos(vAngle) * vSpeed
    vel.Y = math.sin(vAngle) * vSpeed
    vel.Z = -LAUNCH_DOWN_BASE - math.random() * LAUNCH_DOWN_RANDOM
    m:SetLaunchVelocity(vel)

    table.insert(meteors, m)
end

function BeginPlay()
    timer = 0
    meteors = {}
end

function EndPlay()
end

function Tick(dt)
    if not isActivePhase() then
        -- 비활성 페이즈에선 활성 운석을 모두 정리하고 타이머 리셋
        if #meteors > 0 then
            for _, m in ipairs(meteors) do
                if m:IsValid() then m:Destroy() end
            end
            meteors = {}
        end
        timer = 0
        return
    end

    pruneDead()

    timer = timer + dt
    if timer >= SPAWN_INTERVAL and #meteors < MAX_CONCURRENT then
        timer = 0
        spawnOne()
    end
end

function OnOverlap(OtherActor)
end
