local ProjectilePool = require("Asset.Script.ProjectilePool")

local Script = {}
Script.__index = Script

-- ============================================================
-- Projectile Script
-- ============================================================

Script.Properties = {
    bCanTick = {
        Type = "Bool",
        Default = true,
        Category = "Script"
    },

    Speed = {
        Type = "Float",
        Default = 8.0,
        Category = "Script"
    },

    LifeTime = {
        Type = "Float",
        Default = 5.0,
        Category = "Script"
    },

    -- true면 BeginPlay 때 바로 비활성 풀 상태로 들어감
    -- 미리 배치해둔 Projectile Actor들은 true로 두는 게 맞음
    bStartInactive = {
        Type = "Bool",
        Default = true,
        Category = "Pool"
    },
}

function Script.new(component, properties)
    local self = setmetatable({}, Script)

    self.component = component
    self.owner = component:GetOwner()

    -- 여기서 "[BeginPlay]" 로그 찍는 건 틀렸다.
    -- new는 생성자고 BeginPlay가 아니다.
    Log("[Projectile Construct] " .. tostring(self.owner.Name))

    self.Direction = Vector.Forward()
    self.Elapsed = 0.0

    self.bActive = false
    self.bDestroyed = false
    self.CoroutineVersion = 0

    properties = properties or {}

    for key, desc in pairs(Script.Properties) do
        if properties[key] ~= nil then
            self[key] = properties[key]
        else
            self[key] = desc.Default
        end
    end

    return self
end

function Script:SetPooledActive(bActive)
    -- 여기는 네 엔진 바인딩에 맞춰 연결해야 함.
    -- 이상적으로는 아래 기능들이 필요하다.
    --
    -- self.owner:SetHidden(not bActive)
    -- self.owner:SetCollisionEnabled(bActive)
    -- self.component:SetTickEnabled(bActive)

    -- 임시 처리:
    -- 비활성 상태일 때 충돌이 계속 살아있으면 문제가 생기므로
    -- 가능하면 반드시 C++에서 Collision Enable/Disable 바인딩을 추가하는 게 맞다.
    if not bActive then
        self.owner.Location = Vector(0, -1000000, 0)
    end
end

function Script:Activate(spawnLocation, targetActor)
    self.bActive = true
    self.bDestroyed = false
    self.Elapsed = 0.0
    self.Direction = Vector.Forward()

    self.CoroutineVersion = self.CoroutineVersion + 1
    local version = self.CoroutineVersion

    if spawnLocation ~= nil then
        self.owner.Location = spawnLocation
    end

    self:SetPooledActive(true)

    StartCoroutine(function()
        self:SetDirection(version, targetActor)
    end)
end

function Script:ReleaseSelf()
    if self.bDestroyed then
        return
    end

    self.bDestroyed = true
    self.CoroutineVersion = self.CoroutineVersion + 1

    ProjectilePool:Release(self)
end

function Script:SetDirection(version, targetActor)
    coroutine.yield(WaitForFrames(1))

    -- 풀에 반납된 뒤 이전 코루틴이 뒤늦게 실행되는 것 방지
    if not self.bActive then
        return
    end

    if version ~= self.CoroutineVersion then
        return
    end

    local player = targetActor

    if player == nil then
        player = Engine.API.World.FindActorByTag("Player")
    end

    if player == nil then
        LogWarning("[Projectile] Player not found")
        return
    end

    local target = player.Location + Vector.Up() * 1

    local delta = target - self.owner.Location

    if delta:Size() <= 0.001 then
        LogWarning("[Projectile] Direction length is zero")
        return
    end

    self.Direction = delta:Normalized()

    Log("[Projectile Direction] "
        .. self.Direction.X .. ", "
        .. self.Direction.Y .. ", "
        .. self.Direction.Z)
end

function Script:BeginPlay()
    Log("[BeginPlay Projectile] " .. tostring(self.owner.Name))

    ProjectilePool:Register(self)

    if self.bStartInactive then
        self:SetPooledActive(false)
        return
    end

    -- 기존처럼 스폰되자마자 날아가는 Projectile로 쓰고 싶을 때
    self:Activate(self.owner.Location, nil)
end

function Script:Tick(dt)
    if not self.bActive then
        return
    end

    if self.bDestroyed then
        return
    end

    if not self.bCanTick then
        return
    end

    self.Elapsed = self.Elapsed + dt

    if self.Elapsed >= self.LifeTime then
        self:ReleaseSelf()
        return
    end

    self.owner.Location = self.owner.Location + self.Direction * dt * self.Speed
end

function Script:EndPlay()
    Log("[EndPlay Projectile] " .. tostring(self.owner.UUID))
end

function Script:OnBeginOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult)
    if not self.bActive then
        return
    end

    if self.bDestroyed then
        return
    end

    Log("[Projectile OnBeginOverlap] " .. tostring(self.owner.UUID))

    if OtherActor ~= nil and OtherActor:HasTag("Player") then
        self:ReleaseSelf()
        return
    end
end

return Script