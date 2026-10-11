local CombatEvents = require("Game.CombatEvents")

local PLAYER_TAG = "player"
local DAMAGE_AMOUNT = 5.0
local DAMAGE_INTERVAL = 0.2
local DAMAGE_TYPE = "ElectricTrap"
local VELOCITY_ZERO_COOLDOWN = 2.0
local LOOP_VOLUME = 1.0
local LOOP_MIN_DISTANCE = 28.0
local LOOP_MAX_DISTANCE = 100.0
local LOOP_ROLLOFF = 2.4
local HIT_EVENT = "trap.electric.player_hit"
local LOOP_NAME_PREFIX = "ElectricTrapLoop_"
local LOOP_AUDIO = {
    { key = "ElectricTrapLoop1", path = "Traps/Electric/ElectricTrapLoop1.wav" },
    { key = "ElectricTrapLoop2", path = "Traps/Electric/ElectricTrapLoop2.wav" },
    { key = "ElectricTrapLoop3", path = "Traps/Electric/ElectricTrapLoop3.wav" },
    { key = "ElectricTrapLoop4", path = "Traps/Electric/ElectricTrapLoop4.wav" },
}

local overlappingPlayers = {}
local loopName = nil
local activeLoopKey = nil
local bLoopAudioStarted = false
_G.ElectricTrapVelocityZeroNextTimes = _G.ElectricTrapVelocityZeroNextTimes or {}

local function get_time_seconds()
    if Game ~= nil and Game.GetTimeSeconds ~= nil then
        return Game.GetTimeSeconds()
    end
    return 0.0
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

local function get_actor_location(actor)
    if actor ~= nil and actor.Location ~= nil then
        return actor.Location
    end
    if obj ~= nil and obj.Location ~= nil then
        return obj.Location
    end
    return Vector.new(0.0, 0.0, 0.0)
end

local function play_event_at(eventName, location)
    if AudioManager == nil or eventName == nil then
        return false
    end

    location = location or get_actor_location(obj)
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

local function make_loop_name()
    local id = actor_id(obj) or tostring(obj)
    return LOOP_NAME_PREFIX .. tostring(id)
end

local function start_loop_audio()
    if AudioManager == nil or AudioManager.Load == nil then
        return false
    end

    local desiredLoopName = loopName or make_loop_name()
    local selected = LOOP_AUDIO[math.random(1, #LOOP_AUDIO)]
    activeLoopKey = selected.key
    for _, entry in ipairs(LOOP_AUDIO) do
        AudioManager.Load(entry.key, entry.path, true)
    end

    local location = get_actor_location(obj)
    local bStarted = false
    if AudioManager.PlayLoopAt ~= nil then
        bStarted = AudioManager.PlayLoopAt(
            activeLoopKey,
            desiredLoopName,
            location.X or 0.0,
            location.Y or 0.0,
            location.Z or 0.0,
            LOOP_VOLUME,
            1.0,
            LOOP_MIN_DISTANCE,
            LOOP_MAX_DISTANCE,
            LOOP_ROLLOFF)
    elseif AudioManager.PlayLoop ~= nil then
        bStarted = AudioManager.PlayLoop(activeLoopKey, desiredLoopName, LOOP_VOLUME, 1.0)
    end

    if bStarted then
        loopName = desiredLoopName
        bLoopAudioStarted = true
    end
    return bStarted
end

local function ensure_loop_audio()
    if AudioManager == nil then
        return
    end

    if not bLoopAudioStarted or loopName == nil then
        start_loop_audio()
        return
    end

    if AudioManager.IsLoopPlaying ~= nil and not AudioManager.IsLoopPlaying(loopName) then
        bLoopAudioStarted = false
        start_loop_audio()
    end
end

local function stop_loop_audio()
    if AudioManager == nil or loopName == nil then
        return
    end

    if AudioManager.FadeOutLoop ~= nil then
        AudioManager.FadeOutLoop(loopName, 180.0)
    elseif AudioManager.StopLoop ~= nil then
        AudioManager.StopLoop(loopName)
    end
    loopName = nil
    activeLoopKey = nil
    bLoopAudioStarted = false
end

local function zero_player_velocity_if_ready(entry)
    if entry == nil or not is_valid_actor(entry.Actor) then
        return
    end

    local id = actor_id(entry.Actor)
    if id == nil then
        return
    end

    local now = get_time_seconds()
    local nextAllowedTime = _G.ElectricTrapVelocityZeroNextTimes[id] or 0.0
    if now < nextAllowedTime then
        return
    end

    local movement = nil
    if type(entry.Actor.GetCharacterMovement) == "function" then
        movement = entry.Actor:GetCharacterMovement()
    end
    if movement ~= nil and type(movement.SetVelocity) == "function" then
        movement:SetVelocity(Vector.new(0.0, 0.0, 0.0))
    end

    play_event_at(HIT_EVENT, get_actor_location(entry.Actor))
    _G.ElectricTrapVelocityZeroNextTimes[id] = now + VELOCITY_ZERO_COOLDOWN
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
    start_loop_audio()
end

function EndPlay()
    stop_loop_audio()
    overlappingPlayers = {}
end

function OnOverlap(OtherActor, OverlappedComponent, OtherComponent)
    ensure_loop_audio()

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
    zero_player_velocity_if_ready(overlappingPlayers[id])
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

    ensure_loop_audio()

    if AudioManager ~= nil and AudioManager.SetLoopPosition ~= nil and loopName ~= nil then
        local location = get_actor_location(obj)
        AudioManager.SetLoopPosition(loopName, location.X or 0.0, location.Y or 0.0, location.Z or 0.0)
    end

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
                zero_player_velocity_if_ready(entry)
                entry.TimeUntilNextDamage = entry.TimeUntilNextDamage + DAMAGE_INTERVAL
            end
        end
    end
end
