-- MeteorSpawner.lua
-- DodgeMeteor 페이즈 동안 spawner의 위치 주변 반경에 운석을 떨어뜨린다.
-- 이 스크립트가 attach된 액터(obj)가 spawn 중심이 된다.
-- 운석 자체의 충돌/lifetime/destroy는 cpp의 AMeteor가 담당.

-- 튜닝 파라미터 (디자이너가 lua에서 직접 수정 — 빌드 불필요)
local SPAWN_INTERVAL    = 1.0    -- 초당 1마리
local SPAWN_RADIUS      = 50.0   -- spawner 위치 기준 반경 (XY)
local SPAWN_HEIGHT      = 30.0   -- spawn 시작 Z 오프셋 (위에서 떨어뜨림)
local MAX_CONCURRENT    = 30     -- 동시 활성 운석 한도

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
    local m = World.SpawnActor("AMeteor")
    if m == nil then return end

    local center = obj.Location
    local angle = math.random() * 2 * math.pi
    local r = math.random() * SPAWN_RADIUS
    m.Location = Vector(
        center.X + math.cos(angle) * r,
        center.Y + math.sin(angle) * r,
        center.Z + SPAWN_HEIGHT)

    table.insert(meteors, m)
end

function BeginPlay()
    timer = 0
    meteors = {}
end

function EndPlay()
    -- 페이즈 종료/씬 종료 시 활성 운석 정리
    for _, m in ipairs(meteors) do
        if m:IsValid() then m:Destroy() end
    end
    meteors = {}
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
