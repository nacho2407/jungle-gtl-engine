local ItemPickup = {}
ItemPickup.__index = ItemPickup

ItemPickup.Properties = {
    ItemType = {
        Type = "String",
        Default = "HealthPack",
        Category = "Item"
    },
    RecoverAmount = {
        Type = "Float",
        Default = 30.0,
        Category = "Item"
    },
    TimeSlowDuration = {
        Type = "Float",
        Default = 5.0,
        Category = "Item"
    },
    TimeSlowScale = {
        Type = "Float",
        Default = 0.5,
        Category = "Item"
    },
    PickupRadius = {
        Type = "Float",
        Default = 2.0,
        Category = "Item"
    }
}

local function ApplyProperties(target, properties)
    properties = properties or {}

    for key, desc in pairs(ItemPickup.Properties) do
        if properties[key] ~= nil then
            target[key] = properties[key]
        else
            target[key] = desc.Default
        end
    end
end

function ItemPickup.new(component, properties)
    local self = setmetatable({}, ItemPickup)

    self.component = component
    self.owner = component:GetOwner()
    self.bConsumed = false
    self.time = 0.0
    self.dropDuration = 0.45
    self.dropStartHeight = 2.0
    self.baseLocation = nil

    ApplyProperties(self, properties)
    return self
end

function ItemPickup:BeginPlay()
    self.baseLocation = nil
end

function ItemPickup:IsPlayer(actor)
    return actor ~= nil and actor:HasTag("Player")
end

function ItemPickup:Consume(player)
    if self.bConsumed then
        return
    end

    if not self:IsPlayer(player) then
        return
    end

    local consumed = false
    if self.ItemType == "TimeSlow" then
        if _G.GameJam and _G.GameJam.ActivateTimeSlow then
            consumed = _G.GameJam.ActivateTimeSlow(self.TimeSlowDuration, self.TimeSlowScale, self.owner)
        end
    else
        if _G.GameJam and _G.GameJam.RecoverPlayer then
            _G.GameJam.RecoverPlayer(self.RecoverAmount, self.owner)
            local manager = _G.GameJam.Manager
            local game = manager and manager.context and manager.context.managers and manager.context.managers.Game
            consumed = game ~= nil and game.isRunning
        end
    end

    if not consumed then
        return
    end

    self.bConsumed = true
    Engine.API.World.DestroyActor(self.owner)
end

function ItemPickup:TryDistancePickup()
    local player = Engine.API.World.FindActorByTag("Player")
    if player == nil then
        return
    end

    local delta = player.Location - self.owner.Location
    if delta:Size() <= (self.PickupRadius or 2.0) then
        self:Consume(player)
    end
end

function ItemPickup:Tick(dt)
    if self.bConsumed then
        return
    end

    local realDt = Engine.API.World.GetUnscaledDeltaTime()
    if realDt <= 0.0 then
        realDt = dt or 0.0
    end

    self.time = self.time + realDt

    if self.baseLocation ~= nil then
        local dropT = math.min(1.0, self.time / self.dropDuration)
        local eased = 1.0 - (1.0 - dropT) * (1.0 - dropT)
        local bob = math.sin(self.time * 4.0) * 0.18
        self.owner.Location = self.baseLocation + Vector(0.0, 0.0, self.dropStartHeight * (1.0 - eased) + bob)
        self.owner.Rotation = self.owner.Rotation + Vector(0.0, 0.0, realDt * 90.0)
    else
        self.baseLocation = self.owner.Location
        self.owner.Location = self.baseLocation + Vector(0.0, 0.0, self.dropStartHeight)
    end

    self:TryDistancePickup()
end

function ItemPickup:OnBeginOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult)
    self:Consume(OtherActor)
end

return ItemPickup
