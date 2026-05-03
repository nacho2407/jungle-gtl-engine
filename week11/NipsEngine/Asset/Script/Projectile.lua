local Script = {}
Script.__index = Script

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
}

function Script.new(component, properties)
    local self = setmetatable({}, Script)

    self.component = component
    self.owner = component:GetOwner()
    Log("[BeginPlay Projectile] called" .. self.owner.Name)

    self.Direction = Vector.Forward()
    self.Elapsed = 0.0
    self.bDestroyed = false

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

function Script:DestroySelf()
    if self.bDestroyed then
        return
    end

    self.bDestroyed = true

    Engine.API.World.DestroyActor(self.owner)
end

function Script:BeginPlay()
    Log("[BeginPlay Projectile] called")

    local player = Engine.API.World.FindActorByTag("Player")

    if player == nil then
        LogWarning("[BeginPlay Projectile] Player not found")
        return
    end

    if self.owner == nil then
        LogError("[BeginPlay Projectile] self.owner is nil")
        return
    end

    Log("[BeginPlay Projectile] Player Found")
    Log("[BeginPlay Projectile] Player Location = "
        .. player.Location.X .. ", "
        .. player.Location.Y .. ", "
        .. player.Location.Z)

    Log("[BeginPlay Projectile] Owner Location = "
        .. self.owner.Location.X .. ", "
        .. self.owner.Location.Y .. ", "
        .. self.owner.Location.Z)

    local delta = player.Location - self.owner.Location

    Log("[BeginPlay Projectile] Delta = "
        .. delta.X .. ", "
        .. delta.Y .. ", "
        .. delta.Z)

    local dist = delta:Size()

    Log("[BeginPlay Projectile] Distance = " .. dist)

    if dist > 0.001 then
        self.Direction = delta:Normalized()

        Log("[BeginPlay Projectile] Direction = "
            .. self.Direction.X .. ", "
            .. self.Direction.Y .. ", "
            .. self.Direction.Z)
    else
        LogWarning("[BeginPlay Projectile] Delta too small. Direction not changed")
    end
end

function Script:Tick(dt)
    if self.bDestroyed then
        return
    end

    if not self.bCanTick then
        return
    end

    self.Elapsed = self.Elapsed + dt

    if self.Elapsed >= self.LifeTime then
        self:DestroySelf()
        return
    end

    self.owner.Location = self.owner.Location + self.Direction * dt * self.Speed
end

function Script:EndPlay()
    Log("[EndPlay] " .. tostring(self.owner.UUID))
end

function Script:OnBeginOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult)
    if self.bDestroyed then
        return
    end

    Log("[OnBeginOverlap] " .. tostring(self.owner.UUID))

    if OtherActor ~= nil and OtherActor:HasTag("Player") then
        self:DestroySelf()
        return
    end
end

return Script