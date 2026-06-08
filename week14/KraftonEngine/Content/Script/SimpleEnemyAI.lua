local CombatEvents = require("Game.CombatEvents")

local PLAYER_TAG = "player"

-- Base tuning was done against a scale-7 robodude. At BeginPlay the actor's
-- actual scale is read and distance/speed constants are rescaled accordingly,
-- so the AI works the same on a scale-12 robodude (e.g. FL_Level1.Scene) as
-- on the scale-7 ones (Default.Scene / FPSARMREAL.Scene).
local SCALE_REFERENCE = 7.0

local MAX_HEALTH = 60.0
local THINK_INTERVAL = 0.16
local SIGHT_RANGE = 260.0
local PROXIMITY_ALERT_RANGE = 140.0
local FORGET_RANGE = 340.0
local SIGHT_HALF_FOV_DOT = 0.35
local EYE_HEIGHT = 5.2
local TARGET_HEIGHT = 1.4
local FIRE_RANGE = 120.0
local FIRE_INTERVAL_MIN = 0.85
local FIRE_INTERVAL_MAX = 1.35
local FIRE_DAMAGE = 3.0
local INACCURACY_DEGREES = 7.5
local FIRE_ACTION_DURATION = 0.55
local HIT_REACT_DURATION = 0.45
local CHASE_SPEED = 4.6
local CHASE_START_RANGE = 34.0
local CHASE_STOP_RANGE = 25.0
local FOOTSTEP_INTERVAL = 0.42

local scaleMultiplier = 1.0

local AUDIO_WALK_STEP = "enemy.human.walk.step"
local AUDIO_FIRE = "enemy.human.weapon.fire"
local AUDIO_DEATH = "enemy.human.death"

local STATE_IDLE = 0
local STATE_ALERT = 1
local STATE_CHASE = 2

local targetActor = nil
local currentHealth = MAX_HEALTH
local isDead = false
local thinkTimer = 0.0
local fireTimer = 0.0
local hitReactTimer = 0.0
local footstepTimer = 0.0
local alerted = false
local isMoving = false
local debugTimer = 0.0
local DEBUG_INTERVAL = 1.0

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function random_range(minValue, maxValue)
    return minValue + (maxValue - minValue) * math.random()
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

local function is_valid_actor(actor)
    if actor == nil then return false end
    if type(actor.IsValid) == "function" then
        return actor:IsValid()
    end
    return true
end

local function play_event_at(eventName, location)
    if eventName == nil or AudioManager == nil then
        return false
    end

    location = location or obj.Location
    if AudioManager.PlayEventAt ~= nil then
        return AudioManager.PlayEventAt(eventName, location.X or 0.0, location.Y or 0.0, location.Z or 0.0)
    end
    if AudioManager.PlayOneShotAt ~= nil then
        return AudioManager.PlayOneShotAt(eventName, location.X or 0.0, location.Y or 0.0, location.Z or 0.0)
    end
    if AudioManager.PlayEvent ~= nil then
        return AudioManager.PlayEvent(eventName)
    end
    return false
end

local function horizontal(v)
    return Vector.new(v.X, v.Y, 0.0)
end

local function normalized_or_zero(v)
    local length = v:Length()
    if length <= 0.0001 then
        return Vector.new(0.0, 0.0, 0.0)
    end
    return v / length
end

local function get_anim_state()
    if obj == nil then return nil end

    _G.SimpleEnemyAnimState = _G.SimpleEnemyAnimState or {}

    local ownerId = obj.UUID
    local state = _G.SimpleEnemyAnimState[ownerId]
    if state == nil then
        state = {
            MoveState = STATE_IDLE,
            Alerted = false,
            Moving = false,
            ActionName = nil,
            ActionDuration = 0.0,
            ActionSerial = 0,
        }
        _G.SimpleEnemyAnimState[ownerId] = state
    end

    return state
end

local function request_anim_action(name, duration)
    local state = get_anim_state()
    if state == nil then return end

    state.ActionName = name
    state.ActionDuration = duration or 0.0
    state.ActionSerial = (state.ActionSerial or 0) + 1
end

local function update_anim_state()
    local state = get_anim_state()
    if state == nil then return end

    state.Alerted = alerted == true
    state.Moving = isMoving == true
    if not alerted then
        state.MoveState = STATE_IDLE
    elseif isMoving then
        state.MoveState = STATE_CHASE
    else
        state.MoveState = STATE_ALERT
    end
end

local function clear_anim_state()
    if obj == nil or _G.SimpleEnemyAnimState == nil then return end
    _G.SimpleEnemyAnimState[obj.UUID] = nil
end

local function get_eye_location()
    return obj.Location + Vector.new(0.0, 0.0, EYE_HEIGHT * scaleMultiplier)
end

local function get_target_location(target)
    return target.Location + Vector.new(0.0, 0.0, TARGET_HEIGHT)
end

local function face_target(target)
    local delta = horizontal(target.Location - obj.Location)
    if delta:Length() <= 0.001 then return end

    local yaw = atan2(delta.Y, delta.X) * 180.0 / math.pi
    local rot = obj.Rotation
    obj.Rotation = Vector.new(rot.X, rot.Y, yaw)
end

local function horizontal_distance_to(target)
    if not is_valid_actor(target) then return 0.0 end
    return horizontal(target.Location - obj.Location):Length()
end

local function update_chase(target, dt)
    local wasMoving = isMoving
    isMoving = false
    if not alerted or not is_valid_actor(target) or hitReactTimer > 0.0 then
        return
    end

    local flatDelta = horizontal(target.Location - obj.Location)
    local distance = flatDelta:Length()
    local stopRange = CHASE_STOP_RANGE * scaleMultiplier
    if distance <= stopRange then
        return
    end

    if distance < CHASE_START_RANGE * scaleMultiplier and not wasMoving then
        return
    end

    local dir = normalized_or_zero(flatDelta)
    if dir:Length() <= 0.001 then
        return
    end

    local step = math.min(CHASE_SPEED * scaleMultiplier * dt, math.max(distance - stopRange, 0.0))
    if step <= 0.0 then
        return
    end

    obj:AddWorldOffset(dir * step)
    isMoving = true

    footstepTimer = footstepTimer - dt
    if footstepTimer <= 0.0 then
        play_event_at(AUDIO_WALK_STEP, obj.Location)
        footstepTimer = FOOTSTEP_INTERVAL
    end
end

local function can_see_target(target)
    if not is_valid_actor(target) then return false end

    local source = get_eye_location()
    local targetPos = get_target_location(target)
    local toTarget = targetPos - source
    local distance = toTarget:Length()
    if distance <= 0.001 or distance > SIGHT_RANGE * scaleMultiplier then
        return false
    end

    local flatToTarget = normalized_or_zero(horizontal(toTarget))
    local flatForward = normalized_or_zero(horizontal(obj.Forward))
    if flatToTarget:Length() <= 0.001 or flatForward:Length() <= 0.001 then
        return false
    end

    if flatForward:Dot(flatToTarget) < SIGHT_HALF_FOV_DOT then
        return false
    end

    local staticHit = nil
    if World.RaycastWorldStatic ~= nil then
        staticHit = World.RaycastWorldStatic(source, toTarget:Normalized(), distance, obj)
    end

    return staticHit == nil
end

local function has_clear_static_line_to_target(target, maxRange)
    if not is_valid_actor(target) then return false end

    local source = get_eye_location()
    local targetPos = get_target_location(target)
    local toTarget = targetPos - source
    local distance = toTarget:Length()
    if distance <= 0.001 or distance > maxRange then
        return false
    end

    local staticHit = nil
    if World.RaycastWorldStatic ~= nil then
        staticHit = World.RaycastWorldStatic(source, toTarget:Normalized(), distance, obj)
    end

    return staticHit == nil
end

local function should_alert_to_target(target)
    if not is_valid_actor(target) then return false end

    local distance = horizontal_distance_to(target)
    local proximityRange = PROXIMITY_ALERT_RANGE * scaleMultiplier

    -- 매우 가까운 거리(파이어 레인지 절반 이내)에선 라인오브사이트/FOV 무시.
    -- scale-12 robodude처럼 자체 충돌체가 두꺼운 경우 raycast가 가짜로 막히고,
    -- 코앞에서는 FOV dot 자체가 의미 없어지므로 무조건 경계 상태로 진입한다.
    local closeRange = FIRE_RANGE * scaleMultiplier * 0.5
    if distance <= closeRange then
        return true
    end

    if distance <= proximityRange and has_clear_static_line_to_target(target, proximityRange) then
        return true
    end

    if can_see_target(target) then
        return true
    end

    local forgetRange = FORGET_RANGE * scaleMultiplier
    if alerted and distance <= forgetRange and has_clear_static_line_to_target(target, forgetRange) then
        return true
    end

    return false
end

local function find_target()
    if is_valid_actor(targetActor) then
        return targetActor
    end

    if Game ~= nil and Game.GetPlayerPawn ~= nil then
        targetActor = Game.GetPlayerPawn()
    end
    if not is_valid_actor(targetActor) and World.FindFirstActorByTag ~= nil then
        targetActor = World.FindFirstActorByTag(PLAYER_TAG)
    end
    return targetActor
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

    local attacker = context ~= nil and (context.Instigator or context.instigator or context.DamageCauser or context.damageCauser) or nil
    if is_valid_actor(attacker) then
        targetActor = attacker
        alerted = true
    end

    currentHealth = clamp(currentHealth - amount, 0.0, MAX_HEALTH)
    local killed = currentHealth <= 0.0
    hitReactTimer = HIT_REACT_DURATION

    if killed then
        isDead = true
    else
        request_anim_action("hit", HIT_REACT_DURATION)
    end

    local result = make_damage_result(true, amount, killed)
    if killed then
        play_event_at(AUDIO_DEATH, obj.Location)
        clear_anim_state()
        obj:Destroy()
    end
    return result
end

local function make_inaccurate_direction(baseDir)
    local forward = baseDir:Normalized()
    local right = Vector.new(-forward.Y, forward.X, 0.0)
    if right:Length() <= 0.001 then
        right = Vector.Right()
    else
        right = right:Normalized()
    end

    local up = right:Cross(forward)
    if up:Length() <= 0.001 then
        up = Vector.Up()
    else
        up = up:Normalized()
    end

    local spread = math.tan(INACCURACY_DEGREES * math.pi / 180.0)
    local offsetX = random_range(-spread, spread)
    local offsetY = random_range(-spread, spread)
    return (forward + right * offsetX + up * offsetY):Normalized()
end

local function fire_at(target)
    if not is_valid_actor(target) then return end

    face_target(target)

    local source = get_eye_location()
    local aimDelta = get_target_location(target) - source
    if aimDelta:Length() <= 0.001 then return end

    local fireDir = make_inaccurate_direction(aimDelta)
    local fireRange = FIRE_RANGE * scaleMultiplier
    local hit = World.RaycastPrimitive(source, fireDir, fireRange, obj)
    local hitLocation = source + fireDir * fireRange
    local hitNormal = nil
    local hitActor = nil
    local targetHitLocation = get_target_location(target)
    local bDirectTargetHit = has_clear_static_line_to_target(target, fireRange)

    if hit ~= nil then
        hitLocation = hit.WorldHitLocation
        hitNormal = hit.WorldNormal or hit.ImpactNormal
        hitActor = hit.HitActor
    end

    local fireContext = CombatEvents.MakeDamageContext({
        Instigator = obj,
        DamageCauser = obj,
        HitActor = hitActor,
        HitLocation = hitLocation,
        HitNormal = hitNormal,
        ShotDirection = fireDir,
        Damage = FIRE_DAMAGE,
        DamageType = "EnemyBullet",
    })

    CombatEvents.NotifyAttackFired(obj, fireContext)
    request_anim_action("fire", FIRE_ACTION_DURATION)
    play_event_at(AUDIO_FIRE, source)

    if hit ~= nil then
        CombatEvents.NotifyAttackImpact(obj, fireContext)
    end

    if CombatEvents.IsDamageable(target) and (hitActor == target or bDirectTargetHit) then
        local damageContext = fireContext
        if hitActor ~= target then
            damageContext = CombatEvents.MakeDamageContext({
                Instigator = obj,
                DamageCauser = obj,
                HitActor = target,
                HitLocation = targetHitLocation,
                HitNormal = nil,
                ShotDirection = fireDir,
                Damage = FIRE_DAMAGE,
                DamageType = "EnemyBullet",
            })
            CombatEvents.NotifyAttackImpact(obj, damageContext)
        end
        CombatEvents.ApplyDamageAndNotify(obj, target, damageContext)
    end
end

function BeginPlay()
    currentHealth = MAX_HEALTH
    isDead = false
    thinkTimer = 0.0
    fireTimer = random_range(0.25, 0.65)
    hitReactTimer = 0.0
    footstepTimer = 0.0
    alerted = false
    isMoving = false
    targetActor = nil

    local actorScale = obj.Scale
    local s = math.max(actorScale.X, actorScale.Y, actorScale.Z)
    if s > 0.0 then
        scaleMultiplier = s / SCALE_REFERENCE
    else
        scaleMultiplier = 1.0
    end

    obj:AddTag("enemy")
    CombatEvents.RegisterDamageable(obj, {
        ApplyDamage = apply_enemy_damage,
        IsDead = function() return isDead end,
    })

    update_anim_state()
    print("[SimpleEnemyAI] online: " .. obj.Name)
end

function EndPlay()
    CombatEvents.UnregisterDamageable(obj)
    clear_anim_state()
end

function OnOverlap(OtherActor)
end

function OnHit(OtherActor, HitComponent, OtherComp, NormalImpulse, HitResult)
end

function Tick(dt)
    if isDead then return end

    if fireTimer > 0.0 then
        fireTimer = fireTimer - dt
    end
    if hitReactTimer > 0.0 then
        hitReactTimer = hitReactTimer - dt
    end

    thinkTimer = thinkTimer - dt
    if thinkTimer <= 0.0 then
        thinkTimer = THINK_INTERVAL
        local target = find_target()
        alerted = should_alert_to_target(target)

        if alerted and target ~= nil then
            face_target(target)
        end
    end

    update_chase(targetActor, dt)
    update_anim_state()

    if alerted and horizontal_distance_to(targetActor) <= FIRE_RANGE * scaleMultiplier and hitReactTimer <= 0.0 and fireTimer <= 0.0 then
        fire_at(targetActor)
        fireTimer = random_range(FIRE_INTERVAL_MIN, FIRE_INTERVAL_MAX)
    end

    debugTimer = debugTimer - dt
    if debugTimer <= 0.0 then
        debugTimer = DEBUG_INTERVAL
        local hasTarget = is_valid_actor(targetActor)
        local dist = hasTarget and horizontal_distance_to(targetActor) or -1.0
        print(string.format(
            "[SimpleEnemyAI] target=%s dist=%.2f alerted=%s fireRange=%.2f scale=%.2f",
            hasTarget and "yes" or "NO",
            dist,
            tostring(alerted),
            FIRE_RANGE * scaleMultiplier,
            scaleMultiplier))
    end
end
