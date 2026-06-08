local CombatEvents = require("Game.CombatEvents")

local PLAYER_TAG = "player"
local DAMAGE_AMOUNT = 5.0
local DAMAGE_INTERVAL = 0.2
local DAMAGE_TYPE = "ElectricTrap"

local overlappingPlayers = {}

local function is_valid_actor(actor)
    if actor == nil then
        return false
    end
    if type(actor.IsValid) == "function" then
        return actor:IsValid()
    end
    return true
end

local function actor_id(actor)
    if not is_valid_actor(actor) then
        return nil
    end
    return actor.UUID or tostring(actor)
end

local function has_tag(subject, tag)
    if subject == nil or type(subject.HasTag) ~= "function" then
        return false
    end

    local ok, result = pcall(function()
        return subject:HasTag(tag)
    end)
    return ok and result == true
end

local function is_player_actor(actor, component)
    if not is_valid_actor(actor) then
        return false
    end

    if has_tag(actor, PLAYER_TAG) or has_tag(component, PLAYER_TAG) then
        return true
    end

    if Game ~= nil and Game.GetPlayerPawn ~= nil then
        return actor == Game.GetPlayerPawn()
    end

    return false
end

local function apply_trap_damage(target)
    if not is_valid_actor(target) then
        return false
    end

    if CombatEvents.IsDamageable ~= nil and not CombatEvents.IsDamageable(target) then
        return false
    end

    local context = CombatEvents.MakeDamageContext({
        Instigator = obj,
        DamageCauser = obj,
        HitActor = target,
        Damage = DAMAGE_AMOUNT,
        DamageType = DAMAGE_TYPE,
    })

    CombatEvents.ApplyDamageAndNotify(obj, target, context)
    return true
end

function BeginPlay()
    overlappingPlayers = {}
end

function EndPlay()
    overlappingPlayers = {}
end

function OnOverlap(OtherActor, OverlappedComponent, OtherComponent)
    if not is_player_actor(OtherActor, OtherComponent) then
        return
    end

    local id = actor_id(OtherActor)
    if id == nil then
        return
    end

    local entry = overlappingPlayers[id]
    if entry ~= nil then
        entry.OverlapCount = entry.OverlapCount + 1
        return
    end

    overlappingPlayers[id] = {
        Actor = OtherActor,
        OverlapCount = 1,
        TimeUntilNextDamage = DAMAGE_INTERVAL,
    }

    apply_trap_damage(OtherActor)
end

function OnEndOverlap(OtherActor, OverlappedComponent, OtherComponent)
    local id = actor_id(OtherActor)
    if id == nil then
        return
    end

    local entry = overlappingPlayers[id]
    if entry == nil then
        return
    end

    entry.OverlapCount = entry.OverlapCount - 1
    if entry.OverlapCount <= 0 then
        overlappingPlayers[id] = nil
    end
end

function Tick(dt)
    dt = dt or 0.0

    for id, entry in pairs(overlappingPlayers) do
        local target = entry.Actor
        if not is_valid_actor(target) then
            overlappingPlayers[id] = nil
        else
            entry.TimeUntilNextDamage = entry.TimeUntilNextDamage - dt

            while entry.TimeUntilNextDamage <= 0.0 do
                if not apply_trap_damage(target) then
                    overlappingPlayers[id] = nil
                    break
                end
                entry.TimeUntilNextDamage = entry.TimeUntilNextDamage + DAMAGE_INTERVAL
            end
        end
    end
end
