local WeaponHud = require("HUD/WeaponHud")

local STORY_MODULE = "Dialogue/DrakePreCombat.dialogue"
local VOICE_MODULE = "Dialogue/Generated/DrakePreCombat.voices"

local PLAYER_PAWN_NAME = "kain-temp"
local INTERACT_TEXT = "ACCESS"
local VOICE_END_PADDING = 0.35
local DEFAULT_ENTRY_DURATION = 2.5
local DEFAULT_FADE_IN = 0.3
local FADE_OUT_DURATION = 0.35
local LETTERBOX_CLOSE_DELAY = 0.35
local SKIP_HOLD_DURATION = 3.0

local story = nil
local voiceEntries = {}
local voiceEntriesById = {}
local loadedVoiceKeys = {}
local activeEntry = nil
local activeVoiceKey = nil
local currentIndex = 0
local entryTimer = 0.0
local entryDuration = 0.0
local skipHoldTime = 0.0

local cutsceneStarted = false
local cutsceneActive = false
local cutsceneClosing = false
local closeTimer = 0.0

local inputLocked = false
local movementLocked = false
local autoInputLocked = false
local lockedMovement = nil
local savedMaxWalkSpeed = nil
local savedSprintSpeedMultiplier = nil
local savedWallRunMaxSpeed = nil
local savedAutoInputWASD = nil
local savedAutoInputMouseLook = nil

local start_cutscene = nil

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function get_key(name)
    if Key == nil then return nil end
    return Key[name]
end

local function get_raw_key(key)
    if key == nil or Input == nil then return false end
    if Input.GetRawKey ~= nil then return Input.GetRawKey(key) end
    if Input.GetKey ~= nil then return Input.GetKey(key) end
    return false
end

local function is_skip_held()
    return get_raw_key(get_key("Ctrl"))
        or get_raw_key(get_key("LeftCtrl"))
        or get_raw_key(get_key("RightCtrl"))
        or get_raw_key(get_key("GamepadB"))
end

local function is_valid_actor(actor)
    if actor == nil then return false end
    if type(actor.IsValid) == "function" then
        return actor:IsValid()
    end
    return true
end

local function get_player_actor()
    if Game ~= nil and Game.GetPlayerPawn ~= nil then
        local pawn = Game.GetPlayerPawn()
        if is_valid_actor(pawn) then
            return pawn
        end
    end

    if World ~= nil and World.FindActorByName ~= nil then
        local player = World.FindActorByName(PLAYER_PAWN_NAME)
        if is_valid_actor(player) then
            return player
        end
    end

    return nil
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

local function load_dialogue_assets()
    story = require(STORY_MODULE)
    voiceEntries = {}
    voiceEntriesById = {}

    local ok, voices = pcall(require, VOICE_MODULE)
    if ok and voices ~= nil then
        voiceEntries = voices.entries or {}
        voiceEntriesById = voices.by_id or {}
    end
end

local function get_voice_entry(entry, index)
    if entry ~= nil and entry.id ~= nil and voiceEntriesById ~= nil then
        local byIdEntry = voiceEntriesById[entry.id]
        if byIdEntry ~= nil then
            return byIdEntry
        end
    end
    return voiceEntries ~= nil and voiceEntries[index] or nil
end

local function stop_current_voice()
    if activeVoiceKey ~= nil and AudioManager ~= nil and AudioManager.Stop ~= nil then
        AudioManager.Stop(activeVoiceKey)
    end
    activeVoiceKey = nil
end

local function play_voice(entry, index)
    if AudioManager == nil or AudioManager.Load == nil or AudioManager.Play == nil then
        return nil
    end

    local voiceEntry = get_voice_entry(entry, index)
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

local function show_dialogue_entry(entry, index)
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
            weight = entry.weight or (story ~= nil and story.default_weight) or 400,
            lineHeight = height,
            opacity = 0.0,
        })
    end

    local voiceEntry = play_voice(entry, index)
    activeEntry = entry
    entryTimer = 0.0
    entryDuration = voiceEntry ~= nil and voiceEntry.duration ~= nil and voiceEntry.duration > 0.0
        and (voiceEntry.duration + VOICE_END_PADDING)
        or (entry.duration or (story ~= nil and story.default_duration) or DEFAULT_ENTRY_DURATION)
end

local function unregister_interaction()
    if obj ~= nil and obj.UUID ~= nil and _G.InspectableItems ~= nil then
        _G.InspectableItems[obj.UUID] = nil
    end
end

local function restore_player_after_cutscene()
    if inputLocked and Game ~= nil and Game.SetInputPossessed ~= nil then
        Game.SetInputPossessed(true)
    end
    if Game ~= nil and Game.SetMouseCaptureWhileInputBlocked ~= nil then
        Game.SetMouseCaptureWhileInputBlocked(false)
    end
    inputLocked = false

    if autoInputLocked then
        local player = get_player_actor()
        if player ~= nil and player.SetCharacterAutoInput ~= nil then
            player:SetCharacterAutoInput(savedAutoInputWASD ~= false, savedAutoInputMouseLook ~= false)
        end
    end
    autoInputLocked = false
    savedAutoInputWASD = nil
    savedAutoInputMouseLook = nil

    if movementLocked and lockedMovement ~= nil then
        if savedMaxWalkSpeed ~= nil then lockedMovement:SetMaxWalkSpeed(savedMaxWalkSpeed) end
        if savedSprintSpeedMultiplier ~= nil then lockedMovement:SetSprintSpeedMultiplier(savedSprintSpeedMultiplier) end
        if savedWallRunMaxSpeed ~= nil then lockedMovement:SetWallRunMaxSpeed(savedWallRunMaxSpeed) end
    end
    movementLocked = false
    lockedMovement = nil
    savedMaxWalkSpeed = nil
    savedSprintSpeedMultiplier = nil
    savedWallRunMaxSpeed = nil
end

local function lock_player_for_cutscene()
    if not inputLocked and Game ~= nil and Game.SetInputPossessed ~= nil then
        -- 일반 Lua 입력 스냅샷을 비워 이동/사격/상호작용 입력을 막고, 스킵만 raw input으로 받습니다.
        if Game.SetMouseCaptureWhileInputBlocked ~= nil then
            Game.SetMouseCaptureWhileInputBlocked(true)
        end
        Game.SetInputPossessed(false)
        inputLocked = true
    end

    local player = get_player_actor()
    if player == nil then
        return
    end

    if not autoInputLocked
        and player.GetCharacterAutoInputWASD ~= nil
        and player.GetCharacterAutoInputMouseLook ~= nil
        and player.SetCharacterAutoInput ~= nil then
        -- 컷씬 중 비활성 입력이 카메라 회전값으로 누적되지 않도록 자동 입력을 잠급니다.
        savedAutoInputWASD = player:GetCharacterAutoInputWASD()
        savedAutoInputMouseLook = player:GetCharacterAutoInputMouseLook()
        player:SetCharacterAutoInput(false, false)
        autoInputLocked = true
    end

    if movementLocked or player.GetCharacterMovement == nil then
        return
    end

    lockedMovement = player:GetCharacterMovement()
    if lockedMovement == nil then
        return
    end

    -- 입력 차단 직전 관성이나 보류된 이동값이 남아도 플레이어가 컷씬 중 움직이지 않도록 속도 계수를 잠급니다.
    savedMaxWalkSpeed = lockedMovement:GetMaxWalkSpeed()
    savedSprintSpeedMultiplier = lockedMovement:GetSprintSpeedMultiplier()
    savedWallRunMaxSpeed = lockedMovement:GetWallRunMaxSpeed()
    lockedMovement:SetMaxWalkSpeed(0.0)
    lockedMovement:SetSprintSpeedMultiplier(0.0)
    lockedMovement:SetWallRunMaxSpeed(0.0)
    movementLocked = true
end

local function complete_cutscene_close()
    cutsceneClosing = false
    closeTimer = 0.0
    restore_player_after_cutscene()
end

local function show_skip_ui()
    if WeaponHud ~= nil and WeaponHud.ShowSkipPrompt ~= nil then
        WeaponHud.ShowSkipPrompt({
            keyboardText = "Ctrl",
            gamepadText = "B",
            label = "SKIP",
        })
    end
    if WeaponHud ~= nil and WeaponHud.SetSkipProgress ~= nil then
        WeaponHud.SetSkipProgress(0.0)
    end
end

local function hide_skip_ui()
    if WeaponHud ~= nil and WeaponHud.HideSkipPrompt ~= nil then
        WeaponHud.HideSkipPrompt()
    elseif WeaponHud ~= nil and WeaponHud.SetSkipProgress ~= nil then
        WeaponHud.SetSkipProgress(0.0)
    end
end

local function begin_cutscene_close()
    stop_current_voice()
    activeEntry = nil
    cutsceneActive = false
    cutsceneClosing = true
    closeTimer = LETTERBOX_CLOSE_DELAY
    skipHoldTime = 0.0
    hide_skip_ui()

    if WeaponHud ~= nil and WeaponHud.HideDialogue ~= nil then
        WeaponHud.HideDialogue()
    end
    if WeaponHud ~= nil and WeaponHud.HideLetterbox ~= nil then
        WeaponHud.HideLetterbox()
    end
end

local function show_next_entry()
    stop_current_voice()
    currentIndex = currentIndex + 1

    if story == nil or story.entries == nil or currentIndex > #story.entries then
        begin_cutscene_close()
        return
    end

    show_dialogue_entry(story.entries[currentIndex], currentIndex)
end

start_cutscene = function()
    if cutsceneStarted then
        return
    end

    cutsceneStarted = true
    cutsceneActive = true
    cutsceneClosing = false
    closeTimer = 0.0
    currentIndex = 0
    skipHoldTime = 0.0

    unregister_interaction()
    lock_player_for_cutscene()

    if WeaponHud ~= nil and WeaponHud.ShowLetterbox ~= nil then
        WeaponHud.ShowLetterbox()
    end
    show_skip_ui()

    show_next_entry()
end

local function register_interaction()
    if obj == nil or obj.UUID == nil then
        return
    end

    _G.InspectableItems = _G.InspectableItems or {}
    _G.InspectableItems[obj.UUID] = {
        actor = obj,
        interact_text = INTERACT_TEXT,
        on_interact = function()
            start_cutscene()
            return true
        end,
    }
end

local function update_skip(dt)
    if is_skip_held() then
        skipHoldTime = skipHoldTime + dt
        if WeaponHud ~= nil and WeaponHud.SetSkipProgress ~= nil then
            WeaponHud.SetSkipProgress(skipHoldTime / SKIP_HOLD_DURATION)
        end
        if skipHoldTime >= SKIP_HOLD_DURATION then
            begin_cutscene_close()
            return true
        end
    else
        skipHoldTime = 0.0
        if WeaponHud ~= nil and WeaponHud.SetSkipProgress ~= nil then
            WeaponHud.SetSkipProgress(0.0)
        end
    end

    return false
end

function BeginPlay()
    story = nil
    voiceEntries = {}
    voiceEntriesById = {}
    loadedVoiceKeys = {}
    activeEntry = nil
    activeVoiceKey = nil
    currentIndex = 0
    entryTimer = 0.0
    entryDuration = 0.0
    skipHoldTime = 0.0
    cutsceneStarted = false
    cutsceneActive = false
    cutsceneClosing = false
    closeTimer = 0.0
    inputLocked = false
    movementLocked = false
    autoInputLocked = false
    lockedMovement = nil
    savedMaxWalkSpeed = nil
    savedSprintSpeedMultiplier = nil
    savedWallRunMaxSpeed = nil
    savedAutoInputWASD = nil
    savedAutoInputMouseLook = nil

    load_dialogue_assets()
    register_interaction()
end

function EndPlay()
    unregister_interaction()
    stop_current_voice()

    if WeaponHud ~= nil and WeaponHud.HideDialogue ~= nil then
        WeaponHud.HideDialogue()
    end
    if WeaponHud ~= nil and WeaponHud.HideLetterbox ~= nil then
        WeaponHud.HideLetterbox({ instant = true })
    end
    hide_skip_ui()

    restore_player_after_cutscene()
    activeEntry = nil
    cutsceneActive = false
    cutsceneClosing = false
end

function Tick(dt)
    dt = dt or 0.0

    if cutsceneClosing then
        closeTimer = closeTimer - dt
        if closeTimer <= 0.0 then
            complete_cutscene_close()
        end
        return
    end

    if not cutsceneActive then
        return
    end

    if update_skip(dt) then
        return
    end

    if activeEntry == nil then
        return
    end

    entryTimer = entryTimer + dt

    local fadeIn = activeEntry.fade_in or (story ~= nil and story.default_fade_in) or DEFAULT_FADE_IN
    local alpha = 1.0
    if entryTimer < fadeIn then
        alpha = entryTimer / fadeIn
    elseif entryTimer > entryDuration - FADE_OUT_DURATION then
        alpha = (entryDuration - entryTimer) / FADE_OUT_DURATION
    end
    alpha = clamp(alpha, 0.0, 1.0)

    if WeaponHud ~= nil and WeaponHud.SetDialogueOpacity ~= nil then
        WeaponHud.SetDialogueOpacity(alpha)
    end

    if entryTimer >= entryDuration then
        show_next_entry()
    end
end
