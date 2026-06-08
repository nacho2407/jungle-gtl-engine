local CombatEvents = require("Game.CombatEvents")
local WeaponHud = require("HUD/WeaponHud")

local STORY_MODULE = "Dialogue/DrakePreCombat.dialogue"
local VOICE_MODULE = "Dialogue/Generated/DrakePreCombat.voices"

local MAX_HEALTH = 100.0
local SIGHT_RANGE = 18.0
local SIGHT_HALF_FOV_DOT = 0.55
local ENEMY_CLEAR_RADIUS = 35.0
local EYE_HEIGHT = 1.65
local DIALOGUE_END_PADDING = 0.35
local CUTSCENE_LOCK_INPUT = true

local currentHealth = MAX_HEALTH
local cutsceneStarted = false
local cutscenePending = false
local activeEntry = nil
local activeVoiceKey = nil
local entryTimer = 0.0
local entryDuration = 0.0
local dialogueQueue = {}
local story = nil
local storyEntriesById = {}
local voiceEntriesById = {}
local loadedVoiceKeys = {}

local lockedMovement = nil
local savedMaxWalkSpeed = nil
local savedSprintSpeedMultiplier = nil
local savedWallRunMaxSpeed = nil
local bMovementLocked = false

local dialogueSequence = {
    "DrakePreCombat_System_DriveAutoPlay",
    "DrakePreCombat_System_LancerDocConfirmed",
    "DrakePreCombat_System_LancerAccessDenied",
    "DrakePreCombat_Drake_Cleanup",
    "DrakePreCombat_Kain_Refuse",
    "DrakePreCombat_Drake_RepeatThat",
    "DrakePreCombat_Kain_RefuseAgain",
    "DrakePreCombat_Drake_Acknowledged",
    "DrakePreCombat_Drake_StayPut",
}

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function is_valid_actor(actor)
    if actor == nil then return false end
    if type(actor.IsValid) == "function" then
        return actor:IsValid()
    end
    return true
end

local function same_actor(a, b)
    if not is_valid_actor(a) or not is_valid_actor(b) then return false end
    if a == b then return true end
    return a.UUID ~= nil and b.UUID ~= nil and a.UUID == b.UUID
end

local function horizontal(v)
    return Vector.new(v.X or 0.0, v.Y or 0.0, 0.0)
end

local function horizontal_distance(a, b)
    if a == nil or b == nil then
        return math.huge
    end
    return horizontal(a - b):Length()
end

local function normalized_or_zero(v)
    if v == nil or v:Length() <= 0.001 then
        return Vector.new(0.0, 0.0, 0.0)
    end
    return v:Normalized()
end

local function get_player_actor()
    if Game ~= nil and Game.GetPlayerPawn ~= nil then
        local pawn = Game.GetPlayerPawn()
        if is_valid_actor(pawn) then
            return pawn
        end
    end

    if World ~= nil and World.FindActorByName ~= nil then
        local player = World.FindActorByName("kain-temp")
        if is_valid_actor(player) then
            return player
        end
    end

    return nil
end

local function actor_has_tag(actor, tag)
    if not is_valid_actor(actor) or type(actor.HasTag) ~= "function" then
        return nil
    end

    local ok, result = pcall(function()
        return actor:HasTag(tag)
    end)
    if not ok then
        return nil
    end
    return result == true
end

local function is_entry_dead(entry)
    if entry == nil or entry.IsDead == nil then
        return false
    end

    local ok, result = pcall(entry.IsDead)
    return ok and result == true
end

local function are_nearby_enemies_cleared()
    if CombatEvents == nil or CombatEvents._damageables == nil or not is_valid_actor(obj) then
        return true
    end

    local origin = obj.Location
    local player = get_player_actor()
    for _, entry in pairs(CombatEvents._damageables) do
        local actor = entry ~= nil and entry.Actor or nil
        if is_valid_actor(actor)
            and not same_actor(actor, obj)
            and not same_actor(actor, player)
            and not is_entry_dead(entry)
            and horizontal_distance(origin, actor.Location) <= ENEMY_CLEAR_RADIUS then

            local isEnemy = actor_has_tag(actor, "enemy")
            if isEnemy == true or isEnemy == nil then
                return false
            end
        end
    end

    return true
end

local function get_actor_eye_location(actor)
    local location = actor ~= nil and actor.Location or Vector.new(0.0, 0.0, 0.0)
    return location + Vector.new(0.0, 0.0, EYE_HEIGHT)
end

local function can_see_player(player)
    if not is_valid_actor(obj) or not is_valid_actor(player) then
        return false
    end

    local source = get_actor_eye_location(obj)
    local target = get_actor_eye_location(player)
    local toTarget = target - source
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

    if World ~= nil and World.RaycastWorldStatic ~= nil then
        local hit = World.RaycastWorldStatic(source, toTarget:Normalized(), distance, obj)
        if hit ~= nil and not same_actor(hit.HitActor, player) then
            return false
        end
    end

    return true
end

local function load_dialogue_assets()
    story = require(STORY_MODULE)
    storyEntriesById = {}
    if story ~= nil and story.entries ~= nil then
        for _, entry in ipairs(story.entries) do
            if entry.id ~= nil then
                storyEntriesById[entry.id] = entry
            end
        end
    end

    local ok, voices = pcall(require, VOICE_MODULE)
    voiceEntriesById = ok and voices ~= nil and voices.by_id or {}
end

local function get_dialogue_text(entry)
    if entry == nil then return "" end
    local speaker = entry.speaker or ""
    local text = entry.text or entry.text_en or ""
    if speaker == "" then
        return text
    end
    return speaker .. ": " .. text
end

local function stop_current_voice()
    if activeVoiceKey ~= nil and AudioManager ~= nil and AudioManager.Stop ~= nil then
        AudioManager.Stop(activeVoiceKey)
    end
    activeVoiceKey = nil
end

local function play_voice(entry)
    if entry == nil or AudioManager == nil or AudioManager.Load == nil or AudioManager.Play == nil then
        return nil
    end

    local voiceEntry = voiceEntriesById[entry.id]
    if voiceEntry == nil or voiceEntry.key == nil or voiceEntry.path == nil then
        return nil
    end

    stop_current_voice()
    if not loadedVoiceKeys[voiceEntry.key] then
        if not AudioManager.Load(voiceEntry.key, voiceEntry.path, false) then
            return nil
        end
        loadedVoiceKeys[voiceEntry.key] = true
    end

    AudioManager.Play(voiceEntry.key, voiceEntry.volume or 1.0)
    activeVoiceKey = voiceEntry.key
    return voiceEntry
end

local function show_entry(entry)
    if entry == nil then return end

    local text = get_dialogue_text(entry)
    local fontSize = entry.size or (story ~= nil and story.default_size) or 22
    local lineHeight = fontSize + 26
    local width = clamp(42.0 + string.len(text) * fontSize * 0.52, 520.0, 1280.0)
    local height = clamp(lineHeight, 46.0, 72.0)

    if WeaponHud ~= nil and WeaponHud.ShowDialogue ~= nil then
        WeaponHud.ShowDialogue(text, {
            width = width,
            height = height,
            font = entry.font or (story ~= nil and story.default_font) or "Pretendard",
            fontSize = fontSize,
            weight = entry.weight or (story ~= nil and story.default_weight) or 700,
            lineHeight = height,
            opacity = 0.0,
        })
    end

    local voiceEntry = play_voice(entry)
    activeEntry = entry
    entryTimer = 0.0
    entryDuration = voiceEntry ~= nil and voiceEntry.duration ~= nil
        and (voiceEntry.duration + DIALOGUE_END_PADDING)
        or (entry.duration or (story ~= nil and story.default_duration) or 3.2)
end

local function restore_player_after_cutscene()
    if Game ~= nil and Game.SetInputPossessed ~= nil then
        Game.SetInputPossessed(true)
    end
    if Game ~= nil and Game.SetMouseCaptureWhileInputBlocked ~= nil then
        Game.SetMouseCaptureWhileInputBlocked(false)
    end
    if bMovementLocked and lockedMovement ~= nil then
        if savedMaxWalkSpeed ~= nil then lockedMovement:SetMaxWalkSpeed(savedMaxWalkSpeed) end
        if savedSprintSpeedMultiplier ~= nil then lockedMovement:SetSprintSpeedMultiplier(savedSprintSpeedMultiplier) end
        if savedWallRunMaxSpeed ~= nil then lockedMovement:SetWallRunMaxSpeed(savedWallRunMaxSpeed) end
    end
    bMovementLocked = false
    lockedMovement = nil
    savedMaxWalkSpeed = nil
    savedSprintSpeedMultiplier = nil
    savedWallRunMaxSpeed = nil
end

local function show_next_entry()
    stop_current_voice()
    local nextId = table.remove(dialogueQueue, 1)
    if nextId == nil then
        activeEntry = nil
        if WeaponHud ~= nil and WeaponHud.HideDialogue ~= nil then
            WeaponHud.HideDialogue()
        end
        if WeaponHud ~= nil and WeaponHud.HideLetterbox ~= nil then
            WeaponHud.HideLetterbox()
        end
        if CUTSCENE_LOCK_INPUT then
            restore_player_after_cutscene()
        end
        return
    end

    show_entry(storyEntriesById[nextId])
end

local function lock_player_for_cutscene()
    if not CUTSCENE_LOCK_INPUT then return end

    if Game ~= nil and Game.SetInputPossessed ~= nil then
        if Game.SetMouseCaptureWhileInputBlocked ~= nil then
            Game.SetMouseCaptureWhileInputBlocked(true)
        end
        Game.SetInputPossessed(false)
    end

    local player = get_player_actor()
    if player == nil or player.GetCharacterMovement == nil then return end

    lockedMovement = player:GetCharacterMovement()
    if lockedMovement == nil or bMovementLocked then return end

    savedMaxWalkSpeed = lockedMovement:GetMaxWalkSpeed()
    savedSprintSpeedMultiplier = lockedMovement:GetSprintSpeedMultiplier()
    savedWallRunMaxSpeed = lockedMovement:GetWallRunMaxSpeed()
    lockedMovement:SetMaxWalkSpeed(0.0)
    lockedMovement:SetSprintSpeedMultiplier(0.0)
    lockedMovement:SetWallRunMaxSpeed(0.0)
    bMovementLocked = true
end

local function start_cutscene()
    if cutsceneStarted then
        return
    end

    cutscenePending = false
    cutsceneStarted = true
    lock_player_for_cutscene()
    if WeaponHud ~= nil and WeaponHud.ShowLetterbox ~= nil then
        WeaponHud.ShowLetterbox()
    end
    dialogueQueue = {}
    for _, id in ipairs(dialogueSequence) do
        table.insert(dialogueQueue, id)
    end
    show_next_entry()
end

local function request_cutscene()
    if cutsceneStarted then
        return
    end

    cutscenePending = true
    if are_nearby_enemies_cleared() then
        start_cutscene()
    end
end

local function make_damage_result(applied, damageApplied)
    return {
        bApplied = applied == true,
        bKilled = false,
        bCritical = false,
        DamageApplied = damageApplied or 0.0,
        RemainingHealth = currentHealth,
        MaxHealth = MAX_HEALTH,
        HealthRatio = MAX_HEALTH > 0.0 and currentHealth / MAX_HEALTH or 0.0,
        Victim = obj,
    }
end

local function apply_damage(context)
    local amount = context ~= nil and (context.Damage or context.damage) or 0.0
    if amount > 0.0 then
        currentHealth = clamp(currentHealth - amount, 0.0, MAX_HEALTH)
    end
    request_cutscene()
    return make_damage_result(amount > 0.0, amount)
end

function BeginPlay()
    currentHealth = MAX_HEALTH
    cutsceneStarted = false
    cutscenePending = false
    activeEntry = nil
    activeVoiceKey = nil
    entryTimer = 0.0
    entryDuration = 0.0
    dialogueQueue = {}
    loadedVoiceKeys = {}
    bMovementLocked = false
    lockedMovement = nil

    load_dialogue_assets()
    if obj ~= nil then
        obj:AddTag("MCCURSON")
        obj:AddTag("enemy")
        CombatEvents.RegisterDamageable(obj, {
            ApplyDamage = apply_damage,
            IsDead = function() return false end,
        })
    end
end

function EndPlay()
    stop_current_voice()
    restore_player_after_cutscene()
    if WeaponHud ~= nil and WeaponHud.HideDialogue ~= nil then
        WeaponHud.HideDialogue()
    end
    if WeaponHud ~= nil and WeaponHud.HideLetterbox ~= nil then
        WeaponHud.HideLetterbox()
    end
    if obj ~= nil then
        CombatEvents.UnregisterDamageable(obj)
    end
end

function OnHit(OtherActor, HitComponent, OtherComp, NormalImpulse, HitResult)
    request_cutscene()
end

function Tick(dt)
    dt = dt or 0.0

    if not cutsceneStarted then
        local player = get_player_actor()
        if can_see_player(player) then
            request_cutscene()
        elseif cutscenePending and are_nearby_enemies_cleared() then
            start_cutscene()
        end
    end

    if activeEntry == nil then
        return
    end

    entryTimer = entryTimer + dt
    local fadeIn = activeEntry.fade_in or (story ~= nil and story.default_fade_in) or 0.2
    local fadeOut = 0.35
    local alpha = 1.0
    if entryTimer < fadeIn then
        alpha = entryTimer / fadeIn
    elseif entryTimer > entryDuration - fadeOut then
        alpha = (entryDuration - entryTimer) / fadeOut
    end
    alpha = clamp(alpha, 0.0, 1.0)

    if WeaponHud ~= nil and WeaponHud.SetDialogueOpacity ~= nil then
        WeaponHud.SetDialogueOpacity(alpha)
    end

    if entryTimer >= entryDuration then
        show_next_entry()
    end
end
