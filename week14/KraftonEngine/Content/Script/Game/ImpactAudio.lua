local EventBus = require("Game.LuaEventBus")
local CombatEvents = require("Game.CombatEvents")

local ImpactAudio = {
    _impactSubscription = nil,
    _hitSubscription = nil,
}

local DEFAULT_BULLET_IMPACT_EVENT = "world.default.bullet_impact"
local PLAYER_BULLET_IMPACT_EVENT = "player.bullet_impact"
local DRONE_BULLET_IMPACT_EVENT = "enemy.drone.bullet_impact"
local ENEMY_HUMAN_BULLET_IMPACT_EVENT = "enemy.human.bullet_impact"
local BOSS_HIGH_HEALTH_BULLET_IMPACT_EVENT = "boss.high_health.bullet_impact"
local BOSS_LOW_HEALTH_BULLET_IMPACT_EVENT = "boss.low_health.bullet_impact"

local function is_bullet_context(context)
    if context == nil then
        return false
    end

    local damageType = context.DamageType or context.damageType
    return damageType == nil
        or damageType == "Bullet"
        or damageType == "EnemyBullet"
        or damageType == "cannon"
        or damageType == "powerShot"
end

local function is_valid_actor(actor)
    if actor == nil then
        return false
    end
    if type(actor.IsValid) == "function" then
        return actor:IsValid()
    end
    return true
end

local function has_tag(actor, tag)
    return is_valid_actor(actor) and type(actor.HasTag) == "function" and actor:HasTag(tag)
end

local function same_actor(a, b)
    if not is_valid_actor(a) or not is_valid_actor(b) then
        return false
    end
    if a == b then
        return true
    end
    return a.UUID ~= nil and b.UUID ~= nil and a.UUID == b.UUID
end

local function is_player_actor(actor)
    if not is_valid_actor(actor) then
        return false
    end
    if has_tag(actor, "player") then
        return true
    end
    if Game ~= nil and Game.GetPlayerPawn ~= nil then
        return same_actor(actor, Game.GetPlayerPawn())
    end
    return false
end

local function get_hit_location(context, fallbackActor)
    local location = context ~= nil and (context.HitLocation or context.hitLocation) or nil
    if location ~= nil then
        return location
    end
    if is_valid_actor(fallbackActor) then
        return fallbackActor.Location
    end
    return nil
end

local function play_at(eventName, location)
    if eventName == nil or location == nil then
        return false
    end
    if AudioManager == nil then
        return false
    end
    if AudioManager.PlayEventAt ~= nil then
        return AudioManager.PlayEventAt(eventName, location.X or 0.0, location.Y or 0.0, location.Z or 0.0)
    end
    if AudioManager.PlayOneShotAt ~= nil then
        return AudioManager.PlayOneShotAt(eventName, location.X or 0.0, location.Y or 0.0, location.Z or 0.0)
    end
    return false
end

local function resolve_enemy_hit_event(context, result)
    local victim = result ~= nil and (result.Victim or result.victim) or nil
    if not has_tag(victim, "enemy") then
        return nil
    end

    if has_tag(victim, "drone") then
        return nil
    end

    if has_tag(victim, "boss") then
        local healthRatio = result.HealthRatio or result.healthRatio or 1.0
        if healthRatio <= 0.5 then
            return BOSS_LOW_HEALTH_BULLET_IMPACT_EVENT
        end
        return BOSS_HIGH_HEALTH_BULLET_IMPACT_EVENT
    end

    return ENEMY_HUMAN_BULLET_IMPACT_EVENT
end

local function on_attack_impact(context)
    if not is_bullet_context(context) then
        return
    end

    local hitActor = context.HitActor or context.hitActor
    if is_player_actor(hitActor) then
        play_at(PLAYER_BULLET_IMPACT_EVENT, get_hit_location(context, hitActor))
        return
    end

    if has_tag(hitActor, "drone") then
        play_at(DRONE_BULLET_IMPACT_EVENT, get_hit_location(context, hitActor))
        return
    end

    if has_tag(hitActor, "enemy") then
        return
    end

    play_at(DEFAULT_BULLET_IMPACT_EVENT, get_hit_location(context, hitActor))
end

local function on_attack_hit(context, result)
    if not is_bullet_context(context) then
        return
    end

    local eventName = resolve_enemy_hit_event(context, result)
    if eventName == nil then
        return
    end

    play_at(eventName, get_hit_location(context, result.Victim or result.victim))
end

function ImpactAudio.Initialize()
    ImpactAudio.Shutdown()

    ImpactAudio._impactSubscription = EventBus.On(CombatEvents.Events.AttackImpact, on_attack_impact)
    ImpactAudio._hitSubscription = EventBus.On(CombatEvents.Events.AttackHit, on_attack_hit)
end

function ImpactAudio.Shutdown()
    if ImpactAudio._impactSubscription ~= nil then
        EventBus.Off(ImpactAudio._impactSubscription)
        ImpactAudio._impactSubscription = nil
    end
    if ImpactAudio._hitSubscription ~= nil then
        EventBus.Off(ImpactAudio._hitSubscription)
        ImpactAudio._hitSubscription = nil
    end
end

return ImpactAudio
