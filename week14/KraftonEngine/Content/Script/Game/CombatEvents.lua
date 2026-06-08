local EventBus = require("Game.LuaEventBus")

local CombatEvents = {
    _damageables = {},
    _attackReceivers = {},
}

CombatEvents.Events = {
    AttackFired = "Combat.AttackFired",
    AttackImpact = "Combat.AttackImpact",
    AttackHit = "Combat.AttackHit",
    AttackKilled = "Combat.AttackKilled",
    Damaged = "Combat.Damaged",
    Death = "Combat.Death",
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

local function actor_id(actor)
    if not is_valid_actor(actor) then
        return nil
    end
    return actor.UUID
end

local function is_player_actor(actor)
    if not is_valid_actor(actor) or Game == nil or Game.GetPlayerPawn == nil then
        return false
    end

    return actor == Game.GetPlayerPawn()
end

local function actor_has_tag(actor, tag)
    return is_valid_actor(actor)
        and type(actor.HasTag) == "function"
        and actor:HasTag(tag)
end

local function is_boss_result(result)
    if result == nil then
        return false
    end

    if result.bBoss == true or result.boss == true or result.IsBoss == true or result.isBoss == true then
        return true
    end

    return actor_has_tag(result.Victim or result.victim, "boss")
end

local function add_score(methodName, ...)
    if ScoreManager == nil then
        return
    end

    local method = ScoreManager[methodName]
    if method ~= nil then
        method(...)
    end
end

local function call(callback, ...)
    if callback ~= nil then
        return callback(...)
    end
    return nil
end

local function normalize_result(result, targetActor)
    result = result or {}

    local applied = result.bApplied
    if applied == nil then applied = result.applied end
    applied = applied == true

    local killed = result.bKilled
    if killed == nil then killed = result.killed end
    killed = killed == true

    local critical = result.bCritical
    if critical == nil then critical = result.critical end
    critical = critical == true

    local damageApplied = result.DamageApplied or result.damageApplied or 0.0
    local remainingHealth = result.RemainingHealth or result.remainingHealth or 0.0
    local maxHealth = result.MaxHealth or result.maxHealth or 0.0
    local healthRatio = result.HealthRatio or result.healthRatio
    if healthRatio == nil and maxHealth > 0.0 then
        healthRatio = remainingHealth / maxHealth
    end

    result.bApplied = applied
    result.applied = applied
    result.bKilled = killed
    result.killed = killed
    result.bCritical = critical
    result.critical = critical
    result.DamageApplied = damageApplied
    result.damageApplied = damageApplied
    result.RemainingHealth = remainingHealth
    result.remainingHealth = remainingHealth
    result.MaxHealth = maxHealth
    result.maxHealth = maxHealth
    result.HealthRatio = healthRatio
    result.healthRatio = healthRatio
    result.Victim = result.Victim or result.victim or targetActor
    result.victim = result.Victim

    return result
end

function CombatEvents.MakeDamageContext(fields)
    fields = fields or {}
    return {
        Instigator = fields.Instigator or fields.instigator,
        DamageCauser = fields.DamageCauser or fields.damageCauser,
        HitActor = fields.HitActor or fields.hitActor,
        HitLocation = fields.HitLocation or fields.hitLocation,
        HitNormal = fields.HitNormal or fields.hitNormal,
        ShotDirection = fields.ShotDirection or fields.shotDirection,
        Damage = fields.Damage or fields.damage or 0.0,
        DamageType = fields.DamageType or fields.damageType or "Generic",
        bCritical = fields.bCritical or fields.critical or false,
    }
end

function CombatEvents.RegisterDamageable(actor, callbacks)
    local id = actor_id(actor)
    if id == nil then
        return false
    end

    callbacks = callbacks or {}
    CombatEvents._damageables[id] = {
        Actor = actor,
        ApplyDamage = callbacks.ApplyDamage or callbacks.applyDamage,
        IsDead = callbacks.IsDead or callbacks.isDead,
    }
    return true
end

function CombatEvents.UnregisterDamageable(actor)
    local id = actor_id(actor)
    if id ~= nil then
        CombatEvents._damageables[id] = nil
    end
end

function CombatEvents.IsDamageable(actor)
    local id = actor_id(actor)
    return id ~= nil and CombatEvents._damageables[id] ~= nil
end

function CombatEvents.RegisterAttackReceiver(actor, callbacks)
    local id = actor_id(actor)
    if id == nil then
        return false
    end

    callbacks = callbacks or {}
    CombatEvents._attackReceivers[id] = {
        Actor = actor,
        OnAttackFired = callbacks.OnAttackFired or callbacks.onAttackFired,
        OnAttackHit = callbacks.OnAttackHit or callbacks.onAttackHit,
        OnAttackKilled = callbacks.OnAttackKilled or callbacks.onAttackKilled,
    }
    return true
end

function CombatEvents.UnregisterAttackReceiver(actor)
    local id = actor_id(actor)
    if id ~= nil then
        CombatEvents._attackReceivers[id] = nil
    end
end

function CombatEvents.ApplyDamage(targetActor, context)
    local id = actor_id(targetActor)
    local entry = id ~= nil and CombatEvents._damageables[id] or nil
    context = CombatEvents.MakeDamageContext(context)
    context.HitActor = context.HitActor or targetActor

    if entry == nil or entry.ApplyDamage == nil then
        return normalize_result({ bApplied = false, Victim = targetActor }, targetActor)
    end

    local result = normalize_result(entry.ApplyDamage(context), targetActor)
    if result.bApplied then
        if is_player_actor(targetActor) then
            add_score("AddDamageTaken", result.DamageApplied)
        end
        EventBus.Emit(CombatEvents.Events.Damaged, context, result)
    end
    if result.bKilled then
        if is_player_actor(targetActor) then
            add_score("AddDeath", 1)
        end
        EventBus.Emit(CombatEvents.Events.Death, context, result)
    end
    return result
end

function CombatEvents.NotifyAttackFired(attacker, context)
    context = CombatEvents.MakeDamageContext(context)
    context.Instigator = context.Instigator or attacker

    local id = actor_id(attacker)
    local receiver = id ~= nil and CombatEvents._attackReceivers[id] or nil
    call(receiver ~= nil and receiver.OnAttackFired or nil, context)
    if is_player_actor(attacker) then
        add_score("AddShotFired", 1)
    end
    EventBus.Emit(CombatEvents.Events.AttackFired, context)
end

function CombatEvents.NotifyAttackImpact(attacker, context)
    context = CombatEvents.MakeDamageContext(context)
    context.Instigator = context.Instigator or attacker

    EventBus.Emit(CombatEvents.Events.AttackImpact, context)
end

function CombatEvents.NotifyAttackResult(attacker, context, result)
    context = CombatEvents.MakeDamageContext(context)
    context.Instigator = context.Instigator or attacker
    result = normalize_result(result, context.HitActor)

    if not result.bApplied then
        return result
    end

    local id = actor_id(attacker)
    local receiver = id ~= nil and CombatEvents._attackReceivers[id] or nil
    call(receiver ~= nil and receiver.OnAttackHit or nil, context, result)
    if is_player_actor(attacker) then
        add_score("AddShotHit", 1)
        add_score("AddDamageDealt", result.DamageApplied)
    end
    EventBus.Emit(CombatEvents.Events.AttackHit, context, result)

    if result.bKilled then
        call(receiver ~= nil and receiver.OnAttackKilled or nil, context, result)
        if is_player_actor(attacker) then
            if is_boss_result(result) then
                add_score("AddBossKill", 1)
            else
                add_score("AddEnemyKill", 1)
            end
        end
        EventBus.Emit(CombatEvents.Events.AttackKilled, context, result)
    end

    return result
end

function CombatEvents.ApplyDamageAndNotify(attacker, targetActor, context)
    context = CombatEvents.MakeDamageContext(context)
    context.Instigator = context.Instigator or attacker
    context.HitActor = context.HitActor or targetActor

    local result = CombatEvents.ApplyDamage(targetActor, context)
    return CombatEvents.NotifyAttackResult(attacker, context, result)
end

return CombatEvents
