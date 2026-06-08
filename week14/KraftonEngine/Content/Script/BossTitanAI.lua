local CombatEvents = require("Game.CombatEvents")

local PLAYER_TAG = "player"

local MAX_HEALTH = 650.0
local THINK_INTERVAL = 0.12
local SIGHT_RANGE = 80.0
local OPENING_WALK_END_RANGE = 64.0
local OPENING_WALK_SPEED = 2.6
local OPENING_WALK_ACTION_DURATION = 999.0
local KEEP_RANGE = 28.0
local MIN_RANGE = 9.0
local FIRE_MIN_RANGE = 14.0
local CLOSE_RETREAT_RANGE = 12.0
local MELEE_RANGE = 10.5
local CANNON_RANGE = 58.0
local APPROACH_RUN_SPEED = 5.2
local APPROACH_RUN_SPEED_PHASE3 = 7.4
local APPROACH_RUN_DISTANCE = 40.0
local TARGET_HEIGHT = 1.6
local FALLBACK_MUZZLE_HEIGHT = 7.0
local MELEE_MAX_VERTICAL_DELTA = 5.5
local MELEE_ACTION_LOCK = 3.8
local MELEE_HIT_DELAY = 0.72
local POWER_SHOT_ACTION_LOCK = 1.25
local CANNON_ACTION_LOCK = 0.85
local RETREAT_ACTION_LOCK = 0.55
local MELEE_KNOCKBACK_DISTANCE = 5.2
local MELEE_KNOCKBACK_DURATION = 0.22
local MUZZLE_BONE = "ja_c_propGun"
local MUZZLE_LOCAL_OFFSET = Vector.new(0.0, 0.0, 0.0)
local TACTIC_REEVALUATE_INTERVAL = 0.25
local APPROACH_COMMIT_TIME = 0.75
local DUEL_COMMIT_TIME = 1.1
local CLOSE_COMBAT_COMMIT_TIME = 0.6
local LEAP_MIN_RANGE = 34.0
local LEAP_MAX_RANGE = 64.0
local LEAP_MAX_VERTICAL_DELTA = 8.5
local LEAP_LANDING_OFFSET = 8.0
local LEAP_WINDUP_TIME = 0.55
local LEAP_FLIGHT_TIME = 0.92
local LEAP_LAND_TIME = 0.7
local LEAP_ARC_HEIGHT = 11.0
local LEAP_COOLDOWN = 7.5
local LEAP_LAND_DAMAGE_RADIUS = 10.5
local LEAP_LAND_SHAKE_RADIUS = 28.0
local LEAP_LAND_SHAKE_MAX_VERTICAL_DELTA = 12.0
local LEAP_LAND_DAMAGE = 22.0
local LEAP_LAND_SHAKE_SCALE = 1.35
local LEAP_KNOCKBACK_DISTANCE = 6.0
local LEAP_KNOCKBACK_DURATION = 0.28
local AIM_PITCH_MIN = -35.0
local AIM_PITCH_MAX = 40.0

local MOVE_IDLE = 0
local MOVE_WALK = 1
local MOVE_RUN = 2
local MOVE_STRAFE_LEFT = 3
local MOVE_STRAFE_RIGHT = 4
local MOVE_RETREAT = 5
local MOVE_LEAP_FLOAT = 6

local TACTIC = {
    openingWalk = "openingWalk",
    approach = "approach",
    duel = "duel",
    closeCombat = "closeCombat",
    leapEngage = "leapEngage",
}

local ANIM_ROOT = "Content/Data/Boss/Heavy/Animations/"
local ANIM = {
    idle = ANIM_ROOT .. "a_IDLE_combat_deadbolt_titan_torso_d_lod0.uasset",
    walk = ANIM_ROOT .. "a_WalkN_deadbolt_titan_torso_d_lod0.uasset",
    run = ANIM_ROOT .. "a_Run_deadbolt_titan_torso_d_lod0.uasset",
    strafeLeft = ANIM_ROOT .. "a_combat_walk_L_deadbolt_titan_torso_d_lod0.uasset",
    strafeRight = ANIM_ROOT .. "a_combat_walk_R_deadbolt_titan_torso_d_lod0.uasset",
    cannon = ANIM_ROOT .. "a_Fire_auto_deadbolt_titan_torso_d_lod0.uasset",
    powerShot = ANIM_ROOT .. "htLegion_MP_Stand_PowerShot_deadbolt_titan_torso_d_lod0.uasset",
    melee = ANIM_ROOT .. "at_elite_melee_low_stomp_F_deadbolt_titan_torso_d_lod0.uasset",
    retreat = ANIM_ROOT .. "a_bound_back_deadbolt_titan_torso_d_lod0.uasset",
    leapStart = ANIM_ROOT .. "a_MP_Jump_start_deadbolt_titan_torso_d_lod0.uasset",
    leapFloat = ANIM_ROOT .. "a_MP_Jump_float_deadbolt_titan_torso_d_lod0.uasset",
    leapLand = ANIM_ROOT .. "a_traverse_land_A_deadbolt_titan_torso_d_lod0.uasset",
    phase = ANIM_ROOT .. "a_Legion_gunup_deadbolt_titan_torso_d_lod0.uasset",
    hitReact = ANIM_ROOT .. "at_combat_start_react_deadbolt_titan_torso_d_lod0.uasset",
}

local MOVE_BY_ANIM_PATH = {
    [ANIM.idle] = MOVE_IDLE,
    [ANIM.walk] = MOVE_WALK,
    [ANIM.run] = MOVE_RUN,
    [ANIM.strafeLeft] = MOVE_STRAFE_LEFT,
    [ANIM.strafeRight] = MOVE_STRAFE_RIGHT,
    [ANIM.retreat] = MOVE_RETREAT,
    [ANIM.leapFloat] = MOVE_LEAP_FLOAT,
}

local ACTION_BY_ANIM_PATH = {
    [ANIM.cannon] = "cannon",
    [ANIM.powerShot] = "powerShot",
    [ANIM.melee] = "melee",
    [ANIM.retreat] = "retreat",
    [ANIM.leapStart] = "leapStart",
    [ANIM.leapLand] = "leapLand",
    [ANIM.phase] = "phase",
    [ANIM.hitReact] = "hitReact",
}

local ACTION_DURATIONS = {
    cannon = CANNON_ACTION_LOCK,
    powerShot = POWER_SHOT_ACTION_LOCK,
    melee = MELEE_ACTION_LOCK,
    retreat = RETREAT_ACTION_LOCK,
    leapStart = LEAP_WINDUP_TIME,
    leapLand = LEAP_LAND_TIME,
    phase = 1.0,
    hitReact = 1.4,
}

local DEBUG_LOG_INTERVAL = 0.4
local debugLogTimer = 0.0
local DEBUG_LOG_PATH = "boss_log.txt"
local debugStartTime = 0.0
local debugSessionTime = 0.0
local debugLogWriteFailed = false

local function debug_buffer_preopen(line)
    if _G.BossLogPreOpenLines == nil then
        _G.BossLogPreOpenLines = {}
    end
    table.insert(_G.BossLogPreOpenLines, line)
    _G.BossLogPreOpened = true
end

local function debug_open_log()
    _G.BossLogPath = DEBUG_LOG_PATH
    if _G.BossLogSessionOpen ~= true then
        if DebugFile ~= nil and DebugFile.WriteText ~= nil then
            local text = "=== Boss log session start ===\n"
            local preOpenLines = _G.BossLogPreOpenLines
            if preOpenLines ~= nil then
                for i = 1, #preOpenLines do
                    text = text .. preOpenLines[i] .. "\n"
                end
            end

            if DebugFile.WriteText(DEBUG_LOG_PATH, text) then
                _G.BossLogSessionOpen = true
                _G.BossLogPreOpened = false
                _G.BossLogPreOpenLines = nil
                debugLogWriteFailed = false
                if DebugFile.GetLogPath ~= nil then
                    print("[BossLog] writing to " .. DebugFile.GetLogPath(DEBUG_LOG_PATH))
                end
            else
                print("[BossLog] failed to open " .. DEBUG_LOG_PATH)
            end
        else
            print("[BossLog] DebugFile API is not available.")
        end
    end
end

local function debug_close_log()
    if _G.BossLogSessionOpen == true and DebugFile ~= nil and DebugFile.AppendText ~= nil then
        DebugFile.AppendText(DEBUG_LOG_PATH, "=== Boss log session end ===\n")
    end
    _G.BossLogSessionOpen = false
    _G.BossLogPreOpened = false
    _G.BossLogPreOpenLines = nil
    _G.BossDebugLog = nil
end

local function debug_log(line)
    print(line)
    if DebugFile ~= nil and DebugFile.AppendText ~= nil then
        if _G.BossLogSessionOpen ~= true then
            debug_buffer_preopen(line)
            return
        end
        if not DebugFile.AppendText(DEBUG_LOG_PATH, line .. "\n") and not debugLogWriteFailed then
            debugLogWriteFailed = true
            print("[BossLog] append failed: " .. DEBUG_LOG_PATH)
        end
    end
end
_G.BossDebugLog = debug_log

local MOVE_NAMES = {
    [MOVE_IDLE] = "IDLE",
    [MOVE_WALK] = "WALK",
    [MOVE_RUN] = "RUN",
    [MOVE_STRAFE_LEFT] = "STRAFE_L",
    [MOVE_STRAFE_RIGHT] = "STRAFE_R",
    [MOVE_RETREAT] = "RETREAT",
    [MOVE_LEAP_FLOAT] = "LEAP_FLOAT",
}

local ANIM_NAMES = {
    [ANIM_ROOT .. "a_IDLE_combat_deadbolt_titan_torso_d_lod0.uasset"] = "idle",
    [ANIM_ROOT .. "a_WalkN_deadbolt_titan_torso_d_lod0.uasset"] = "walk",
    [ANIM_ROOT .. "a_Run_deadbolt_titan_torso_d_lod0.uasset"] = "run",
    [ANIM_ROOT .. "a_combat_walk_L_deadbolt_titan_torso_d_lod0.uasset"] = "strafeL",
    [ANIM_ROOT .. "a_combat_walk_R_deadbolt_titan_torso_d_lod0.uasset"] = "strafeR",
    [ANIM_ROOT .. "a_bound_back_deadbolt_titan_torso_d_lod0.uasset"] = "retreat",
    [ANIM_ROOT .. "a_MP_Jump_float_deadbolt_titan_torso_d_lod0.uasset"] = "leapFloat",
    [ANIM_ROOT .. "a_MP_Jump_start_deadbolt_titan_torso_d_lod0.uasset"] = "leapStart",
    [ANIM_ROOT .. "a_traverse_land_A_deadbolt_titan_torso_d_lod0.uasset"] = "leapLand",
    [ANIM_ROOT .. "a_Fire_auto_deadbolt_titan_torso_d_lod0.uasset"] = "cannon",
    [ANIM_ROOT .. "htLegion_MP_Stand_PowerShot_deadbolt_titan_torso_d_lod0.uasset"] = "powerShot",
    [ANIM_ROOT .. "at_elite_melee_low_stomp_F_deadbolt_titan_torso_d_lod0.uasset"] = "melee",
    [ANIM_ROOT .. "a_Legion_gunup_deadbolt_titan_torso_d_lod0.uasset"] = "phase",
    [ANIM_ROOT .. "at_combat_start_react_deadbolt_titan_torso_d_lod0.uasset"] = "hitReact",
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
local currentTactic = TACTIC.openingWalk
local tacticCommitTimer = 0.0
local tacticReevaluateTimer = 0.0
local openingWalkActive = true

local cooldowns = {
    cannon = 0.0,
    powerShot = 1.4,
    melee = 0.0,
    retreat = 0.0,
    leap = 2.5,
}

local activeAttack = nil
local leapState = nil

local function is_animation_locked()
    return animationLockTimer > 0.0
end

local function is_retreating()
    return actionTimer > 0.0 and currentAnim == ANIM.retreat
end

local function is_leaping()
    return leapState ~= nil
end

local function tactic_commit_time(name)
    if name == TACTIC.approach then return APPROACH_COMMIT_TIME end
    if name == TACTIC.duel then return DUEL_COMMIT_TIME end
    if name == TACTIC.closeCombat then return CLOSE_COMBAT_COMMIT_TIME end
    if name == TACTIC.leapEngage then
        return LEAP_WINDUP_TIME + LEAP_FLIGHT_TIME + LEAP_LAND_TIME
    end
    return TACTIC_REEVALUATE_INTERVAL
end

local function set_tactic(name, commitTime)
    if name == nil then return end

    local nextCommitTime = commitTime or tactic_commit_time(name)
    if currentTactic ~= name then
        currentTactic = name
        tacticCommitTimer = nextCommitTime
        tacticReevaluateTimer = TACTIC_REEVALUATE_INTERVAL
        return
    end

    currentTactic = name
    tacticCommitTimer = math.max(tacticCommitTimer, nextCommitTime)
    tacticReevaluateTimer = TACTIC_REEVALUATE_INTERVAL
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

local function lerp(a, b, alpha)
    return a + (b - a) * alpha
end

local function smoothstep(alpha)
    local t = clamp(alpha, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)
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

local function get_anim_state()
    if obj == nil then return nil end

    _G.BossTitanAnimState = _G.BossTitanAnimState or {}

    local ownerId = obj.UUID
    local state = _G.BossTitanAnimState[ownerId]
    if state == nil then
        state = {
            MoveState = MOVE_IDLE,
            ActionName = nil,
            ActionDuration = 0.0,
            ActionSerial = 0,
            AimPitch = 0.0,
            AimWeight = 0.0,
        }
        _G.BossTitanAnimState[ownerId] = state
    end

    return state
end

local function reset_anim_state()
    local state = get_anim_state()
    if state == nil then return end

    state.MoveState = MOVE_IDLE
    state.ActionName = nil
    state.ActionDuration = 0.0
    state.ActionSerial = 0
    state.AimPitch = 0.0
    state.AimWeight = 0.0
end

local function set_move_anim(moveState)
    local state = get_anim_state()
    if state == nil then return end

    state.MoveState = moveState
end

local function request_action_anim(name, duration)
    local state = get_anim_state()
    if state == nil or name == nil then return end

    state.ActionName = name
    state.ActionDuration = duration or ACTION_DURATIONS[name] or 1.0
    state.ActionSerial = (state.ActionSerial or 0) + 1
end

local function hold_action_anim(name, duration)
    local state = get_anim_state()
    if state == nil or name == nil then return end

    if state.ActionName ~= name then
        state.ActionSerial = (state.ActionSerial or 0) + 1
    end
    state.ActionName = name
    state.ActionDuration = duration or ACTION_DURATIONS[name] or 1.0
end

local function clear_action_anim(name)
    local state = get_anim_state()
    if state == nil then return end
    if name ~= nil and state.ActionName ~= name then return end

    state.ActionName = nil
    state.ActionDuration = 0.0
end

local function set_aim_anim(pitch, weight)
    local state = get_anim_state()
    if state == nil then return end

    state.AimPitch = clamp(pitch or 0.0, AIM_PITCH_MIN, AIM_PITCH_MAX)
    state.AimWeight = clamp(weight or 0.0, 0.0, 1.0)
end

local function play_anim(path, looping, force)
    if path == nil then return end

    local actionName = ACTION_BY_ANIM_PATH[path]
    if force and actionName ~= nil then
        request_action_anim(actionName, ACTION_DURATIONS[actionName])
        currentAnim = path
        return
    end

    if not force and is_animation_locked() then return end

    local moveState = MOVE_BY_ANIM_PATH[path]
    if moveState ~= nil then
        set_move_anim(moveState)
        currentAnim = path
        return
    end

    if not force and currentAnim == path then return end

    if actionName ~= nil then
        request_action_anim(actionName, ACTION_DURATIONS[actionName])
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

local function update_anim_aim(target)
    if target == nil or not target:IsValid() then
        set_aim_anim(0.0, 0.0)
        return
    end

    local source = get_muzzle_location()
    local aimLocation = get_target_aim_location(target)
    local aimDelta = aimLocation - source
    local flatDistance = math.sqrt(aimDelta.X * aimDelta.X + aimDelta.Y * aimDelta.Y)
    if flatDistance <= 0.001 then
        set_aim_anim(0.0, 0.0)
        return
    end

    local pitch = -math.deg(atan2(aimDelta.Z, flatDistance))
    local weight = 1.0
    if is_leaping() then
        weight = 0.0
    elseif activeAttack ~= nil and activeAttack.kind == "melee" then
        weight = 0.25
    end

    set_aim_anim(pitch, weight)
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
    leapState = nil

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
        deathTimer = ACTION_DURATIONS.hitReact
        activeAttack = nil
        leapState = nil
        set_aim_anim(0.0, 0.0)
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
    request_action_anim(kind, lockDuration or duration)
    currentAnim = animPath

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

local function calculate_leap_landing(target)
    if target == nil then return obj.Location end

    local awayFromTarget = normalized_or_zero(horizontal_delta(target.Location, obj.Location))
    if awayFromTarget:Length() <= 0.001 then
        awayFromTarget = Vector.new(-1.0, 0.0, 0.0)
    end

    local landing = target.Location + awayFromTarget * LEAP_LANDING_OFFSET
    landing.Z = homeZ or obj.Location.Z
    return landing
end

local function play_leap_landing_camera_shake(distance)
    if CameraManager == nil or CameraManager.StartWaveShake == nil then
        return
    end

    local falloff = 1.0 - clamp(distance / LEAP_LAND_SHAKE_RADIUS, 0.0, 1.0)
    local scale = LEAP_LAND_SHAKE_SCALE * (0.25 + falloff * 0.75)
    CameraManager.StartWaveShake(scale)
end

local function apply_leap_landing_hit()
    local target = find_target()
    if target == nil or not target:IsValid() then return end

    local dist = horizontal_distance(obj.Location, target.Location)
    local verticalDelta = math.abs(target.Location.Z - obj.Location.Z)

    if dist <= LEAP_LAND_SHAKE_RADIUS and verticalDelta <= LEAP_LAND_SHAKE_MAX_VERTICAL_DELTA then
        play_leap_landing_camera_shake(dist)
    end

    if dist > LEAP_LAND_DAMAGE_RADIUS then return end
    if verticalDelta > LEAP_MAX_VERTICAL_DELTA then return end

    local hitDir = normalized_or_zero(horizontal_delta(obj.Location, target.Location))
    local hitLocation = target.Location
    Debug.DrawSphere(obj.Location, LEAP_LAND_DAMAGE_RADIUS, 255, 120, 30, 0.65, 16)
    Debug.DrawSphere(obj.Location, LEAP_LAND_SHAKE_RADIUS, 255, 180, 60, 0.35, 24)

    if CombatEvents.IsDamageable(target) then
        CombatEvents.ApplyDamageAndNotify(obj, target, {
            Instigator = obj,
            DamageCauser = obj,
            HitActor = target,
            HitLocation = hitLocation,
            ShotDirection = hitDir,
            Damage = LEAP_LAND_DAMAGE + phase * 4.0,
            DamageType = "leapLand",
        })
    end

    local action = target:GetActionComponent()
    if action ~= nil then
        action:Knockback(hitDir, LEAP_KNOCKBACK_DISTANCE, LEAP_KNOCKBACK_DURATION)
    end
end

local function can_start_leap(bb)
    if bb == nil then return false end
    if not bb.canLeap then return false end
    if activeAttack ~= nil or is_animation_locked() or is_leaping() then return false end
    return true
end

local function start_leap_engage(bb)
    if not can_start_leap(bb) then return false end

    local landing = calculate_leap_landing(bb.target)
    local totalTime = LEAP_WINDUP_TIME + LEAP_FLIGHT_TIME + LEAP_LAND_TIME
    leapState = {
        stage = "windup",
        elapsed = 0.0,
        startLocation = obj.Location,
        landingLocation = landing,
        didLandHit = false,
    }

    cooldowns.leap = LEAP_COOLDOWN
    actionTimer = totalTime
    animationLockTimer = math.max(animationLockTimer, totalTime)
    activeAttack = nil
    set_tactic(TACTIC.leapEngage, totalTime)
    play_anim(ANIM.leapStart, false, true)

    CombatEvents.NotifyAttackFired(obj, {
        Instigator = obj,
        DamageCauser = obj,
        Damage = LEAP_LAND_DAMAGE + phase * 4.0,
        DamageType = "leapWindup",
    })

    return true
end

local function update_leap(dt)
    if leapState == nil then return false end

    local target = find_target()
    if target ~= nil and target:IsValid() then
        face_target(target)
    end

    leapState.elapsed = leapState.elapsed + dt

    if leapState.stage == "windup" then
        if leapState.elapsed >= LEAP_WINDUP_TIME then
            leapState.stage = "flight"
            leapState.elapsed = 0.0
            leapState.startLocation = obj.Location
            if target ~= nil and target:IsValid() then
                leapState.landingLocation = calculate_leap_landing(target)
            end
            play_anim(ANIM.leapFloat, true, true)
        end
        return true
    end

    if leapState.stage == "flight" then
        local alpha = clamp(leapState.elapsed / LEAP_FLIGHT_TIME, 0.0, 1.0)
        local eased = smoothstep(alpha)
        local nextLocation = lerp(leapState.startLocation, leapState.landingLocation, eased)
        nextLocation.Z = lerp(leapState.startLocation, leapState.landingLocation, eased).Z + math.sin(alpha * math.pi) * LEAP_ARC_HEIGHT
        obj.Location = nextLocation

        if leapState.elapsed >= LEAP_FLIGHT_TIME then
            obj.Location = leapState.landingLocation
            leapState.stage = "land"
            leapState.elapsed = 0.0
            play_anim(ANIM.leapLand, false, true)
            if not leapState.didLandHit then
                apply_leap_landing_hit()
                leapState.didLandHit = true
            end
        end
        return true
    end

    if leapState.stage == "land" then
        if leapState.elapsed >= LEAP_LAND_TIME then
            leapState = nil
            actionTimer = 0.0
            animationLockTimer = 0.0
            set_tactic(TACTIC.closeCombat, CLOSE_COMBAT_COMMIT_TIME)
        end
        return true
    end

    leapState = nil
    return false
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
    tacticCommitTimer = math.max(0.0, tacticCommitTimer - dt)
    tacticReevaluateTimer = math.max(0.0, tacticReevaluateTimer - dt)

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
    local lineOfSight = has_line_of_sight(target)
    local canLeap = cooldowns.leap <= 0.0
        and lineOfSight
        and dist >= LEAP_MIN_RANGE
        and dist <= LEAP_MAX_RANGE
        and math.abs(verticalDelta) <= LEAP_MAX_VERTICAL_DELTA

    return {
        target = target,
        distance = dist,
        aimDistance = aimDelta:Length(),
        verticalDelta = verticalDelta,
        canGroundMelee = canGroundMelee,
        canLeap = canLeap,
        isClose = dist <= CLOSE_RETREAT_RANGE or canGroundMelee,
        isDuelRange = dist > CLOSE_RETREAT_RANGE and dist <= KEEP_RANGE + 8.0,
        isFar = dist > KEEP_RANGE + 8.0,
        healthRatio = health_ratio(),
        phase = phase,
        lineOfSight = lineOfSight,
    }
end

local function update_opening_walk(bb, dt)
    if not openingWalkActive then return false end

    currentTactic = TACTIC.openingWalk
    if bb.distance <= OPENING_WALK_END_RANGE then
        openingWalkActive = false
        clear_action_anim("openingWalk")
        set_tactic(TACTIC.approach, APPROACH_COMMIT_TIME)
        debug_log(string.format("[%.2f][BossAI] opening_walk_end dist=%.1f", debugSessionTime, bb.distance))
        return false
    end

    local toTarget = horizontal_delta(obj.Location, bb.target.Location)
    local dirToTarget = normalized_or_zero(toTarget)
    move_horizontal(dirToTarget, OPENING_WALK_SPEED, dt)
    set_move_anim(MOVE_WALK)
    hold_action_anim("openingWalk", OPENING_WALK_ACTION_DURATION)
    currentAnim = ANIM.walk
    return true
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
        request_action_anim("retreat", RETREAT_ACTION_LOCK)
        currentAnim = ANIM.retreat
        return true
    end

    return false
end

local function choose_close_combat_action(bb)
    local meleeScore = score_melee(bb)
    if meleeScore > -999.0 then
        return "melee"
    end

    local retreatScore = score_retreat(bb)
    if retreatScore > -999.0 then
        return "retreat"
    end

    return nil
end

local function choose_duel_action(bb)
    local powerScore = score_power_shot(bb)
    local cannonScore = score_cannon(bb)

    if powerScore <= -999.0 and cannonScore <= -999.0 then
        return nil
    end

    if random01() < 0.18 then
        return nil
    end

    if powerScore > cannonScore then
        return "powerShot"
    end
    return "cannon"
end

local function choose_next_tactic(bb)
    if can_start_leap(bb) then
        local leapChance = phase >= 3 and 0.68 or 0.42
        if bb.distance > KEEP_RANGE + 18.0 or random01() < leapChance then
            return TACTIC.leapEngage
        end
    end

    if bb.isClose then
        return TACTIC.closeCombat
    end

    if bb.isFar then
        return TACTIC.approach
    end

    return TACTIC.duel
end

local function refresh_tactic(bb)
    if is_leaping() or activeAttack ~= nil or phaseLockTimer > 0.0 then return end

    if currentTactic == nil then
        set_tactic(choose_next_tactic(bb))
        return
    end

    if bb.isClose and currentTactic ~= TACTIC.closeCombat then
        set_tactic(TACTIC.closeCombat, CLOSE_COMBAT_COMMIT_TIME)
        return
    end

    if bb.isFar and currentTactic == TACTIC.closeCombat then
        set_tactic(TACTIC.approach, APPROACH_COMMIT_TIME)
        return
    end

    if tacticCommitTimer > 0.0 or tacticReevaluateTimer > 0.0 then
        return
    end

    local nextTactic = choose_next_tactic(bb)
    if nextTactic ~= currentTactic then
        set_tactic(nextTactic)
    else
        tacticReevaluateTimer = TACTIC_REEVALUATE_INTERVAL
    end
end

local function run_tactic_action(bb)
    if currentTactic == TACTIC.leapEngage then
        if start_leap_engage(bb) then
            return true
        end
        set_tactic(TACTIC.approach, APPROACH_COMMIT_TIME)
        actionTimer = 0.2
        return true
    end

    if currentTactic == TACTIC.closeCombat then
        return run_attack(choose_close_combat_action(bb))
    end

    if currentTactic == TACTIC.duel then
        local action = choose_duel_action(bb)
        if action == nil then
            actionTimer = 0.25 + random01() * 0.25
            return true
        end
        return run_attack(action)
    end

    if currentTactic == TACTIC.approach then
        if can_start_leap(bb) and random01() < (phase >= 3 and 0.55 or 0.35) then
            return start_leap_engage(bb)
        end
        actionTimer = 0.2
        return true
    end

    return run_attack(choose_attack(bb))
end

local function locomote_approach(bb, dt)
    local toTarget = horizontal_delta(obj.Location, bb.target.Location)
    local dirToTarget = normalized_or_zero(toTarget)

    if actionTimer > 0.0 and currentAnim == ANIM.retreat then
        move_horizontal(dirToTarget * -1.0, 14.0 + phase * 2.5, dt)
        return
    end

    if currentTactic == TACTIC.leapEngage then
        local approachSpeed = phase >= 3 and APPROACH_RUN_SPEED_PHASE3 or APPROACH_RUN_SPEED
        move_horizontal(dirToTarget, approachSpeed, dt)
        play_anim(bb.distance > APPROACH_RUN_DISTANCE and ANIM.run or ANIM.walk, true, false)
        return
    end

    if bb.distance > KEEP_RANGE - 3.0 then
        local approachSpeed = phase >= 3 and APPROACH_RUN_SPEED_PHASE3 or APPROACH_RUN_SPEED
        move_horizontal(dirToTarget, approachSpeed, dt)
        play_anim(bb.distance > APPROACH_RUN_DISTANCE and ANIM.run or ANIM.walk, true, false)
        return
    end

    play_anim(ANIM.idle, true, false)
end

local function locomote_duel(bb, dt)
    local toTarget = horizontal_delta(obj.Location, bb.target.Location)
    local dirToTarget = normalized_or_zero(toTarget)

    if bb.distance > KEEP_RANGE + 8.0 then
        move_horizontal(dirToTarget, 5.0 + phase, dt)
        play_anim(ANIM.walk, true, false)
        return
    end

    if bb.distance < FIRE_MIN_RANGE then
        move_horizontal(dirToTarget * -1.0, 4.5 + phase, dt)
        play_anim(ANIM.retreat, false, false)
        return
    end

    if strafeFlipTimer <= 0.0 then
        strafeSign = random01() < 0.5 and -1.0 or 1.0
        strafeFlipTimer = 1.0 + random01() * 1.4
    end

    local side = Vector.new(-dirToTarget.Y, dirToTarget.X, 0.0) * strafeSign
    move_horizontal(side, 2.8 + phase * 0.8, dt)
    play_anim(strafeSign > 0.0 and ANIM.strafeRight or ANIM.strafeLeft, true, false)
end

local function locomote_close_combat(bb, dt)
    local toTarget = horizontal_delta(obj.Location, bb.target.Location)
    local dirToTarget = normalized_or_zero(toTarget)

    if bb.distance < MIN_RANGE then
        move_horizontal(dirToTarget * -1.0, 6.0 + phase, dt)
        play_anim(ANIM.retreat, false, false)
        return
    end

    if bb.distance > MELEE_RANGE and cooldowns.melee <= 0.2 then
        move_horizontal(dirToTarget, 4.5 + phase, dt)
        play_anim(ANIM.walk, true, false)
        return
    end

    play_anim(ANIM.idle, true, false)
end

local function locomote(bb, dt)
    if currentTactic == TACTIC.approach then
        locomote_approach(bb, dt)
        return
    end

    if currentTactic == TACTIC.leapEngage then
        locomote_approach(bb, dt)
        return
    end

    if currentTactic == TACTIC.closeCombat then
        locomote_close_combat(bb, dt)
        return
    end

    if currentTactic == TACTIC.duel then
        locomote_duel(bb, dt)
        return
    end

    local toTarget = horizontal_delta(obj.Location, bb.target.Location)
    local dirToTarget = normalized_or_zero(toTarget)

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
    leapState = nil
    currentAnim = nil
    currentTactic = TACTIC.openingWalk
    openingWalkActive = true
    tacticCommitTimer = 0.0
    tacticReevaluateTimer = 0.0
    reset_anim_state()
    cooldowns.cannon = 0.0
    cooldowns.powerShot = 1.4
    cooldowns.melee = 0.0
    cooldowns.retreat = 0.0
    cooldowns.leap = 2.5

    obj:AddTag("enemy")
    obj:AddTag("boss")
    CombatEvents.RegisterDamageable(obj, {
        ApplyDamage = apply_boss_damage,
        IsDead = function() return isDead end,
    })

    debug_open_log()
    _G.BossDebugLog = debug_log
    debugSessionTime = 0.0

    play_anim(ANIM.idle, true, true)
    debug_log("[BossTitanAI] Titan boss online.")
end

function EndPlay()
    if obj ~= nil and _G.BossTitanAnimState ~= nil then
        _G.BossTitanAnimState[obj.UUID] = nil
    end

    CombatEvents.UnregisterDamageable(obj)
    debug_close_log()
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
    debugSessionTime = debugSessionTime + dt
    debugLogTimer = debugLogTimer - dt

    local target = find_target()
    update_anim_aim(target)

    if update_leap(dt) then
        return
    end

    update_active_attack(dt)
    apply_pending_phase_if_ready()

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
    local openingWalkHandled = update_opening_walk(bb, dt)

    if not openingWalkHandled then
        refresh_tactic(bb)

        if actionTimer <= 0.0 and thinkTimer <= 0.0 then
            thinkTimer = THINK_INTERVAL
            run_tactic_action(bb)
        end

        if activeAttack == nil and not is_leaping() and (not is_animation_locked() or is_retreating()) then
            locomote(bb, dt)
        end
    end

    if debugLogTimer <= 0.0 then
        debugLogTimer = DEBUG_LOG_INTERVAL
        local state = get_anim_state()
        local moveName = state and MOVE_NAMES[state.MoveState] or "?"
        local actionName = state and state.ActionName or "nil"
        local actionSerial = state and state.ActionSerial or 0
        local animName = ANIM_NAMES[currentAnim] or (currentAnim and "?" or "nil")
        local leapStage = leapState and leapState.stage or "nil"
        debug_log(string.format(
            "[%.2f][BossAI] tac=%s dist=%.1f anim=%s move=%s act=%s ser=%d lock=%.2f actT=%.2f leap=%s open=%s loc=(%.1f,%.1f,%.1f)",
            debugSessionTime,
            tostring(currentTactic),
            bb.distance,
            animName,
            moveName,
            tostring(actionName),
            actionSerial,
            animationLockTimer,
            actionTimer,
            leapStage,
            tostring(openingWalkActive),
            obj.Location.X, obj.Location.Y, obj.Location.Z))
    end
end
