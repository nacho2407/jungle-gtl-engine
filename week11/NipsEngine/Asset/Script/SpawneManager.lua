local TitleState = require("Asset.Script.Game.States.TitleState")

local SpawnManager = {}
SpawnManager.__index = SpawnManager

-- Editor에 노출될 기본 Property 목록
SpawnManager.Properties = {
    bCanTick = {
        Type = "Bool",
        Default = true,
        Category = "SpawnManager"
    },

    bAutoStart = {
        Type = "Bool",
        Default = true,
        Category = "SpawnManager"
    },

    -- SpawnPoint를 못 찾았을 때 사용할 fallback 위치
    spawnLocation = {
        Type = "Vector",
        Default = {0, 0, 0},
        Category = "SpawnManager"
    }
}

-- 굳이 Property로 안 빼도 되는 설정값들
local SpawnConfig = {
    -- 스폰 간격
    StartSpawnInterval = 1.5,     -- 시작 시 1.5초마다 스폰
    MinSpawnInterval = 0.25,      -- 5분 후 최소 0.25초마다 스폰
    IntervalRampTime = 300.0,     -- 5분 = 300초

    -- 한 번 스폰 타이밍마다 몇 마리 생성할지
    SpawnCountPerTick = 1,

    -- SpawnPoint가 하나도 없을 때 fallback 위치 사용 여부
    bUseFallbackSpawnLocation = true,

    SpawnPointActiveInterval = 20.0,
    SpawnPointActiveCount = 2
}

local SpawnPrefabsInfo = {
    "NormalEnemy",
    "SpeedEnemy",
    "BigEnemy",
    "ADEnemy"
}

local function Clamp01(value)
    if value < 0.0 then
        return 0.0
    end

    if value > 1.0 then
        return 1.0
    end

    return value
end

local function Lerp(a, b, t)
    return a + (b - a) * t
end

-- instance를 반환하는 함수 SpawnManager 객체화하는 함수
function SpawnManager.new(component, properties)
    local self = setmetatable({}, SpawnManager)

    self.component = component
    self.owner = component:GetOwner()

    self.bRunning = false

    self.spawnedCount = 0

    -- 총 게임 진행 시간
    self.totalElapsed = 0.0

    -- 마지막 스폰 이후 지난 시간
    self.spawnElapsed = 0.0

    -- 현재 스폰 간격
    self.SpawnInterval = SpawnConfig.StartSpawnInterval

    -- BeginPlay 때 캐싱한 전체 SpawnPoint
    self.SpawnPoints = {}

    -- 현재 활성화된 SpawnPoint
    self.ActiveSpawnPoints = {}

    properties = properties or {}

    for key, desc in pairs(SpawnManager.Properties) do
        if properties[key] ~= nil then
            self[key] = properties[key]
        else
            self[key] = desc.Default
        end
    end

    return self
end

function SpawnManager:CacheSpawnPoints()
    self.SpawnPoints = {}
    self.ActiveSpawnPoints = {}

    local SpawnPointActors = Engine.API.World.FindActorsByTag("SpawnPoint")

    if SpawnPointActors == nil then
        Log("[SpawnManager] SpawnPointActors is nil")
        return
    end

    for i, actor in ipairs(SpawnPointActors) do
        if actor ~= nil then
            local location = actor.Location

            local groupIndex = math.floor((i - 1) / SpawnConfig.SpawnPointActiveCount)

            table.insert(self.SpawnPoints, {
                Index = i,
                Actor = actor,
                Location = location,
                ActiveTime = groupIndex * SpawnConfig.SpawnPointActiveInterval,
                bActive = false
            })

            Log("[SpawnManager] Cached SpawnPoint " .. tostring(i))
        end
    end

    Log("[SpawnManager] Cached SpawnPoint Count: " .. tostring(#self.SpawnPoints))
end

function SpawnManager:UpdateActiveSpawnPoints()
    self.ActiveSpawnPoints = {}

    for i, point in ipairs(self.SpawnPoints) do
        if self.totalElapsed >= point.ActiveTime then
            if not point.bActive then
                point.bActive = true
                Log("[SpawnManager] SpawnPoint Activated: " .. tostring(point.Index))
            end

            table.insert(self.ActiveSpawnPoints, point)
        end
    end
end

function SpawnManager:UpdateSpawnInterval()
    local t = Clamp01(self.totalElapsed / SpawnConfig.IntervalRampTime)

    self.SpawnInterval = Lerp(
        SpawnConfig.StartSpawnInterval,
        SpawnConfig.MinSpawnInterval,
        t
    )
end

function SpawnManager:GetRandomPrefabIndex()
    local count = #SpawnPrefabsInfo

    if count <= 0 then
        return nil
    end

    return Engine.API.Random.RandomInt(1, count)
end

function SpawnManager:GetRandomActiveSpawnPoint()
    local count = #self.ActiveSpawnPoints

    if count <= 0 then
        return nil
    end

    local index = Engine.API.Random.RandomInt(1, count)
    return self.ActiveSpawnPoints[index]
end

---@param prefabIndex integer
---@param spawnLocation Vector
---@return AActor|nil enemy
function SpawnManager:SpawnEnemy(prefabIndex, spawnLocation)
    prefabIndex = prefabIndex or 1
    spawnLocation = spawnLocation or self.spawnLocation

    local prefabName = SpawnPrefabsInfo[prefabIndex]

    if prefabName == nil then
        Log("[SpawnManager] Invalid prefab index: " .. tostring(prefabIndex))
        return nil
    end

    local prefabPath = "Asset/Prefab/Enemy/" .. prefabName

    local enemy = Engine.API.World.SpawnActorFromPrefab(prefabPath)

    if enemy == nil then
        Log("[SpawnManager] Spawn failed: " .. prefabPath)
        return nil
    end

    if spawnLocation ~= nil then
        enemy.Location = spawnLocation
    end

    self.spawnedCount = self.spawnedCount + 1

    Log("[SpawnManager] Spawned: " .. prefabName .. " Count: " .. tostring(self.spawnedCount))

    return enemy
end

function SpawnManager:SpawnOnce()
    local prefabIndex = self:GetRandomPrefabIndex()

    if prefabIndex == nil then
        Log("[SpawnManager] No prefab info")
        return
    end

    local spawnPoint = self:GetRandomActiveSpawnPoint()

    if spawnPoint ~= nil then
        self:SpawnEnemy(prefabIndex, spawnPoint.Location)
        return
    end

    if SpawnConfig.bUseFallbackSpawnLocation then
        Log("[SpawnManager] No active SpawnPoint. Use fallback spawnLocation")
        self:SpawnEnemy(prefabIndex, self.spawnLocation)
        return
    end

    Log("[SpawnManager] No active SpawnPoint")
end

function SpawnManager:StartSpawn()
    self.bRunning = true

    self.totalElapsed = 0.0
    self.spawnElapsed = 0.0
    self.SpawnInterval = SpawnConfig.StartSpawnInterval

    self:UpdateActiveSpawnPoints()
end

function SpawnManager:PauseSpawn()
    self.bRunning = false
end

function SpawnManager:StopSpawn()
    self.bRunning = false

    self.totalElapsed = 0.0
    self.spawnElapsed = 0.0
    self.SpawnInterval = SpawnConfig.StartSpawnInterval
end

-- LifeCycle ---------------------------

function SpawnManager:BeginPlay()
    self:CacheSpawnPoints()

    if self.bAutoStart then
        Log("[SpawnManager BeginPlay] Spawn 시작")
        self:StartSpawn()
    end
end

function SpawnManager:Tick(dt)
    if not self.bCanTick then
        return
    end

    if not self.bRunning then
        return
    end

    self.totalElapsed = self.totalElapsed + dt
    self.spawnElapsed = self.spawnElapsed + dt

    self:UpdateActiveSpawnPoints()
    self:UpdateSpawnInterval()

    if self.spawnElapsed >= self.SpawnInterval then
        self.spawnElapsed = 0.0

        for i = 1, SpawnConfig.SpawnCountPerTick do
            self:SpawnOnce()
        end
    end
end

function SpawnManager:EndPlay()
    Log("[SpawnManager EndPlay] " .. tostring(self.owner.UUID))
end

return SpawnManager