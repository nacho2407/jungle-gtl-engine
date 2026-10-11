local CombatEvents = require("Game.CombatEvents")

local beamParticle = nil
local currentHealth = 100.0
local isDead = false

local BEAM_SOURCE_PARAMETER = "BeamSource"
local BEAM_TARGET_PARAMETER = "BeamEnd"

local MAX_HEALTH = 100.0
local BEAM_DAMAGE = 15.0
local MOVE_INTERVAL = 2.0
local MOVE_SPEED = 1.0
local BEAM_ACTIVE_DURATION = 1.0
local BEAM_RANGE = 500.0
local SOURCE_HEIGHT = 60.0

local moveTimer = 0.0
local beamTimer = 0.0
local moveDirection = Vector.new(1, 0, 0)
local currentBeamHitActor = nil
local currentBeamHitNormal = nil

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function atan2(y, x)
    if math.atan2 ~= nil then
        return math.atan2(y, x)
    end

    if x > 0.0 then
        return math.atan(y / x)
    elseif x < 0.0 and y >= 0.0 then
        return math.atan(y / x) + math.pi
    elseif x < 0.0 and y < 0.0 then
        return math.atan(y / x) - math.pi
    elseif x == 0.0 and y > 0.0 then
        return math.pi * 0.5
    elseif x == 0.0 and y < 0.0 then
        return -math.pi * 0.5
    end

    return 0.0
end

local function find_beam_particle()
    -- EnergyBullet은 적군 유닛에 직접 붙은 ParticleSystemComponent를 사용한다.
    -- 전역 "gun" 태그 Actor를 찾으면 여러 적이 하나의 빔 컴포넌트를 공유할 수 있으므로 사용하지 않는다.
    beamParticle = obj:GetParticleSystem()
end

local function get_beam_source_location()
    return obj.Location + Vector.new(0, 0, SOURCE_HEIGHT)
end

local function get_random_xy_direction()
    local angle = math.random() * math.pi * 2.0
    return Vector.new(math.cos(angle), math.sin(angle), 0.0):Normalized()
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

local function update_facing_from_move_direction()
    local yawDegrees = atan2(moveDirection.Y, moveDirection.X) * 180.0 / math.pi
    obj.Rotation = Vector.new(0.0, 0.0, yawDegrees)
end

local function stop_beam()
    beamTimer = 0.0
    if beamParticle == nil then
        find_beam_particle()
    end

    if beamParticle == nil then
        return
    end

    beamParticle:SetEmitterSpawningEnabled(false)
    beamParticle:Deactivate()
end

local function raycast_beam_target(sourcePos, direction)
    currentBeamHitActor = nil
    currentBeamHitNormal = nil

    local hit = World.RaycastPrimitive(sourcePos, direction, BEAM_RANGE, obj)
    if hit ~= nil then
        currentBeamHitActor = hit.HitActor
        currentBeamHitNormal = hit.WorldNormal or hit.ImpactNormal
        return hit.WorldHitLocation
    end

    return sourcePos + direction * BEAM_RANGE
end

local function start_beam()
    if beamParticle == nil then
        find_beam_particle()
    end

    if beamParticle == nil then
        return
    end

    local sourcePos = get_beam_source_location()
    local direction = obj.Forward:Normalized()
    local targetPos = raycast_beam_target(sourcePos, direction)

    beamParticle:SetVectorParameter(BEAM_SOURCE_PARAMETER, sourcePos)
    beamParticle:SetVectorParameter(BEAM_TARGET_PARAMETER, targetPos)

    beamParticle:ResetSystem()
    beamParticle:SetEmitterSpawningEnabled(true)
    beamParticle:Activate()

    beamTimer = BEAM_ACTIVE_DURATION

    if currentBeamHitActor ~= nil then
        local impactContext = CombatEvents.MakeDamageContext({
            Instigator = obj,
            DamageCauser = obj,
            HitActor = currentBeamHitActor,
            HitLocation = targetPos,
            HitNormal = currentBeamHitNormal,
            ShotDirection = direction,
            Damage = BEAM_DAMAGE,
            DamageType = "Beam",
        })
        CombatEvents.NotifyAttackImpact(obj, impactContext)
    end

    if currentBeamHitActor ~= nil and CombatEvents.IsDamageable(currentBeamHitActor) then
        local damageContext = CombatEvents.MakeDamageContext({
            Instigator = obj,
            DamageCauser = obj,
            HitActor = currentBeamHitActor,
            HitLocation = targetPos,
            HitNormal = currentBeamHitNormal,
            ShotDirection = direction,
            Damage = BEAM_DAMAGE,
            DamageType = "Beam",
        })

        CombatEvents.NotifyAttackFired(obj, damageContext)
        CombatEvents.ApplyDamageAndNotify(obj, currentBeamHitActor, damageContext)
    end
end

local function choose_new_move_and_fire()
    moveDirection = get_random_xy_direction()
    update_facing_from_move_direction()
    start_beam()
    moveTimer = MOVE_INTERVAL
end

local function update_movement(dt)
    local delta = moveDirection * (MOVE_SPEED * dt)
    delta.Z = 0.0
    obj:AddWorldOffset(delta)
end

function BeginPlay()
    find_beam_particle()

    currentHealth = MAX_HEALTH
    isDead = false
    moveTimer = 0.0
    beamTimer = 0.0
    moveDirection = get_random_xy_direction()
    currentBeamHitActor = nil
    currentBeamHitNormal = nil

    obj:AddTag("enemy")
    obj:AddTag("drone")
    CombatEvents.RegisterDamageable(obj, {
        ApplyDamage = apply_enemy_damage,
        IsDead = function() return isDead end,
    })

    if beamParticle ~= nil then
        beamParticle:Deactivate()
    end
    choose_new_move_and_fire()
end

function EndPlay()
    CombatEvents.UnregisterDamageable(obj)
    stop_beam()
end

function OnOverlap(OtherActor)
end

function OnHit(OtherActor, HitComponent, OtherComp, NormalImpulse, HitResult)
end

function Tick(dt)
    if isDead then return end

    update_movement(dt)

    moveTimer = moveTimer - dt
    if moveTimer <= 0.0 then
        choose_new_move_and_fire()
    end

    if beamTimer > 0.0 then
        beamTimer = beamTimer - dt
        if beamTimer <= 0.0 then
            stop_beam()
        end
    end
end
