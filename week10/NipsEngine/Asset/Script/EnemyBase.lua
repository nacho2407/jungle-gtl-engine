local Script = {}


Script.__index = Script

local ATTACK_ID_PREFIX = "AttackId:"

local function ExtractAttackId(actor)
    if actor == nil or actor.GetTags == nil then
        return nil
    end

    local tags = actor:GetTags()
    if tags == nil then
        return nil
    end

    for _, tag in ipairs(tags) do
        tag = tostring(tag or "")
        if string.sub(tag, 1, #ATTACK_ID_PREFIX) == ATTACK_ID_PREFIX then
            return string.sub(tag, #ATTACK_ID_PREFIX + 1)
        end
    end

    return nil
end

Script.Properties = {
    Speed = {
        Type = "Float",
        Default = 5.0,
        Category = "Movement"
    },

    HP = {
        Type = "Int",
        Default = 1,
        Category = "Stats"
    },

    bCanMove = {
        Type = "Bool",
        Default = true,
        Category = "Movement"
    }
}

function Script.new(component, properties)
    local self = setmetatable({}, Script)

    self.component = component
    self.owner = component:GetOwner()
    self.target = nil
    self.bMovementLocked = false
    self.bWasCutByBlade = false
    self.lastAttackId = nil

    -- 런타임 전용 상태
    self.bIsHitReacting = false

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

function Script:HitReaction()
    if self.bIsHitReacting then
        return
    end

    self.bIsHitReacting = true
    self.bMovementLocked = true

    Log("[Lua] HitReaction start")

    coroutine.yield(WaitForSeconds(1.0))

    self.bIsHitReacting = false
    self.bMovementLocked = false

    Log("[Lua] HitReaction end")
end

function Script:BeginPlay()
    Log("[Enemy BeginPlay] " .. tostring(self.owner.UUID))
    local player = Engine.API.World.FindActorByTag("Player")

    if player ~= nil then
        self.target = player
        Log("[Enemy BeginPlay] " .. tostring(player.Name))
    else
        Log("[Enemy BeginPlay] 검색한 대상이 존재하지 않음")
    end
end

function Script:Tick(dt)
    if not self.bCanMove or self.bMovementLocked or self.target == nil then
        return
    end

    local delta = self.target.Location - self.owner.Location
    delta.Z = 0

    local distance = delta:Size()

    if distance > 0.1 then
        local direction = delta:Normalized()

        -- Yaw (Z축) - 플레이어 방향 바라보기
        local targetYaw = math.atan2(direction.Y, direction.X) * (180 / math.pi)
        local currentRot = self.owner.Rotation
        local diff = (targetYaw - currentRot.Z + 540) % 360 - 180
        local newYaw = currentRot.Z + diff * math.min(1.0, dt * 10)
        self.owner.Rotation = Vector(currentRot.X, currentRot.Y, newYaw)

        -- 이동
        local step = math.min(distance, dt * self.Speed)
        self.owner.Location = self.owner.Location + direction * step
    end
end

function Script:EndPlay()
    Log("[Enemy EndPlay] " .. tostring(self.owner.UUID))
end

function Script:OnHit(HitComponent, OtherActor, OtherComp, NormalImpulse, Hit)
    Log("[Enemy OnHit] " .. tostring(self.owner.UUID))
end

function Script:OnBeginOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult)
    Log("[Enemy OnBeginOverlap] " .. tostring(self.owner.UUID))

    if OtherActor == nil then
        return
    end

    if OtherActor:IsA("ABladeSlash") then
        self.bWasCutByBlade = true
        self.lastAttackId = ExtractAttackId(OtherActor)
        
        if _G.GameJam and _G.GameJam.ActivateHitStop then
            _G.GameJam.ActivateHitStop(0.1, 0.2)
        end
        return
    end
    
    if OtherActor:HasTag("Enemy") then
        return
    end

    if OtherActor:HasTag("Player") then
        if _G.GameJam and _G.GameJam.DamagePlayer then
            _G.GameJam.DamagePlayer(nil, self.owner)
        end
    end

    StartCoroutine(function()
        self:HitReaction()
    end)
end

function Script:OnEndOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult)
    Log("[Enemy OnEndOverlap] " .. tostring(self.owner.UUID))
end

return Script
