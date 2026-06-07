local CombatEvents = require("Game.CombatEvents")

local targetActor = nil
local beamParticle = nil
local currentHealth = 100.0
local isDead = false

local PLAYER_TAG = "player"
local BEAM_SOURCE_PARAMETER = "BeamSource"
local BEAM_TARGET_PARAMETER = "BeamEnd"

local MAX_HEALTH = 100.0
local BEAM_DAMAGE = 15.0
local FIRE_INTERVAL = 1.0
local SIGHT_RANGE = 900.0
local FOV_DOT = 0.55
local SOURCE_HEIGHT = 60.0
local TARGET_HEIGHT = 60.0
local MIN_FIRE_DISTANCE = 20.0

local fireTimer = 0.0

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function find_target()
    targetActor = World.FindFirstActorByTag(PLAYER_TAG)
end

local function find_beam_particle()
    -- EnergyBullet은 적군 유닛에 직접 붙은 ParticleSystemComponent를 사용한다.
    -- 전역 "gun" 태그 Actor를 찾으면 여러 적이 하나의 빔 컴포넌트를 공유할 수 있으므로 사용하지 않는다.
    beamParticle = obj:GetParticleSystem()
end

local function get_beam_source_location()
    return obj.Location + Vector.new(0, 0, SOURCE_HEIGHT)
end

local function get_beam_target_location()
    if targetActor == nil then
        return nil
    end

    return targetActor.Location + Vector.new(0, 0, TARGET_HEIGHT)
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
        if beamParticle ~= nil then
            beamParticle:Deactivate()
        end
        obj:Destroy()
    end
    return result
end

local function is_target_visible(sourcePos, targetPos)
    local toTarget = targetPos - sourcePos
    local distance = toTarget:Length()
    if distance < MIN_FIRE_DISTANCE or distance > SIGHT_RANGE then
        return false
    end

    local dir = toTarget:Normalized()
    local forward = obj.Forward:Normalized()
    if forward:Dot(dir) < FOV_DOT then
        return false
    end

    local wallHit = World.RaycastWorldStatic(sourcePos, dir, distance, obj)
    return wallHit == nil
end

local function get_visible_beam_endpoints()
    if targetActor == nil or not targetActor:IsValid() then
        find_target()
    end

    if targetActor == nil then
        return nil, nil
    end

    local sourcePos = get_beam_source_location()
    local targetPos = get_beam_target_location()
    if targetPos == nil then
        return nil, nil
    end

    if not is_target_visible(sourcePos, targetPos) then
        return nil, nil
    end

    return sourcePos, targetPos
end

local function fire_beam_one_shot(sourcePos, targetPos)
    if beamParticle == nil then
        find_beam_particle()
    end

    if beamParticle == nil then
        return
    end

    beamParticle:SetVectorParameter(BEAM_SOURCE_PARAMETER, sourcePos)
    beamParticle:SetVectorParameter(BEAM_TARGET_PARAMETER, targetPos)

    -- One-shot 재생. 유지형 빔이 아니므로 Tick 중 Source/Target을 계속 갱신하지 않는다.
    -- 파티클 에셋의 EmitterDuration / bLooping 설정으로 자연 종료시킨다.
    beamParticle:ResetSystem()
    beamParticle:SetEmitterSpawningEnabled(true)
    beamParticle:Activate()
end

function BeginPlay()
    find_target()
    find_beam_particle()

    currentHealth = MAX_HEALTH
    isDead = false
    fireTimer = 0.0

    obj:AddTag("enemy")
    CombatEvents.RegisterDamageable(obj, {
        ApplyDamage = apply_enemy_damage,
        IsDead = function() return isDead end,
    })

    if beamParticle ~= nil then
        beamParticle:Deactivate()
    end
end

function EndPlay()
    CombatEvents.UnregisterDamageable(obj)
    if beamParticle ~= nil then
        beamParticle:Deactivate()
    end
end

function OnOverlap(OtherActor)
end

function OnHit(OtherActor, HitComponent, OtherComp, NormalImpulse, HitResult)
end

function Tick(dt)
    if isDead then return end

    local sourcePos, targetPos = get_visible_beam_endpoints()
    if sourcePos == nil or targetPos == nil then
        fireTimer = 0.0
        return
    end

    fireTimer = fireTimer - dt
    if fireTimer > 0.0 then
        return
    end

    local damageContext = CombatEvents.MakeDamageContext({
        Instigator = obj,
        DamageCauser = obj,
        HitActor = targetActor,
        HitLocation = targetPos,
        ShotDirection = (targetPos - sourcePos):Normalized(),
        Damage = BEAM_DAMAGE,
        DamageType = "Beam",
    })

    CombatEvents.NotifyAttackFired(obj, damageContext)
    fire_beam_one_shot(sourcePos, targetPos)
    if CombatEvents.IsDamageable(targetActor) then
        CombatEvents.ApplyDamageAndNotify(obj, targetActor, damageContext)
    end
    fireTimer = FIRE_INTERVAL
end
