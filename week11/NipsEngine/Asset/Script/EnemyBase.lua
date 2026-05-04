local Script = {}


Script.__index = Script

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
    if not self.bCanMove or self.bMovementLocked then
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
    
    if OtherActor:HasTag("Enemy") then
        return
    end

    StartCoroutine(function()
        self:HitReaction()
    end)
end

function Script:OnEndOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult)
    Log("[Enemy OnEndOverlap] " .. tostring(self.owner.UUID))
end

return Script