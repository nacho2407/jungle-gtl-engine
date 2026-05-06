local EnemyBase = require("Asset.Script.EnemyBase")
local ProjectilePool = require("Asset.Script.ProjectilePool")

local ADEnemy = {}
ADEnemy.__index = ADEnemy

setmetatable(ADEnemy, {
    __index = EnemyBase
})

ADEnemy.Properties = {
    Speed = {
        Type = "Float",
        Default = 3.0,
        Category = "Movement"
    },

    HP = {
        Type = "Int",
        Default = 5,
        Category = "Stats"
    },

    ProjectileInterval = {
        Type = "Float",
        Default = 1.0,
        Category = "Stats"
    },

    ShootRange = {
        Type = "Float",
        Default = 12.0,
        Category = "Stats"
    },

    MoveStopRange = {
        Type = "Float",
        Default = 7.0,
        Category = "Stats"
    },

    RetreatRange = {
        Type = "Float",
        Default = 4.0,
        Category = "Stats"
    },

    ProjectileCount = {
        Type = "Int",
        Default = 3,
        Category = "Attack"
    },

    ProjectileSpread = {
        Type = "Float",
        Default = 1.0,
        Category = "Attack"
    }
}

function ADEnemy.new(component, properties)
    local self = EnemyBase.new(component, properties)
    setmetatable(self, ADEnemy)

    properties = properties or {}

    for key, desc in pairs(ADEnemy.Properties) do
        if properties[key] ~= nil then
            self[key] = properties[key]
        else
            self[key] = desc.Default
        end
    end

    -- 프로퍼티 복사 후 초기화해야 함
    self.ShootTimer = tonumber(self.ProjectileInterval) or 1.0

    return self
end
function ADEnemy:BeginPlay()
    EnemyBase.BeginPlay(self)

    -- 🔧 수정 4: 로그 메시지 클래스명 수정
    Log("[ADEnemy BeginPlay]")
end

function ADEnemy:OnBeginOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult)
    EnemyBase.OnBeginOverlap(self, OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult)

    -- 🔧 수정 4: 로그 메시지 클래스명 수정
    Log("[ADEnemy Overlap]")
end

function ADEnemy:SpawnProjectile()
    Log("[ADEnemy] SpawnProjectile called")

    local prefabPath = "Asset/Prefab/Projectile"

    local count = math.max(1, math.floor(self.ProjectileCount or 1))
    local spread = tonumber(self.ProjectileSpread) or 0.0

    local centerIndex = (count + 1) * 0.5

    for i = 1, count do
        local offsetIndex = i - centerIndex

        local spawnOffset = Vector.Right() * (offsetIndex * spread)
        local spawnLocation = self.owner.Location + Vector.Up() + spawnOffset

        local projectile = ProjectilePool:Spawn(spawnLocation, self.target, prefabPath)

        if projectile == nil then
            LogWarning("[ADEnemy] SpawnProjectile 실패 index=" .. tostring(i))
        else
            Log("[ADEnemy] Projectile spawned index=" .. tostring(i))
        end
    end
end

function ADEnemy:Tick(dt)
    if self.target == nil then
        return
    end

    local delta = self.target.Location - self.owner.Location
    delta.Z = 0

    local distance = delta:Size()

    if distance <= 0.001 then
        return
    end

    local direction = delta:Normalized()

    local moveStopRange = tonumber(self.MoveStopRange) or 7.0
    local retreatRange = tonumber(self.RetreatRange) or 4.0
    local speed = tonumber(self.Speed) or 3.0
    local interval = tonumber(self.ProjectileInterval) or 1.0

    -- 이동 로직만 bCanMove / bMovementLocked 영향을 받음
    if self.bCanMove and not self.bMovementLocked then
        if distance < retreatRange then
            self.owner.Location = self.owner.Location - direction * speed * dt

        elseif distance > moveStopRange then
            local step = math.min(distance - moveStopRange, speed * dt)
            self.owner.Location = self.owner.Location + direction * step
        end
    end

    -- 발사 로직: 거리/이동상태와 독립
    self.ShootTimer = (tonumber(self.ShootTimer) or 0.0) + dt

    if self.ShootTimer >= interval then
        self:SpawnProjectile()
        self.ShootTimer = 0.0
    end
end

return ADEnemy