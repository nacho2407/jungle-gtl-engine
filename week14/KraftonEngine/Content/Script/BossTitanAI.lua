local CombatEvents = require("Game.CombatEvents")

local PLAYER_TAG = "player"

local MAX_HEALTH = 650.0
local THINK_INTERVAL = 0.12
local SIGHT_RANGE = 80.0
local KEEP_RANGE = 28.0
local MIN_RANGE = 9.0
local FIRE_MIN_RANGE = 14.0
local CLOSE_RETREAT_RANGE = 12.0
local MELEE_RANGE = 10.5
local CANNON_RANGE = 58.0
local TARGET_HEIGHT = 1.6
local FALLBACK_MUZZLE_HEIGHT = 7.0
local MELEE_MAX_VERTICAL_DELTA = 5.5
local MELEE_ACTION_LOCK = 2.35
local MELEE_HIT_DELAY = 0.72
local POWER_SHOT_ACTION_LOCK = 1.25
local CANNON_ACTION_LOCK = 0.85
local RETREAT_ACTION_LOCK = 0.55
local MELEE_KNOCKBACK_DISTANCE = 5.2
local MELEE_KNOCKBACK_DURATION = 0.22
local MUZZLE_BONE = "ja_c_propGun"
local MUZZLE_LOCAL_OFFSET = Vector.new(0.0, 0.0, 0.0)

local ANIM_ROOT = "Content/Data/Boss/Heavy/Animations/"
local ANIM = {
    idle = ANIM_ROOT .. "a_IDLE_combat_deadbolt_titan_torso_d_lod0.uasset",
    walk = ANIM_ROOT .. "a_WalkCenter_deadbolt_titan_torso_d_lod0.uasset",
    run = ANIM_ROOT .. "a_combat_run_F_deadbolt_titan_torso_d_lod0.uasset",
    strafeLeft = ANIM_ROOT .. "a_combat_walk_L_deadbolt_titan_torso_d_lod0.uasset",
    strafeRight = ANIM_ROOT .. "a_combat_walk_R_deadbolt_titan_torso_d_lod0.uasset",
    cannon = ANIM_ROOT .. "a_Fire_auto_deadbolt_titan_torso_d_lod0.uasset",
    powerShot = ANIM_ROOT .. "htLegion_MP_Stand_PowerShot_deadbolt_titan_torso_d_lod0.uasset",
    melee = ANIM_ROOT .. "at_elite_melee_low_stomp_F_deadbolt_titan_torso_d_lod0.uasset",
    retreat = ANIM_ROOT .. "a_bound_back_deadbolt_titan_torso_d_lod0.uasset",
    phase = ANIM_ROOT .. "a_Legion_gunup_deadbolt_titan_torso_d_lod0.uasset",
    hitReact = ANIM_ROOT .. "at_combat_start_react_deadbolt_titan_torso_d_lod0.uasset",
}

local targetActor = nil
local mesh = nil
local currentHealth = MAX_HEALTH
local isDead = false
local deathTimer = 0.0

local phase = 1
local pendingPhase = nil
local thinkTimer = 0.0
local actionTimer = 0.0
local animationLockTimer = 0.0
local phaseLockTimer = 0.0
local strafeFlipTimer = 0.0
local strafeSign = 1.0
local currentAnim = nil
local homeZ = nil

local cooldowns = {
    cannon = 0.0,
    powerShot = 1.4,
    melee = 0.0,
    retreat = 0.0,
}

local activeAttack = nil

local function is_animation_locked()
    return animationLockTimer > 0.0
end

local function is_retreating()
    return actionTimer > 0.0 and currentAnim == ANIM.retreat
end

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function health_ratio()
    if MAX_HEALTH <= 0.0 then return 0.0 end
    return clamp(currentHealth / MAX_HEALTH, 0.0, 1.0)
end

local function horizontal_delta(fromPos, toPos)
    return Vector.new(toPos.X - fromPos.X, toPos.Y - fromPos.Y, 0.0)
end

local function horizontal_distance(a, b)
    return horizontal_delta(a, b):Length()
end

local function normalized_or_zero(v)
    local length = v:Length()
    if length <= 0.0001 then
        return Vector.new(0.0, 0.0, 0.0)
    end
    return v / length
end

local function random01()
    return math.random()
end

local function atan2(y, x)
    if x > 0.0 then
        return math.atan(y / x)
    end
    if x < 0.0 and y >= 0.0 then
        return math.atan(y / x) + math.pi
    end
    if x < 0.0 and y < 0.0 then
        return math.atan(y / x) - math.pi
    end
    if y > 0.0 then
        return math.pi * 0.5
    end
    if y < 0.0 then
        return -math.pi * 0.5
    end
    return 0.0
end

local function play_anim(path, looping, force)
    if mesh == nil or path == nil then return end
    if not force and is_animation_locked() then return end
    if not force and currentAnim == path then return end

    if mesh:PlayAnimationByPath(path, looping == true) then
        currentAnim = path
    end
end

local function find_target()
    if targetActor ~= nil and targetActor:IsValid() then
        return targetActor
    end

    targetActor = World.FindFirstActorByTag(PLAYER_TAG)
    return targetActor
end

local function get_muzzle_location()
    if mesh ~= nil then
        local muzzle = mesh:GetBoneSocketLocation(MUZZLE_BONE, MUZZLE_LOCAL_OFFSET)
        if muzzle ~= nil and muzzle:Length() > 0.001 then
            return muzzle
        end
    end

    return obj.Location + Vector.new(0.0, 0.0, FALLBACK_MUZZLE_HEIGHT)
end

local function get_target_aim_location(target)
    if target == nil then return obj.Location end

    local camera = nil
    if type(target.GetCamera) == "function" then
        camera = target:GetCamera()
    end

    if camera ~= nil and type(camera.GetLocation) == "function" then
        local cameraLocation = camera:GetLocation()
        if cameraLocation ~= nil then
            return cameraLocation
        end
    end

    if camera ~= nil and camera.Location ~= nil then
        return camera.Location
    end

    return target.Location + Vector.new(0.0, 0.0, TARGET_HEIGHT)
end

local function has_line_of_sight(target)
    if target == nil then return false end

    local source = get_muzzle_location()
    local targetPos = get_target_aim_location(target)
    local toTarget = targetPos - source
    local distance = toTarget:Length()
    if distance <= 0.001 or distance > SIGHT_RANGE then
        return false
    end

    local hit = World.RaycastWorldStatic(source, toTarget:Normalized(), distance, obj)
    return hit == nil
end

local function face_target(target)
    if target == nil then return end

    local delta = horizontal_delta(obj.Location, target.Location)
    if delta:Length() <= 0.001 then return end

    local yaw = math.deg(atan2(delta.Y, delta.X))
    local rot = obj.Rotation
    obj.Rotation = Vector.new(rot.X, rot.Y, yaw)
end

local function move_horizontal(direction, speed, dt)
    local dir = normalized_or_zero(direction)
    if dir:Length() <= 0.001 then return end

    local nextLocation = obj.Location + dir * (speed * dt)
    if homeZ ~= nil then
        nextLocation.Z = homeZ
    end
    obj.Location = nextLocation
end

local function make_damage_result(applied, amount, killed)
    return {
        bApplied = applied == true,
        bKilled = killed == true,
        bCritical = false,
        DamageApplied = amount or 0.0,
        RemainingHealth = currentHealth,
        MaxHealth = MAX_HEALTH,
        HealthRatio = health_ratio(),
        Victim = obj,
    }
end

local function enter_phase(nextPhase)
    if nextPhase <= phase then return end

    if activeAttack ~= nil or is_animation_locked() then
        pendingPhase = math.max(pendingPhase or nextPhase, nextPhase)
        return
    end

    phase = nextPhase
    pendingPhase = nil
    phaseLockTimer = 1.0
    actionTimer = phaseLockTimer
    animationLockTimer = phaseLockTimer
    activeAttack = nil

    cooldowns.cannon = 0.4
    cooldowns.powerShot = phase >= 3 and 0.7 or 1.2
    cooldowns.melee = 0.5

    play_anim(ANIM.phase, false, true)
end

local function update_phase_from_health()
    local ratio = health_ratio()
    if ratio <= 0.35 then
        enter_phase(3)
    elseif ratio <= 0.65 then
        enter_phase(2)
    end
end

local function apply_boss_damage(context)
    local amount = context ~= nil and (context.Damage or context.damage) or 0.0
    if amount <= 0.0 or isDead then
        return make_damage_result(false, 0.0, false)
    end

    currentHealth = clamp(currentHealth - amount, 0.0, MAX_HEALTH)
    local killed = currentHealth <= 0.0
    if killed then
        isDead = true
        deathTimer = 1.4
        activeAttack = nil
        play_anim(ANIM.hitReact, false, true)
    else
        update_phase_from_health()
    end

    return make_damage_result(true, amount, killed)
end

local function start_attack(kind, animPath, duration, hitDelay, range, damage, cooldown, lockDuration)
    activeAttack = {
        kind = kind,
        elapsed = 0.0,
        hitDelay = hitDelay,
        range = range,
        damage = damage,
        didHit = false,
    }

    actionTimer = duration
    animationLockTimer = math.max(animationLockTimer, lockDuration or duration)
    cooldowns[kind] = cooldown
    play_anim(animPath, false, true)

    CombatEvents.NotifyAttackFired(obj, {
        Instigator = obj,
        DamageCauser = obj,
        Damage = damage,
        DamageType = kind,
    })
end

local function apply_active_attack_hit()
    if activeAttack == nil or activeAttack.didHit then return end

    local target = find_target()
    if target == nil or not target:IsValid() then return end

    local source = get_muzzle_location()
    local targetPos = get_target_aim_location(target)
    local aimDelta = targetPos - source
    local aimDistance = aimDelta:Length()
    local dist = horizontal_distance(obj.Location, target.Location)
    if activeAttack.kind == "melee" then
        if dist > activeAttack.range then return end
    else
        if aimDistance > activeAttack.range then return end
        if not has_line_of_sight(target) then return end
    end

    local shotDir = normalized_or_zero(aimDelta)
    local horizontalHitDir = normalized_or_zero(horizontal_delta(obj.Location, target.Location))
    local hitLocation = activeAttack.kind == "melee" and target.Location or targetPos

    if activeAttack.kind == "melee" then
        Debug.DrawSphere(hitLocation, 0.45, 255, 80, 30, 0.5, 12)
    else
        Debug.DrawLine(source, targetPos, 255, 70, 20, 0.5)
    end

    if CombatEvents.IsDamageable(target) then
        CombatEvents.ApplyDamageAndNotify(obj, target, {
            Instigator = obj,
            DamageCauser = obj,
            HitActor = target,
            HitLocation = hitLocation,
            ShotDirection = shotDir,
            Damage = activeAttack.damage,
            DamageType = activeAttack.kind,
        })
    end

    if activeAttack.kind == "melee" then
        local action = target:GetActionComponent()
        if action ~= nil then
            action:Knockback(horizontalHitDir, MELEE_KNOCKBACK_DISTANCE, MELEE_KNOCKBACK_DURATION)
        end
    end

    activeAttack.didHit = true
end

local function update_active_attack(dt)
    if activeAttack == nil then return end

    activeAttack.elapsed = activeAttack.elapsed + dt
    if activeAttack.elapsed >= activeAttack.hitDelay then
        apply_active_attack_hit()
    end

    if actionTimer <= 0.0 then
        activeAttack = nil
    end
end

local function apply_pending_phase_if_ready()
    if pendingPhase == nil then return end
    if activeAttack ~= nil or is_animation_locked() then return end

    local nextPhase = pendingPhase
    pendingPhase = nil
    enter_phase(nextPhase)
end

local function tick_timers(dt)
    actionTimer = math.max(0.0, actionTimer - dt)
    animationLockTimer = math.max(0.0, animationLockTimer - dt)
    phaseLockTimer = math.max(0.0, phaseLockTimer - dt)
    strafeFlipTimer = math.max(0.0, strafeFlipTimer - dt)

    for key, value in pairs(cooldowns) do
        cooldowns[key] = math.max(0.0, value - dt)
    end
end

local function build_blackboard(target)
    local dist = horizontal_distance(obj.Location, target.Location)
    local muzzle = get_muzzle_location()
    local aimLocation = get_target_aim_location(target)
    local aimDelta = aimLocation - muzzle
    local verticalDelta = target.Location.Z - obj.Location.Z
    local canGroundMelee = dist <= MELEE_RANGE and math.abs(verticalDelta) <= MELEE_MAX_VERTICAL_DELTA

    return {
        target = target,
        distance = dist,
        aimDistance = aimDelta:Length(),
        verticalDelta = verticalDelta,
        canGroundMelee = canGroundMelee,
        healthRatio = health_ratio(),
        phase = phase,
        lineOfSight = has_line_of_sight(target),
    }
end

local function score_cannon(bb)
    if cooldowns.cannon > 0.0 or not bb.lineOfSight then return -1000.0 end
    if bb.canGroundMelee then return -1000.0 end
    if bb.distance < FIRE_MIN_RANGE and math.abs(bb.verticalDelta) <= MELEE_MAX_VERTICAL_DELTA then return -1000.0 end
    if bb.aimDistance > CANNON_RANGE then return -1000.0 end

    local rangeFit = 1.0 - math.abs(bb.distance - KEEP_RANGE) / KEEP_RANGE
    return 4.0 + rangeFit * 3.0 + phase * 0.7 + random01()
end

local function score_power_shot(bb)
    if cooldowns.powerShot > 0.0 or phase < 2 or not bb.lineOfSight then return -1000.0 end
    if bb.canGroundMelee then return -1000.0 end
    if bb.distance < 18.0 and math.abs(bb.verticalDelta) <= MELEE_MAX_VERTICAL_DELTA then return -1000.0 end
    if bb.aimDistance > CANNON_RANGE then return -1000.0 end

    return 5.0 + phase * 1.4 + (1.0 - bb.healthRatio) * 2.0 + random01()
end

local function score_melee(bb)
    if cooldowns.melee > 0.0 or not bb.canGroundMelee then return -1000.0 end
    return 8.0 + phase * 1.2 + random01()
end

local function score_retreat(bb)
    if cooldowns.retreat > 0.0 or bb.distance > CLOSE_RETREAT_RANGE then return -1000.0 end
    if math.abs(bb.verticalDelta) > MELEE_MAX_VERTICAL_DELTA then return -1000.0 end
    return 7.5 + phase + random01()
end

local function choose_attack(bb)
    local best = {
        name = nil,
        score = -1000.0,
    }

    local candidates = {
        { name = "melee", score = score_melee(bb) },
        { name = "powerShot", score = score_power_shot(bb) },
        { name = "cannon", score = score_cannon(bb) },
        { name = "retreat", score = score_retreat(bb) },
    }

    for _, candidate in ipairs(candidates) do
        if candidate.score > best.score then
            best = candidate
        end
    end

    return best.name
end

local function run_attack(name)
    if name == "melee" then
        start_attack("melee", ANIM.melee, MELEE_ACTION_LOCK, MELEE_HIT_DELAY, MELEE_RANGE + 2.0, 22.0 + phase * 6.0, 1.3, MELEE_ACTION_LOCK)
        return true
    end

    if name == "powerShot" then
        start_attack("powerShot", ANIM.powerShot, POWER_SHOT_ACTION_LOCK, 0.55, CANNON_RANGE, 24.0 + phase * 9.0, phase >= 3 and 2.0 or 2.8, POWER_SHOT_ACTION_LOCK)
        return true
    end

    if name == "cannon" then
        start_attack("cannon", ANIM.cannon, CANNON_ACTION_LOCK, 0.22, CANNON_RANGE, 11.0 + phase * 4.0, phase >= 3 and 0.55 or 0.85, CANNON_ACTION_LOCK)
        return true
    end

    if name == "retreat" then
        cooldowns.retreat = 1.1
        actionTimer = RETREAT_ACTION_LOCK
        animationLockTimer = math.max(animationLockTimer, RETREAT_ACTION_LOCK)
        activeAttack = nil
        play_anim(ANIM.retreat, false, true)
        return true
    end

    return false
end

local function locomote(bb, dt)
    local toTarget = horizontal_delta(obj.Location, bb.target.Location)
    local dirToTarget = normalized_or_zero(toTarget)

    if actionTimer > 0.0 and currentAnim == ANIM.retreat then
        move_horizontal(dirToTarget * -1.0, 14.0 + phase * 2.5, dt)
        return
    end

    if bb.distance > KEEP_RANGE then
        move_horizontal(dirToTarget, phase >= 3 and 8.5 or 6.0, dt)
        play_anim(bb.distance > 40.0 and ANIM.run or ANIM.walk, true, false)
        return
    end

    if bb.distance < MIN_RANGE then
        move_horizontal(dirToTarget * -1.0, 5.5 + phase, dt)
        play_anim(ANIM.retreat, false, false)
        return
    end

    if strafeFlipTimer <= 0.0 then
        strafeSign = random01() < 0.5 and -1.0 or 1.0
        strafeFlipTimer = 1.3 + random01() * 1.2
    end

    local side = Vector.new(-dirToTarget.Y, dirToTarget.X, 0.0) * strafeSign
    move_horizontal(side, 2.5 + phase * 0.7, dt)
    play_anim(strafeSign > 0.0 and ANIM.strafeRight or ANIM.strafeLeft, true, false)
end

function BeginPlay()
    mesh = obj:GetSkeletalMesh()
    homeZ = obj.Location.Z
    currentHealth = MAX_HEALTH
    isDead = false
    deathTimer = 0.0
    phase = 1
    pendingPhase = nil
    actionTimer = 0.0
    animationLockTimer = 0.0
    phaseLockTimer = 0.0
    thinkTimer = 0.0
    activeAttack = nil
    currentAnim = nil

    obj:AddTag("enemy")
    obj:AddTag("boss")
    CombatEvents.RegisterDamageable(obj, {
        ApplyDamage = apply_boss_damage,
        IsDead = function() return isDead end,
    })

    play_anim(ANIM.idle, true, true)
    print("[BossTitanAI] Titan boss online.")
end

function EndPlay()
    CombatEvents.UnregisterDamageable(obj)
end

function Tick(dt)
    if isDead then
        deathTimer = deathTimer - dt
        if deathTimer <= 0.0 then
            obj:Destroy()
        end
        return
    end

    tick_timers(dt)
    update_active_attack(dt)
    apply_pending_phase_if_ready()

    local target = find_target()
    if target == nil then
        play_anim(ANIM.idle, true, false)
        return
    end

    face_target(target)

    if phaseLockTimer > 0.0 then
        return
    end

    thinkTimer = thinkTimer - dt
    local bb = build_blackboard(target)

    if actionTimer <= 0.0 and thinkTimer <= 0.0 then
        thinkTimer = THINK_INTERVAL
        run_attack(choose_attack(bb))
    end

    if activeAttack == nil and (not is_animation_locked() or is_retreating()) then
        locomote(bb, dt)
    end
end
