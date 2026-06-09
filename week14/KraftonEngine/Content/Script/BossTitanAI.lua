local CombatEvents = require("Game.CombatEvents")
local WeaponHud = require("HUD/WeaponHud")
local BossAnimDefs = require("Anim.boss_titan_defs")

PLAYER_TAG = "player"

MAX_HEALTH = 650.0
PHASE_TWO_HEALTH_RATIO = 0.5
PHASE_TWO_TRANSITION_ANIM_DURATION = 1.0
PHASE_TWO_CROUCH_HOLD_DURATION = 2.0
BOSS_SECTION_COLOR_PARAM = "SectionColor"
BOSS_PHASE_ONE_COLOR = { 1.0, 1.0, 1.0, 1.0 }
BOSS_PHASE_TWO_COLOR = { 1.0, 0.08, 0.04, 1.0 }
THINK_INTERVAL = 0.12
LOS_CHECK_INTERVAL = 0.15
SIGHT_RANGE = 80.0
OPENING_WALK_END_RANGE = 64.0
OPENING_WALK_SPEED = 2.6
OPENING_WALK_ACTION_DURATION = 999.0
KEEP_RANGE = 28.0
MIN_RANGE = 9.0
FIRE_MIN_RANGE = 14.0
CLOSE_RETREAT_RANGE = 12.0
MELEE_RANGE = 14.0
CANNON_RANGE = 82.0
APPROACH_RUN_SPEED = 5.2
APPROACH_RUN_DISTANCE = 40.0
TARGET_HEIGHT = 1.6
FALLBACK_MUZZLE_HEIGHT = 7.0
MELEE_MAX_VERTICAL_DELTA = 8.5
ATTACK_LOS_FALLBACK_RANGE = 60.0
MELEE_ACTION_LOCK = 3.8
MELEE_HIT_DELAY = 0.72
POWER_SHOT_ACTION_LOCK = 1.25
CANNON_ACTION_LOCK = 0.85
RETREAT_ACTION_LOCK = 0.55
MELEE_KNOCKBACK_DISTANCE = 5.2
MELEE_KNOCKBACK_DURATION = 0.22
MUZZLE_BONE = "ja_c_propGun"
MUZZLE_LOCAL_OFFSET = Vector.new(0.0, 0.0, 0.0)
TACTIC_REEVALUATE_INTERVAL = 0.25
APPROACH_COMMIT_TIME = 0.75
DUEL_COMMIT_TIME = 1.1
CLOSE_COMBAT_COMMIT_TIME = 0.6
LEAP_MIN_RANGE = 28.0
LEAP_MAX_RANGE = 95.0
LEAP_MAX_VERTICAL_DELTA = 8.5
LEAP_START_MAX_VERTICAL_DELTA = 32.0
LEAP_LANDING_OFFSET = 8.0
LEAP_WINDUP_TIME = 0.55
LEAP_FLIGHT_TIME = 0.92
LEAP_LAND_TIME = 0.7
LEAP_ARC_HEIGHT = 11.0
LEAP_COOLDOWN = 7.5
LEAP_LAND_DAMAGE_RADIUS = 10.5
LEAP_LAND_SHAKE_RADIUS = 28.0
LEAP_LAND_SHAKE_MAX_VERTICAL_DELTA = 12.0
LEAP_LAND_DAMAGE = 22.0
LEAP_LAND_SHAKE_SCALE = 1.35
LEAP_KNOCKBACK_DISTANCE = 6.0
LEAP_KNOCKBACK_DURATION = 0.28
AIM_PITCH_MIN = -35.0
AIM_PITCH_MAX = 40.0
DEATH_FALLBACK_ANIM_DURATION = 1.4
DEATH_FALL_DURATION = 0.75
DEATH_FALL_PITCH_DEGREES = 82.0
DEATH_FALL_FORWARD_DISTANCE = 0.0
DEATH_FALL_GROUND_CLEARANCE = 0.25
DEATH_FALL_SHAKE_SCALE = 0.45
DEATH_SLOMO_DURATION = 2.2
DEATH_SLOMO_TIME_DILATION = 0.15

DRAKE_COMBAT_DIALOGUE_MODULE = "Dialogue/DrakeCombat.dialogue"
DRAKE_COMBAT_VOICE_MODULE = "Dialogue/Generated/DrakeCombat.voices"
DRAKE_COMBAT_DIALOGUE_WIDTH = 1120.0
DRAKE_COMBAT_DIALOGUE_HEIGHT = 54.0
DRAKE_COMBAT_DIALOGUE_LINE_HEIGHT = 54.0
DRAKE_COMBAT_DIALOGUE_FADE_OUT = 0.25
DRAKE_COMBAT_DIALOGUE_HOLD_PADDING = 0.35
DRAKE_COMBAT_HEALTH_MARKERS = { 0.90, 0.75, 0.50, 0.25 }
DRAKE_COMBAT_EXCLUDED_DIALOGUE_IDS = {
    DrakeCombat_Drake_ThatIsWhy = true,
}
BOSS_BGM_KEY = "BossMusic"
BOSS_BGM_PATH = "Music/Boss.mp3"
BOSS_BGM_LOOP_NAME = "BossMusicLoop"
BOSS_BGM_VOLUME = 0.74
BOSS_BGM_FADE_OUT_MS = 5000.0
CARD_KEY_PREFAB_PATH = "Content/Prefab/CardKey.prefab"
CARD_KEY_SPAWN_OFFSET = Vector.new(2.0, 0.0, 1.0)
CARD_KEY_ITEM_ID = "vantus_master_key"

local MOVE = BossAnimDefs.MOVE
local MOVE_IDLE = MOVE.IDLE
local MOVE_WALK = MOVE.WALK
local MOVE_RUN = MOVE.RUN
local MOVE_STRAFE_LEFT = MOVE.STRAFE_LEFT
local MOVE_STRAFE_RIGHT = MOVE.STRAFE_RIGHT
local MOVE_RETREAT = MOVE.RETREAT
local MOVE_LEAP_FLOAT = MOVE.LEAP_FLOAT

local TACTIC = {
    openingWalk = "openingWalk",
    approach = "approach",
    duel = "duel",
    closeCombat = "closeCombat",
    leapEngage = "leapEngage",
}

local ANIM = BossAnimDefs.ANIM
local MOVE_BY_ANIM_PATH = BossAnimDefs.MOVE_BY_ANIM_PATH
local ACTION_BY_ANIM_PATH = BossAnimDefs.ACTION_BY_ANIM_PATH
local ACTION_DURATIONS = BossAnimDefs.ACTION_DURATIONS

DEBUG_LOG_INTERVAL = 0.4
local debugLogTimer = 0.0
DEBUG_LOG_PATH = "boss_log.txt"
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

local MOVE_NAMES = BossAnimDefs.MOVE_NAMES
local ANIM_NAMES = BossAnimDefs.ANIM_NAMES

local targetActor = nil
local mesh = nil
local frameMuzzleLocation = nil
local lineOfSightTimer = 0.0
local lineOfSightTargetUUID = nil
local lineOfSightResult = false
local blackboard = {}
local currentHealth = MAX_HEALTH
local isDead = false
local damageableCallbacks = nil
local register_boss_damageable = nil
local bBossDamageableRegistered = false

local phase = 1
local pendingPhase = nil
local phaseTransitionStage = nil
local phaseTransitionTimer = 0.0
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
local introCutsceneLastActionSerial = 0
local introCutsceneMissingControlLogged = false
local introCutsceneControlLogTimer = 0.0
local introCutsceneControlTimer = 0.0
INTRO_CUTSCENE_CONTROL_FAILSAFE_SECONDS = 8.0

local cooldowns = {
    cannon = 0.0,
    powerShot = 1.4,
    melee = 0.0,
    retreat = 0.0,
    leap = 2.5,
}

local activeAttack = nil
local leapState = nil
local deathFallState = nil

local drakeCombatStory = nil
local drakeCombatVoiceManifest = nil
local drakeCombatDialogueEntries = {}
local drakeCombatPlayedDialogueIds = {}
local drakeCombatPlayedHealthMarkers = {}
local drakeCombatLoadedVoiceKeys = {}
local drakeCombatDialogueQueue = {}
local activeDrakeCombatDialogue = nil
local activeDrakeCombatDialogueTimer = 0.0
local activeDrakeCombatDialogueDuration = 0.0
local bSpawnedCardKey = false
local bBossMusicLoaded = false
local bBossMusicPlaying = false

local function apply_boss_phase_visual(targetPhase)
    if mesh == nil or mesh.SetMaterialVector4Parameter == nil then
        return false
    end

    local color = BOSS_PHASE_ONE_COLOR
    if targetPhase >= 2 then
        color = BOSS_PHASE_TWO_COLOR
    end

    local applied = mesh:SetMaterialVector4Parameter(
        BOSS_SECTION_COLOR_PARAM,
        color[1],
        color[2],
        color[3],
        color[4])

    if not applied then
        debug_log("[BossTitanAI] Boss material color parameter was not applied.")
    end

    return applied
end

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

local function vector_length_sq(v)
    return v.X * v.X + v.Y * v.Y + v.Z * v.Z
end

local function horizontal_distance_sq(a, b)
    local dx = b.X - a.X
    local dy = b.Y - a.Y
    return dx * dx + dy * dy
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

local function is_player_actor(actor)
    if actor == nil then
        return false
    end
    if Game ~= nil and Game.GetPlayerPawn ~= nil then
        local player = Game.GetPlayerPawn()
        if player ~= nil and actor.UUID ~= nil and player.UUID ~= nil and actor.UUID == player.UUID then
            return true
        end
    end
    return type(actor.HasTag) == "function" and actor:HasTag(PLAYER_TAG)
end

local function load_drake_combat_dialogue_assets()
    if drakeCombatStory ~= nil then
        return
    end

    local okStory, story = pcall(require, DRAKE_COMBAT_DIALOGUE_MODULE)
    if not okStory or story == nil then
        debug_log("[BossTitanAI] Failed to load Drake combat dialogue: " .. tostring(story))
        drakeCombatStory = { entries = {} }
        drakeCombatDialogueEntries = {}
        return
    end

    drakeCombatStory = story
    drakeCombatDialogueEntries = {}
    if story.entries ~= nil then
        for _, entry in ipairs(story.entries) do
            if entry ~= nil
                and entry.id ~= nil
                and entry.text ~= nil
                and DRAKE_COMBAT_EXCLUDED_DIALOGUE_IDS[entry.id] ~= true
                and (entry.speaker == nil or entry.speaker == "DRAKE") then
                table.insert(drakeCombatDialogueEntries, entry)
            end
        end
    end

    local okVoice, voiceManifest = pcall(require, DRAKE_COMBAT_VOICE_MODULE)
    if okVoice and voiceManifest ~= nil then
        drakeCombatVoiceManifest = voiceManifest
    else
        drakeCombatVoiceManifest = nil
        debug_log("[BossTitanAI] Drake combat voice manifest unavailable: " .. tostring(voiceManifest))
    end
end

local function reset_drake_combat_dialogue_state()
    drakeCombatPlayedDialogueIds = {}
    drakeCombatPlayedHealthMarkers = {}
    drakeCombatDialogueQueue = {}
    activeDrakeCombatDialogue = nil
    activeDrakeCombatDialogueTimer = 0.0
    activeDrakeCombatDialogueDuration = 0.0

    if WeaponHud ~= nil and WeaponHud.HideDialogue ~= nil then
        WeaponHud.HideDialogue()
    end
end

local function get_drake_combat_dialogue_text(entry)
    if entry == nil then return "" end
    local speaker = entry.speaker or "DRAKE"
    local text = entry.text or ""
    if speaker == "" then
        return text
    end
    return speaker .. ": " .. text
end

local function play_drake_combat_voice(entry)
    if entry == nil or AudioManager == nil or AudioManager.Load == nil or AudioManager.Play == nil then
        return nil
    end
    if drakeCombatVoiceManifest == nil or drakeCombatVoiceManifest.by_id == nil then
        return nil
    end

    local voiceEntry = drakeCombatVoiceManifest.by_id[entry.id]
    if voiceEntry == nil or voiceEntry.key == nil or voiceEntry.path == nil then
        return nil
    end

    if drakeCombatLoadedVoiceKeys[voiceEntry.key] ~= true then
        if not AudioManager.Load(voiceEntry.key, voiceEntry.path, false) then
            debug_log("[BossTitanAI] Failed to load Drake combat voice: " .. tostring(voiceEntry.path))
            return nil
        end
        drakeCombatLoadedVoiceKeys[voiceEntry.key] = true
    end

    AudioManager.Play(voiceEntry.key, voiceEntry.volume or 1.0)
    return voiceEntry
end

local function show_drake_combat_dialogue(entry)
    if entry == nil then return end

    local defaultFont = drakeCombatStory and drakeCombatStory.default_font or "Pretendard"
    local defaultSize = drakeCombatStory and drakeCombatStory.default_size or 22
    local defaultWeight = drakeCombatStory and drakeCombatStory.default_weight or 700
    local defaultDuration = drakeCombatStory and drakeCombatStory.default_duration or 3.4

    if WeaponHud ~= nil and WeaponHud.ShowDialogue ~= nil then
        WeaponHud.ShowDialogue(get_drake_combat_dialogue_text(entry), {
            width = DRAKE_COMBAT_DIALOGUE_WIDTH,
            height = DRAKE_COMBAT_DIALOGUE_HEIGHT,
            fontSize = entry.size or defaultSize,
            lineHeight = DRAKE_COMBAT_DIALOGUE_LINE_HEIGHT,
            font = entry.font or defaultFont,
            weight = entry.weight or defaultWeight,
            opacity = 0.0,
        })
    end

    local voiceEntry = play_drake_combat_voice(entry)
    activeDrakeCombatDialogue = entry
    activeDrakeCombatDialogueTimer = 0.0
    activeDrakeCombatDialogueDuration =
        (voiceEntry and voiceEntry.duration or entry.duration or defaultDuration) + DRAKE_COMBAT_DIALOGUE_HOLD_PADDING
end

local function enqueue_drake_combat_dialogue(entry)
    if entry == nil then return end
    if activeDrakeCombatDialogue == nil then
        show_drake_combat_dialogue(entry)
    else
        table.insert(drakeCombatDialogueQueue, entry)
    end
end

local function pick_unplayed_drake_combat_dialogue()
    load_drake_combat_dialogue_assets()

    local candidates = {}
    for _, entry in ipairs(drakeCombatDialogueEntries) do
        if entry.id ~= nil and drakeCombatPlayedDialogueIds[entry.id] ~= true then
            table.insert(candidates, entry)
        end
    end

    if #candidates <= 0 then
        return nil
    end

    local index = math.random(1, #candidates)
    local entry = candidates[index]
    drakeCombatPlayedDialogueIds[entry.id] = true
    return entry
end

local function trigger_random_drake_combat_dialogue()
    local entry = pick_unplayed_drake_combat_dialogue()
    if entry ~= nil then
        enqueue_drake_combat_dialogue(entry)
    end
end

local function check_drake_combat_health_dialogue(previousRatio, nextRatio)
    for _, marker in ipairs(DRAKE_COMBAT_HEALTH_MARKERS) do
        if drakeCombatPlayedHealthMarkers[marker] ~= true and previousRatio > marker and nextRatio <= marker then
            drakeCombatPlayedHealthMarkers[marker] = true
            trigger_random_drake_combat_dialogue()
        end
    end
end

local function update_drake_combat_dialogue(dt)
    if activeDrakeCombatDialogue == nil then
        if #drakeCombatDialogueQueue > 0 then
            local nextEntry = table.remove(drakeCombatDialogueQueue, 1)
            show_drake_combat_dialogue(nextEntry)
        end
        return
    end

    activeDrakeCombatDialogueTimer = activeDrakeCombatDialogueTimer + dt

    local defaultFadeIn = drakeCombatStory and drakeCombatStory.default_fade_in or 0.3
    local fadeIn = activeDrakeCombatDialogue.fade_in or defaultFadeIn
    local fadeOut = activeDrakeCombatDialogue.fade_out or DRAKE_COMBAT_DIALOGUE_FADE_OUT
    local remaining = activeDrakeCombatDialogueDuration - activeDrakeCombatDialogueTimer
    local alpha = 1.0

    if fadeIn > 0.0 and activeDrakeCombatDialogueTimer < fadeIn then
        alpha = activeDrakeCombatDialogueTimer / fadeIn
    elseif fadeOut > 0.0 and remaining < fadeOut then
        alpha = math.max(0.0, remaining / fadeOut)
    end

    if WeaponHud ~= nil and WeaponHud.SetDialogueOpacity ~= nil then
        WeaponHud.SetDialogueOpacity(alpha)
    end

    if activeDrakeCombatDialogueTimer >= activeDrakeCombatDialogueDuration then
        if WeaponHud ~= nil and WeaponHud.HideDialogue ~= nil then
            WeaponHud.HideDialogue()
        end
        activeDrakeCombatDialogue = nil
        activeDrakeCombatDialogueTimer = 0.0
        activeDrakeCombatDialogueDuration = 0.0
    end
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

local function get_intro_cutscene_control()
    if _G.Level3BossIntroCutsceneActive ~= true then
        introCutsceneMissingControlLogged = false
        return nil
    end

    -- Level 3 인트로에서 스폰한 보스만 컷씬 제어 대상이 되도록 UUID를 우선 확인합니다.
    local expectedUuid = _G.Level3BossIntroBossUUID
    if expectedUuid ~= nil and obj ~= nil and expectedUuid ~= obj.UUID then
        return nil
    end

    local control = _G.Level3BossIntroBossControl
    if control ~= nil and control.BossUUID ~= nil and obj ~= nil and control.BossUUID ~= obj.UUID then
        return nil
    end

    if control == nil or control.Active ~= true then
        if not introCutsceneMissingControlLogged then
            debug_log("[BossTitanAI] Cutscene flag is active but boss control is missing; continuing combat tick.")
            introCutsceneMissingControlLogged = true
        end
        return nil
    end

    introCutsceneMissingControlLogged = false
    return control
end

function BossTitanAI_ForceExitStaleIntroCutsceneControl(control)
    debug_log(string.format(
        "[BossTitanAI] Forcing stale cutscene control exit. move=%s action=%s serial=%s elapsed=%.2f",
        tostring(control and control.MoveState),
        tostring(control and control.ActionName),
        tostring(control and control.ActionSerial),
        introCutsceneControlTimer))

    _G.Level3BossIntroCutsceneActive = false
    if obj ~= nil and _G.Level3BossIntroBossUUID == obj.UUID then
        _G.Level3BossIntroBossUUID = nil
        _G.Level3BossIntroBossControl = nil
    end

    activeAttack = nil
    leapState = nil
    actionTimer = 0.0
    animationLockTimer = 0.0
    phaseLockTimer = 0.0
    thinkTimer = 0.0
    openingWalkActive = false
    currentTactic = TACTIC.duel
    targetActor = nil
    clear_action_anim(nil)
    set_move_anim(MOVE_IDLE)
    set_aim_anim(0.0, 0.0)
    register_boss_damageable()
    introCutsceneControlTimer = 0.0
    introCutsceneControlLogTimer = 0.0
end

local function apply_intro_cutscene_control(dt)
    local control = get_intro_cutscene_control()
    if control == nil then
        introCutsceneControlLogTimer = 0.0
        introCutsceneControlTimer = 0.0
        return false
    end

    dt = dt or 0.0
    introCutsceneControlTimer = introCutsceneControlTimer + dt
    if introCutsceneControlTimer >= INTRO_CUTSCENE_CONTROL_FAILSAFE_SECONDS then
        BossTitanAI_ForceExitStaleIntroCutsceneControl(control)
        return false
    end

    introCutsceneControlLogTimer = introCutsceneControlLogTimer - dt
    if introCutsceneControlLogTimer <= 0.0 then
        introCutsceneControlLogTimer = 0.8
        debug_log(string.format(
            "[BossTitanAI] Cutscene control active. move=%s action=%s serial=%s forced=%s damageable=%s",
            tostring(control.MoveState),
            tostring(control.ActionName),
            tostring(control.ActionSerial),
            tostring(control.ForcedLocation ~= nil),
            tostring(bBossDamageableRegistered)))
    end

    -- 컷씬 중에는 전투 판단, 공격 판정, 점프 이동을 모두 멈추고 외부 연출 상태만 반영합니다.
    activeAttack = nil
    leapState = nil
    phaseTransitionStage = nil
    phaseTransitionTimer = 0.0
    actionTimer = 0.0
    animationLockTimer = 0.0
    phaseLockTimer = 0.0
    thinkTimer = 0.0
    openingWalkActive = false
    currentTactic = TACTIC.duel

    if control.ForcedLocation ~= nil then
        -- 컷씬 스크립트의 actor handle 대신 보스 AI 자신의 obj.Location을 직접 고정합니다.
        obj.Location = control.ForcedLocation
        homeZ = control.ForcedLocation.Z
    end

    set_aim_anim(control.AimPitch or 0.0, control.AimWeight or 0.0)
    set_move_anim(control.MoveState or MOVE_IDLE)

    local actionName = control.ActionName
    local actionSerial = control.ActionSerial or 0
    if actionName ~= nil and actionSerial ~= 0 and actionSerial ~= introCutsceneLastActionSerial then
        local state = get_anim_state()
        if state ~= nil then
            state.ActionName = actionName
            state.ActionDuration = control.ActionDuration or ACTION_DURATIONS[actionName] or 1.0
            state.ActionSerial = actionSerial
        end

        introCutsceneLastActionSerial = actionSerial
        if actionName == "leapStart" then
            currentAnim = ANIM.leapStart
        elseif actionName == "leapLand" then
            currentAnim = ANIM.leapLand
        end
    end

    return true
end

local function consume_intro_cutscene_release()
    local release = _G.Level3BossIntroBossRelease
    if release == nil then
        return false
    end

    if release.BossUUID ~= nil and obj ~= nil and release.BossUUID ~= obj.UUID then
        return false
    end

    -- 컷씬 종료 직후 첫 전투 프레임의 기준 위치와 전투 상태를 지상 착지 상태로 확정합니다.
    if release.ForcedLocation ~= nil then
        obj.Location = release.ForcedLocation
        homeZ = release.ForcedLocation.Z
    end

    activeAttack = nil
    leapState = nil
    actionTimer = 0.0
    animationLockTimer = 0.0
    phaseLockTimer = 0.0
    thinkTimer = 0.0
    openingWalkActive = false
    currentTactic = TACTIC.duel
    targetActor = nil
    clear_action_anim(nil)
    set_move_anim(MOVE_IDLE)
    set_aim_anim(0.0, 0.0)
    register_boss_damageable()
    debug_log("[BossTitanAI] Intro release consumed. Boss damageable registered for combat.")

    _G.Level3BossIntroBossReleaseConsumedUUID = obj ~= nil and obj.UUID or release.BossUUID
    _G.Level3BossIntroBossRelease = nil
    if obj ~= nil and _G.Level3BossIntroBossUUID == obj.UUID then
        _G.Level3BossIntroBossUUID = nil
        _G.Level3BossIntroBossControl = nil
    end
    _G.Level3BossIntroCutsceneActive = false
    introCutsceneControlTimer = 0.0
    introCutsceneControlLogTimer = 0.0

    return true
end

local function find_target()
    if targetActor ~= nil and targetActor:IsValid() then
        return targetActor
    end

    targetActor = World.FindFirstActorByTag(PLAYER_TAG)
    return targetActor
end

local function get_action_component(actor)
    if actor ~= nil and type(actor.GetActionComponent) == "function" then
        return actor:GetActionComponent()
    end

    return nil
end

local function get_death_slomo_action_component()
    local ownerAction = get_action_component(obj)
    if ownerAction ~= nil then
        return ownerAction
    end

    local target = find_target()
    local targetAction = get_action_component(target)
    if targetAction ~= nil then
        return targetAction
    end

    if Game ~= nil and Game.GetPlayerPawn ~= nil then
        return get_action_component(Game.GetPlayerPawn())
    end

    return nil
end

local function start_death_slomo()
    local action = get_death_slomo_action_component()
    if action == nil then
        debug_log("[BossTitanAI] Death slomo skipped: ActionComponent not found.")
        return false
    end

    local ok, err = pcall(function()
        action:Slomo(DEATH_SLOMO_DURATION, DEATH_SLOMO_TIME_DILATION)
    end)

    if not ok then
        debug_log("[BossTitanAI] Death slomo failed: " .. tostring(err))
        return false
    end

    return true
end

local function yaw_to_forward(yawDegrees)
    local yawRadians = math.rad(yawDegrees or 0.0)
    return Vector.new(math.cos(yawRadians), math.sin(yawRadians), 0.0)
end

local function play_death_fall_camera_shake()
    if CameraManager == nil or CameraManager.StartWaveShake == nil then
        return
    end

    CameraManager.StartWaveShake(DEATH_FALL_SHAKE_SCALE)
end

local function start_boss_music()
    if AudioManager == nil or AudioManager.Load == nil or AudioManager.PlayLoop == nil then
        return false
    end

    if not bBossMusicLoaded then
        bBossMusicLoaded = AudioManager.Load(BOSS_BGM_KEY, BOSS_BGM_PATH, true) == true
        if not bBossMusicLoaded then
            debug_log("[BossTitanAI] Boss music load failed: " .. BOSS_BGM_PATH)
            return false
        end
    end

    bBossMusicPlaying = AudioManager.PlayLoop(BOSS_BGM_KEY, BOSS_BGM_LOOP_NAME, BOSS_BGM_VOLUME, 1.0) == true
    return bBossMusicPlaying
end

function BossTitanAI_SafeStartBossMusic()
    local ok, result = pcall(start_boss_music)
    if not ok then
        debug_log("[BossTitanAI] Boss music start failed with error: " .. tostring(result))
        bBossMusicPlaying = false
        return false
    end
    return result == true
end

local function fade_out_boss_music()
    if not bBossMusicPlaying or AudioManager == nil then
        return
    end

    if AudioManager.FadeOutLoop ~= nil then
        AudioManager.FadeOutLoop(BOSS_BGM_LOOP_NAME, BOSS_BGM_FADE_OUT_MS)
    elseif AudioManager.StopLoop ~= nil then
        AudioManager.StopLoop(BOSS_BGM_LOOP_NAME)
    end

    bBossMusicPlaying = false
end

function BossTitanAI_SafeFadeOutBossMusic()
    local ok, err = pcall(fade_out_boss_music)
    if not ok then
        debug_log("[BossTitanAI] Boss music fade-out failed with error: " .. tostring(err))
        bBossMusicPlaying = false
    end
end

local function stop_boss_music()
    if AudioManager ~= nil and AudioManager.StopLoop ~= nil then
        AudioManager.StopLoop(BOSS_BGM_LOOP_NAME)
    end
    bBossMusicPlaying = false
end

local function start_death_fall()
    local startRotation = obj.Rotation
    local startLocation = obj.Location
    local fallForward = yaw_to_forward(startRotation.Z)

    deathFallState = {
        elapsed = 0.0,
        duration = DEATH_FALL_DURATION,
        startLocation = startLocation,
        targetLocation = Vector.new(
            startLocation.X + fallForward.X * DEATH_FALL_FORWARD_DISTANCE,
            startLocation.Y + fallForward.Y * DEATH_FALL_FORWARD_DISTANCE,
            (homeZ or startLocation.Z) + DEATH_FALL_GROUND_CLEARANCE),
        startRotation = startRotation,
        targetRotation = Vector.new(
            startRotation.X,
            startRotation.Y + DEATH_FALL_PITCH_DEGREES,
            startRotation.Z),
    }

    return true
end

local function get_first_spawned_actor(spawnResult)
    if spawnResult == nil then
        return nil
    end
    if spawnResult.UUID ~= nil then
        return spawnResult
    end
    return spawnResult[1]
end

local function register_card_key_actor(cardKey)
    if cardKey == nil or cardKey.UUID == nil then
        return
    end

    _G.ItemActorIds = _G.ItemActorIds or {}
    _G.ItemActorIds[cardKey.UUID] = CARD_KEY_ITEM_ID

    _G.InspectableItems = _G.InspectableItems or {}
    _G.InspectableItems[cardKey.UUID] = {
        actor = cardKey,
        item_id = CARD_KEY_ITEM_ID,
    }
end

local function spawn_card_key_reward()
    if bSpawnedCardKey
        or (_G.Level3CardKeySpawned == true)
        or (_G.PickedUpItems ~= nil and _G.PickedUpItems[CARD_KEY_ITEM_ID] == true) then
        return
    end
    bSpawnedCardKey = true

    if World == nil or World.SpawnPrefab == nil then
        debug_log("[BossTitanAI] Card key spawn skipped: World.SpawnPrefab unavailable.")
        return
    end

    local spawnLocation = obj.Location + CARD_KEY_SPAWN_OFFSET
    local cardKey = get_first_spawned_actor(World.SpawnPrefab(CARD_KEY_PREFAB_PATH, spawnLocation))
    if cardKey == nil then
        debug_log("[BossTitanAI] Card key prefab spawn failed: " .. CARD_KEY_PREFAB_PATH)
        return
    end

    _G.Level3CardKeySpawned = true
    register_card_key_actor(cardKey)
    debug_log(string.format(
        "[BossTitanAI] Spawned card key reward at (%.2f, %.2f, %.2f).",
        spawnLocation.X,
        spawnLocation.Y,
        spawnLocation.Z))
end

function BossTitanAI_SafeSpawnCardKeyReward()
    local ok, err = pcall(spawn_card_key_reward)
    if not ok then
        debug_log("[BossTitanAI] Card key reward spawn failed with error: " .. tostring(err))
    end
end

local function update_death_fall(dt)
    if deathFallState == nil then
        return
    end

    deathFallState.elapsed = math.min(deathFallState.elapsed + dt, deathFallState.duration)
    local alpha = 1.0
    if deathFallState.duration > 0.0 then
        alpha = smoothstep(deathFallState.elapsed / deathFallState.duration)
    end

    obj.Location = lerp(deathFallState.startLocation, deathFallState.targetLocation, alpha)
    obj.Rotation = lerp(deathFallState.startRotation, deathFallState.targetRotation, alpha)

    if deathFallState.elapsed >= deathFallState.duration then
        obj.Location = deathFallState.targetLocation
        obj.Rotation = deathFallState.targetRotation
        play_death_fall_camera_shake()
        deathFallState = nil
        BossTitanAI_SafeSpawnCardKeyReward()
    end
end

local function start_death_sequence()
    _G.Level3BossDefeated = true
    BossTitanAI_SafeFadeOutBossMusic()
    activeAttack = nil
    leapState = nil
    openingWalkActive = false
    actionTimer = 0.0
    animationLockTimer = 0.0
    phaseLockTimer = 0.0
    set_move_anim(MOVE_IDLE)
    set_aim_anim(0.0, 0.0)
    request_action_anim("hitReact", DEATH_FALLBACK_ANIM_DURATION)
    currentAnim = ANIM.hitReact

    start_death_slomo()
    start_death_fall()
end

local function get_muzzle_location()
    if frameMuzzleLocation ~= nil then
        return frameMuzzleLocation
    end

    if mesh ~= nil then
        local muzzle = mesh:GetBoneSocketLocation(MUZZLE_BONE, MUZZLE_LOCAL_OFFSET)
        if muzzle ~= nil and muzzle:Length() > 0.001 then
            frameMuzzleLocation = muzzle
            return muzzle
        end
    end

    frameMuzzleLocation = obj.Location + Vector.new(0.0, 0.0, FALLBACK_MUZZLE_HEIGHT)
    return frameMuzzleLocation
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

local function has_line_of_sight(target, source, targetPos, forceRefresh)
    if target == nil then return false end

    local targetUUID = target.UUID
    if not forceRefresh
        and lineOfSightTimer > 0.0
        and lineOfSightTargetUUID == targetUUID then
        return lineOfSightResult
    end

    source = source or get_muzzle_location()
    targetPos = targetPos or get_target_aim_location(target)
    local toTarget = targetPos - source
    local distance = toTarget:Length()
    if distance <= 0.001 or distance > SIGHT_RANGE then
        lineOfSightTargetUUID = targetUUID
        lineOfSightResult = false
        lineOfSightTimer = LOS_CHECK_INTERVAL
        return false
    end

    local hit = World.RaycastWorldStatic(source, toTarget:Normalized(), distance, obj)
    lineOfSightTargetUUID = targetUUID
    lineOfSightResult = hit == nil
    lineOfSightTimer = LOS_CHECK_INTERVAL
    return lineOfSightResult
end

local function can_use_attack_los_fallback(distance, verticalDelta)
    return distance <= ATTACK_LOS_FALLBACK_RANGE
        and math.abs(verticalDelta) <= LEAP_MAX_VERTICAL_DELTA
end

local function can_use_leap_los_fallback(distance, verticalDelta)
    return distance >= LEAP_MIN_RANGE
        and distance <= LEAP_MAX_RANGE
        and math.abs(verticalDelta) <= LEAP_START_MAX_VERTICAL_DELTA
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
        bBoss = true,
        boss = true,
        bCritical = false,
        DamageApplied = amount or 0.0,
        RemainingHealth = currentHealth,
        MaxHealth = MAX_HEALTH,
        HealthRatio = health_ratio(),
        Victim = obj,
    }
end

register_boss_damageable = function()
    if not bBossDamageableRegistered and damageableCallbacks ~= nil then
        bBossDamageableRegistered = CombatEvents.RegisterDamageable(obj, damageableCallbacks) == true
        if not bBossDamageableRegistered then
            debug_log("[BossTitanAI] Failed to register boss damageable. uuid=" .. tostring(obj and obj.UUID))
        elseif CombatEvents ~= nil and type(CombatEvents.RegisterDamageableAlias) == "function" then
            _G.Level3BossDamageActor = obj

            local gunActor = _G.Level3BossGunActor
            local gunActorValid = gunActor ~= nil
            if gunActorValid and type(gunActor.IsValid) == "function" then
                gunActorValid = gunActor:IsValid()
            end
            if gunActorValid then
                CombatEvents.RegisterDamageableAlias(gunActor, obj)
                if type(gunActor.AddTag) == "function" then
                    gunActor:AddTag("enemy")
                    gunActor:AddTag("boss")
                end
            end
        end
    end
end

local function unregister_boss_damageable()
    if bBossDamageableRegistered then
        if CombatEvents ~= nil and type(CombatEvents.UnregisterDamageableAlias) == "function" then
            local gunActor = _G.Level3BossGunActor
            local gunActorValid = gunActor ~= nil
            if gunActorValid and type(gunActor.IsValid) == "function" then
                gunActorValid = gunActor:IsValid()
            end
            if gunActorValid then
                CombatEvents.UnregisterDamageableAlias(gunActor)
            end
        end
        if _G.Level3BossDamageActor == obj then
            _G.Level3BossDamageActor = nil
        end
        CombatEvents.UnregisterDamageable(obj)
        bBossDamageableRegistered = false
    end
end

function BossTitanAI_EnsureBossDamageableRegistered(reason)
    if bBossDamageableRegistered or damageableCallbacks == nil then
        return
    end

    register_boss_damageable()
    if bBossDamageableRegistered then
        debug_log("[BossTitanAI] Boss damageable registered by fail-safe: " .. tostring(reason))
    end
end

local function begin_phase_transition_stage(stageName, duration)
    phaseTransitionStage = stageName
    phaseTransitionTimer = duration

    activeAttack = nil
    leapState = nil
    actionTimer = duration
    animationLockTimer = duration
    phaseLockTimer = duration
    set_move_anim(MOVE_IDLE)
    set_aim_anim(0.0, 0.0)
    request_action_anim(stageName, duration)
    currentAnim = ANIM.crouchStand
end

local function start_phase_two_transition()
    if isDead or phase >= 2 or phaseTransitionStage ~= nil then
        return false
    end

    pendingPhase = nil
    openingWalkActive = false
    currentTactic = TACTIC.duel
    unregister_boss_damageable()
    begin_phase_transition_stage("phaseCrouchDown", PHASE_TWO_TRANSITION_ANIM_DURATION)
    debug_log("[BossTitanAI] Phase 1 depleted. Starting crouch transition to Phase 2.")
    return true
end

local function update_phase_two_transition(dt)
    if phaseTransitionStage == nil then
        return false
    end

    phaseTransitionTimer = phaseTransitionTimer - dt
    set_move_anim(MOVE_IDLE)
    set_aim_anim(0.0, 0.0)

    if phaseTransitionTimer > 0.0 then
        return true
    end

    if phaseTransitionStage == "phaseCrouchDown" then
        begin_phase_transition_stage("phaseCrouchHold", PHASE_TWO_CROUCH_HOLD_DURATION)
        return true
    end

    if phaseTransitionStage == "phaseCrouchHold" then
        begin_phase_transition_stage("phaseStandUp", PHASE_TWO_TRANSITION_ANIM_DURATION)
        return true
    end

    if phaseTransitionStage == "phaseStandUp" then
        phaseTransitionStage = nil
        phaseTransitionTimer = 0.0
        phase = 2
        pendingPhase = nil
        actionTimer = 0.0
        animationLockTimer = 0.0
        phaseLockTimer = 0.0
        thinkTimer = 0.0
        cooldowns.cannon = 0.4
        cooldowns.powerShot = 1.2
        cooldowns.melee = 0.5
        apply_boss_phase_visual(phase)
        clear_action_anim("phaseStandUp")
        register_boss_damageable()
        debug_log("[BossTitanAI] Phase 2 started after crouch transition.")
        return true
    end

    phaseTransitionStage = nil
    phaseTransitionTimer = 0.0
    return false
end

local function enter_phase(nextPhase)
    if nextPhase <= phase then return end

    if activeAttack ~= nil or is_animation_locked() then
        pendingPhase = math.max(pendingPhase or nextPhase, nextPhase)
        return
    end

    phase = nextPhase
    pendingPhase = nil
    apply_boss_phase_visual(phase)
    phaseLockTimer = 1.0
    actionTimer = phaseLockTimer
    animationLockTimer = phaseLockTimer
    activeAttack = nil
    leapState = nil

    cooldowns.cannon = 0.4
    cooldowns.powerShot = 1.2
    cooldowns.melee = 0.5

    play_anim(ANIM.phase, false, true)
end

local function update_phase_from_health()
    local ratio = health_ratio()
    if phase == 1 and ratio <= PHASE_TWO_HEALTH_RATIO then
        start_phase_two_transition()
    end
end

local function apply_boss_damage(context)
    local amount = context ~= nil and (context.Damage or context.damage) or 0.0
    if amount <= 0.0 or isDead or phaseTransitionStage ~= nil then
        return make_damage_result(false, 0.0, false)
    end

    local previousRatio = health_ratio()
    currentHealth = clamp(currentHealth - amount, 0.0, MAX_HEALTH)
    check_drake_combat_health_dialogue(previousRatio, health_ratio())

    local killed = currentHealth <= 0.0
    if killed then
        isDead = true
        start_death_sequence()
        unregister_boss_damageable()
        debug_log("[BossTitanAI] Titan boss defeated. Death slomo and fall started.")
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
    set_move_anim(MOVE_IDLE)
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
        if not has_line_of_sight(target, source, targetPos, true)
            and not can_use_attack_los_fallback(dist, target.Location.Z - obj.Location.Z) then
            return
        end
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
        clear_action_anim(activeAttack.kind)
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
            clear_action_anim("leapLand")
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
    local lineOfSight = has_line_of_sight(target, muzzle, aimLocation, false)
    local attackLineOfSight = lineOfSight or can_use_attack_los_fallback(dist, verticalDelta)
    local leapLineOfSight = lineOfSight or can_use_leap_los_fallback(dist, verticalDelta)
    local canLeap = cooldowns.leap <= 0.0
        and leapLineOfSight
        and dist >= LEAP_MIN_RANGE
        and dist <= LEAP_MAX_RANGE
        and math.abs(verticalDelta) <= LEAP_START_MAX_VERTICAL_DELTA

    blackboard.target = target
    blackboard.distance = dist
    blackboard.aimDistance = aimDelta:Length()
    blackboard.verticalDelta = verticalDelta
    blackboard.canGroundMelee = canGroundMelee
    blackboard.canLeap = canLeap
    blackboard.isClose = dist <= CLOSE_RETREAT_RANGE or canGroundMelee
    blackboard.isDuelRange = dist > CLOSE_RETREAT_RANGE and dist <= KEEP_RANGE + 8.0
    blackboard.isFar = dist > KEEP_RANGE + 8.0
    blackboard.healthRatio = health_ratio()
    blackboard.phase = phase
    blackboard.lineOfSight = lineOfSight
    blackboard.attackLineOfSight = attackLineOfSight
    blackboard.leapLineOfSight = leapLineOfSight

    return blackboard
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
    if cooldowns.cannon > 0.0 or not bb.attackLineOfSight then return -1000.0 end
    if bb.canGroundMelee then return -1000.0 end
    if bb.distance < FIRE_MIN_RANGE and math.abs(bb.verticalDelta) <= MELEE_MAX_VERTICAL_DELTA then return -1000.0 end
    if bb.aimDistance > CANNON_RANGE then return -1000.0 end

    local rangeFit = 1.0 - math.abs(bb.distance - KEEP_RANGE) / KEEP_RANGE
    local losPenalty = bb.lineOfSight and 0.0 or -1.2
    return 4.0 + rangeFit * 3.0 + phase * 0.7 + losPenalty + random01()
end

local function score_power_shot(bb)
    if cooldowns.powerShot > 0.0 or phase < 2 or not bb.attackLineOfSight then return -1000.0 end
    if bb.canGroundMelee then return -1000.0 end
    if bb.distance < 18.0 and math.abs(bb.verticalDelta) <= MELEE_MAX_VERTICAL_DELTA then return -1000.0 end
    if bb.aimDistance > CANNON_RANGE then return -1000.0 end

    local losPenalty = bb.lineOfSight and 0.0 or -1.5
    return 5.0 + phase * 1.4 + (1.0 - bb.healthRatio) * 2.0 + losPenalty + random01()
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
        start_attack("powerShot", ANIM.powerShot, POWER_SHOT_ACTION_LOCK, 0.55, CANNON_RANGE, 24.0 + phase * 9.0, 2.8, POWER_SHOT_ACTION_LOCK)
        return true
    end

    if name == "cannon" then
        start_attack("cannon", ANIM.cannon, CANNON_ACTION_LOCK, 0.22, CANNON_RANGE, 11.0 + phase * 4.0, 0.85, CANNON_ACTION_LOCK)
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
        local leapChance = 0.42
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
        if can_start_leap(bb) and random01() < 0.35 then
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
        move_horizontal(dirToTarget, APPROACH_RUN_SPEED, dt)
        play_anim(bb.distance > APPROACH_RUN_DISTANCE and ANIM.run or ANIM.walk, true, false)
        return
    end

    if bb.distance > KEEP_RANGE - 3.0 then
        move_horizontal(dirToTarget, APPROACH_RUN_SPEED, dt)
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
    load_drake_combat_dialogue_assets()
    reset_drake_combat_dialogue_state()
    apply_boss_phase_visual(1)
    homeZ = obj.Location.Z
    currentHealth = MAX_HEALTH
    isDead = false
    phase = 1
    pendingPhase = nil
    phaseTransitionStage = nil
    phaseTransitionTimer = 0.0
    actionTimer = 0.0
    animationLockTimer = 0.0
    phaseLockTimer = 0.0
    thinkTimer = 0.0
    activeAttack = nil
    leapState = nil
    deathFallState = nil
    introCutsceneMissingControlLogged = false
    introCutsceneControlLogTimer = 0.0
    introCutsceneControlTimer = 0.0
    bBossDamageableRegistered = false
    bSpawnedCardKey = false
    bBossMusicPlaying = false
    currentAnim = nil
    currentTactic = TACTIC.openingWalk
    openingWalkActive = true
    tacticCommitTimer = 0.0
    tacticReevaluateTimer = 0.0
    introCutsceneLastActionSerial = 0
    reset_anim_state()
    cooldowns.cannon = 0.0
    cooldowns.powerShot = 1.4
    cooldowns.melee = 0.0
    cooldowns.retreat = 0.0
    cooldowns.leap = 2.5
    _G.Level3BossDefeated = false
    if _G.PickedUpItems == nil or _G.PickedUpItems[CARD_KEY_ITEM_ID] ~= true then
        _G.Level3CardKeySpawned = false
    end

    if _G.Level3BossIntroCutsceneActive == true then
        -- 인트로 컷씬 중 스폰된 보스는 기존 오프닝 접근 루틴 없이 바로 전투 상태로 전환될 준비만 해둡니다.
        openingWalkActive = false
        currentTactic = TACTIC.duel
    end

    obj:AddTag("enemy")
    obj:AddTag("boss")
    damageableCallbacks = {
        ApplyDamage = apply_boss_damage,
        IsDead = function() return isDead end,
    }
    register_boss_damageable()

    debug_open_log()
    _G.BossDebugLog = debug_log
    debugSessionTime = 0.0

    play_anim(ANIM.idle, true, true)
    BossTitanAI_SafeStartBossMusic()
    debug_log("[BossTitanAI] Titan boss online.")
end

function EndPlay()
    if obj ~= nil and _G.BossTitanAnimState ~= nil then
        _G.BossTitanAnimState[obj.UUID] = nil
    end

    reset_drake_combat_dialogue_state()
    if obj ~= nil and _G.Level3BossIntroBossUUID == obj.UUID then
        _G.Level3BossIntroBossUUID = nil
        _G.Level3BossIntroBossControl = nil
    end
    if obj ~= nil and _G.Level3BossIntroBossRelease ~= nil and _G.Level3BossIntroBossRelease.BossUUID == obj.UUID then
        _G.Level3BossIntroBossRelease = nil
    end

    unregister_boss_damageable()
    damageableCallbacks = nil
    stop_boss_music()
    debug_close_log()
end

function Tick(dt)
    update_drake_combat_dialogue(dt)

    if isDead then
        update_death_fall(dt)
        set_move_anim(MOVE_IDLE)
        set_aim_anim(0.0, 0.0)
        return
    end

    consume_intro_cutscene_release()

    BossTitanAI_EnsureBossDamageableRegistered("tick")

    if apply_intro_cutscene_control(dt) then
        return
    end

    tick_timers(dt)
    debugSessionTime = debugSessionTime + dt
    debugLogTimer = debugLogTimer - dt

    if update_phase_two_transition(dt) then
        return
    end

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
            "[%.2f][BossAI] tac=%s dist=%.1f aim=%.1f z=%.1f los=%s atkLos=%s leapLos=%s canLeap=%s melee=%s anim=%s move=%s act=%s ser=%d lock=%.2f actT=%.2f leap=%s open=%s loc=(%.1f,%.1f,%.1f)",
            debugSessionTime,
            tostring(currentTactic),
            bb.distance,
            bb.aimDistance,
            bb.verticalDelta,
            tostring(bb.lineOfSight),
            tostring(bb.attackLineOfSight),
            tostring(bb.leapLineOfSight),
            tostring(bb.canLeap),
            tostring(bb.canGroundMelee),
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
