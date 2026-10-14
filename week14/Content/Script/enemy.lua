local CombatEvents = require("Game.CombatEvents")

local gunActor = nil
local gunParticle = nil
local currentHealth = 100.0
local isDead = false

local GUN_TAG = "gun"
local BEAM_TARGET_PARAMETER = "BeamEnd"
local MAX_HEALTH = 100.0

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function find_gun_particle()
    gunActor = World.FindFirstActorByTag(GUN_TAG)
    if gunActor == nil then
        gunParticle = nil
        return
    end

    gunParticle = gunActor:GetParticleSystem()
end

local function update_beam_target()
    if gunParticle == nil then
        find_gun_particle()
    end

    if gunParticle == nil then
        return
    end

    gunParticle:SetVectorParameter(BEAM_TARGET_PARAMETER, obj.Location)
end

local function make_damage_result(applied, damageApplied, killed)
    return {
        bApplied = applied == true,
        bKilled = killed == true,
        bCritical = false,
        DamageApplied = damageApplied or 0.0,
        RemainingHealth = currentHealth,
        MaxHealth = MAX_HEALTH,
        HealthRatio = MAX_HEALTH > 0.0 and currentHealth / MAX_HEALTH or 0.0,
        Victim = obj,
    }
end

local function apply_enemy_damage(context)
    local amount = context ~= nil and (context.Damage or context.damage) or 0.0
    if amount <= 0.0 or isDead then
        return make_damage_result(false, 0.0, false)
    end

    currentHealth = clamp(currentHealth - amount, 0.0, MAX_HEALTH)
    local killed = currentHealth <= 0.0
    if killed then
        isDead = true
    end

    local result = make_damage_result(true, amount, killed)
    if killed then
        obj:Destroy()
    end
    return result
end

function BeginPlay()
    currentHealth = MAX_HEALTH
    isDead = false
    obj:AddTag("enemy")
    CombatEvents.RegisterDamageable(obj, {
        ApplyDamage = apply_enemy_damage,
        IsDead = function() return isDead end,
    })
    find_gun_particle()
end

function EndPlay()
    CombatEvents.UnregisterDamageable(obj)
    print("[Enemy EndPlay] " .. obj.Name)
end

function OnOverlap(OtherActor)
    update_beam_target()
end

function OnHit(OtherActor, HitComponent, OtherComp, NormalImpulse, HitResult)
    update_beam_target()
end

function Tick(dt)
end
