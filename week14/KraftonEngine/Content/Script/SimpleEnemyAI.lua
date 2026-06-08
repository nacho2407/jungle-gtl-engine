local CombatEvents = require("Game.CombatEvents")

local PLAYER_TAG = "player"

local MAX_HEALTH = 120.0
local THINK_INTERVAL = 0.16
local SIGHT_RANGE = 55.0
local SIGHT_HALF_FOV_DOT = 0.35
local EYE_HEIGHT = 5.2
local TARGET_HEIGHT = 1.4
local FIRE_RANGE = 62.0
local FIRE_INTERVAL_MIN = 0.85
local FIRE_INTERVAL_MAX = 1.35
local FIRE_DAMAGE = 8.0
local INACCURACY_DEGREES = 7.5
local FIRE_ACTION_DURATION = 0.55
local HIT_REACT_DURATION = 0.45
local CHASE_SPEED = 4.6
local CHASE_START_RANGE = 34.0
local CHASE_STOP_RANGE = 25.0
local FOOTSTEP_INTERVAL = 0.42

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
    return obj.Location + Vector.new(0.0, 0.0, EYE_HEIGHT)
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
    if distance <= CHASE_STOP_RANGE then
        return
    end

    if distance < CHASE_START_RANGE and not wasMoving then
        return
    end

    local dir = normalized_or_zero(flatDelta)
    if dir:Length() <= 0.001 then
        return
    end

    local step = math.min(CHASE_SPEED * dt, math.max(distance - CHASE_STOP_RANGE, 0.0))
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
    if distance <= 0.001 or distance > SIGHT_RANGE then
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

local function find_target()
    if is_valid_actor(targetActor) then
        return targetActor
    end

    targetActor = World.FindFirstActorByTag(PLAYER_TAG)
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
    local hit = World.RaycastPrimitive(source, fireDir, FIRE_RANGE, obj)
    local hitLocation = source + fireDir * FIRE_RANGE
    local hitNormal = nil
    local hitActor = nil

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

    if hitActor == target and CombatEvents.IsDamageable(hitActor) then
        CombatEvents.ApplyDamageAndNotify(obj, hitActor, fireContext)
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
        alerted = can_see_target(target)

        if alerted and target ~= nil then
            face_target(target)
        end
    end

    update_chase(targetActor, dt)
    update_anim_state()

    if alerted and horizontal_distance_to(targetActor) <= FIRE_RANGE and hitReactTimer <= 0.0 and fireTimer <= 0.0 then
        fire_at(targetActor)
        fireTimer = random_range(FIRE_INTERVAL_MIN, FIRE_INTERVAL_MAX)
    end
end
