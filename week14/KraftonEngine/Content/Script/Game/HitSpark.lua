local EventBus = require("Game.LuaEventBus")
local CombatEvents = require("Game.CombatEvents")

local HitSpark = {
    ParticlePath = "Content/Particle/HItSpark.uasset",
    Lifetime = 1.2,
    NormalOffset = 0.03,
    _subscription = nil,
    _active = {},
}

local function is_valid_actor(actor)
    if actor == nil then
        return false
    end
    if type(actor.IsValid) == "function" then
        return actor:IsValid()
    end
    return true
end

local function get_safe_normal(normal, shotDirection)
    if normal ~= nil then
        return normal:Normalized()
    end
    if shotDirection ~= nil then
        return (shotDirection * -1.0):Normalized()
    end
    return Vector.new(0.0, 0.0, 1.0)
end

local function offset_location(location, normal)
    return Vector.new(
        location.X + normal.X * HitSpark.NormalOffset,
        location.Y + normal.Y * HitSpark.NormalOffset,
        location.Z + normal.Z * HitSpark.NormalOffset)
end

function HitSpark.PlayAt(location, normal, shotDirection)
    if location == nil then
        return nil
    end

    local surfaceNormal = get_safe_normal(normal, shotDirection)
    local spawnLocation = offset_location(location, surfaceNormal)
    local actor = World.SpawnParticleSystem(HitSpark.ParticlePath, spawnLocation)
    if actor == nil then
        return nil
    end

    actor:AddTag("runtime-hit-spark")
    HitSpark._active[#HitSpark._active + 1] = {
        Actor = actor,
        Remaining = HitSpark.Lifetime,
    }
    return actor
end

function HitSpark.PlayFromContext(context)
    if context == nil then
        return nil
    end

    return HitSpark.PlayAt(
        context.HitLocation or context.hitLocation,
        context.HitNormal or context.hitNormal,
        context.ShotDirection or context.shotDirection)
end

function HitSpark.Initialize()
    if HitSpark._subscription ~= nil then
        EventBus.Off(HitSpark._subscription)
        HitSpark._subscription = nil
    end

    HitSpark._subscription = EventBus.On(CombatEvents.Events.AttackImpact, function(context)
        HitSpark.PlayFromContext(context)
    end)
end

function HitSpark.Shutdown()
    EventBus.Off(HitSpark._subscription)
    HitSpark._subscription = nil

    for _, entry in ipairs(HitSpark._active) do
        if is_valid_actor(entry.Actor) then
            entry.Actor:Destroy()
        end
    end
    HitSpark._active = {}
end

function HitSpark.Tick(dt)
    for index = #HitSpark._active, 1, -1 do
        local entry = HitSpark._active[index]
        if not is_valid_actor(entry.Actor) then
            table.remove(HitSpark._active, index)
        else
            entry.Remaining = entry.Remaining - dt
            if entry.Remaining <= 0.0 then
                entry.Actor:Destroy()
                table.remove(HitSpark._active, index)
            end
        end
    end
end

return HitSpark
