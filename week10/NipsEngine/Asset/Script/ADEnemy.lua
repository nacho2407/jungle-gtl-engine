local EnemyBase = require("Asset.Script.EnemyBase")

local ADEnemy = {}
ADEnemy.__index = ADEnemy

setmetatable(ADEnemy, {
    __index = EnemyBase
})

ADEnemy.Properties = {
    Speed = {
        Type = "Float",
        Default = 2.0,
        Category = "Movement"
    },

    HP = {
        Type = "Int",
        Default = 5,
        Category = "Stats"
    },

    ProjectileInterval = {
        Type = "Float",
        Default = 2.0,
        Category = "Stats"
    },
    
    ShootRange = {
        Type = "Float",
        Default = 8.0,
        Category = "Stats"
    }
}

function ADEnemy.new(component, properties)
    local self = EnemyBase.new(component, properties)
    setmetatable(self, ADEnemy)

    self.ShootTimer = 0.0

    properties = properties or {}

    for key, desc in pairs(ADEnemy.Properties) do
        if properties[key] ~= nil then
            self[key] = properties[key]
        else
            self[key] = desc.Default
        end
    end

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
    local prefabPath = "Asset/Prefab/Projectile"

    local Projectile = Engine.API.World.SpawnActorFromPrefab(prefabPath)

    if Projectile == nil then
        Log("[ADEnemy] Spawn failed: " .. prefabPath)
        return
    end

    local spawnLocation = self.owner.Location + Vector.Up() 
    Projectile.Location = spawnLocation
end

function ADEnemy:Tick(dt)
    if not self.bCanMove or self.bMovementLocked then
        return
    end

    if self.target == nil then
        return
    end

    local delta = self.target.Location - self.owner.Location
    delta.Z = 0

    local distance = delta:Size()

    if distance > self.ShootRange then
        -- 범위 밖이면 이동
        local direction = delta:Normalized()
        local step = math.min(distance, dt * self.Speed)
        self.owner.Location = self.owner.Location + direction * step
    else
        -- 범위 안이면 타이머 누적 후 발사
        self.ShootTimer = self.ShootTimer + dt
        if self.ShootTimer >= self.ProjectileInterval then
            self:SpawnProjectile()
            self.ShootTimer = 0.0
        end
    end
end

return ADEnemy