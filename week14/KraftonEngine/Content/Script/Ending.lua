local WeaponHud = require("HUD/WeaponHud")
local ScoreResult = require("Game.ScoreResult")

local STORY_MODULE = "Dialogue/DrakePostCombat.dialogue"
local VOICE_MODULE = "Dialogue/Generated/DrakePostCombat.voices"
local CUTSCENE_WIDGET_PATH = "Content/UI/Cutscene/Cutscene.rml"
local PLAYER_PAWN_NAME = "kain-temp"
local LAST_DIALOGUE_ID = "DrakePostCombat_System_LancerOffline"

local CUTSCENE_Z_ORDER = 220
local VOICE_END_PADDING = 0.25
local FADE_OUT_DURATION = 5.0
local DIALOGUE_BOX_WIDTH = 980.0
local DIALOGUE_BOX_HEIGHT = 48.0
local DIALOGUE_LINE_HEIGHT = 48.0
local DIALOGUE_TEXT_LEFT = 16.0
local DIALOGUE_DEFAULT_FONT_SIZE = 22.0
local CREDIT_DISPLAY_DURATION = 4.0
local CREDIT_FADE_DURATION = 0.75
local SCORE_RESULT_RETRY_INTERVAL = 0.25

local story = nil
local cutsceneWidget = nil
local currentIndex = 0
local activeEntry = nil
local entryTime = 0.0
local entryDuration = 0.0
local fadeInDuration = 0.3
local voiceEntries = nil
local voiceEntriesById = nil
local loadedVoiceKeys = {}
local currentVoiceKey = nil
local currentVoiceDuration = 0.0
local sceneTime = 0.0
local cutsceneStarted = false
local cutsceneFinished = false
local fadeOutActive = false
local fadeOutTimer = 0.0
local scoreShown = false
local scoreResultRequested = false
local scoreResultRetryTimer = 0.0

local lockedMovement = nil
local savedMaxWalkSpeed = nil
local savedSprintSpeedMultiplier = nil
local savedWallRunMaxSpeed = nil
local bMovementLocked = false
local bInputLocked = false

local ENDING_CREDITS = {
    { role = "DEVELOPMENT", lines = { "NEEDS MORE SLEEP" } },
    { role = "PRODUCER", lines = { "JEON HYEONGIL" } },
    { role = "PRODUCER", lines = { "JANG MINJOON" } },
    { role = "PRODUCER", lines = { "KIM HYOBEOM" } },
    { role = "SPECIAL THANKS", lines = { "KRAFTON JUNGLE", "GAME TECH LAB" } },
}

local function has_vantus_master_key()
    return _G.PlayerHasVantusMasterKey == true
        or (_G.PickedUpItems ~= nil and _G.PickedUpItems.vantus_master_key == true)
end

local function px(value)
    return string.format("%.2fpx", value)
end

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function smoothstep(alpha)
    local t = clamp(alpha, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)
end

local function get_viewport_size()
    if Engine ~= nil and Engine.GetViewportSize ~= nil then
        local size = Engine.GetViewportSize()
        return size.Width or 1280.0, size.Height or 720.0
    end
    return 1280.0, 720.0
end

local function is_valid_actor(actor)
    if actor == nil then return false end
    if type(actor.IsValid) == "function" then
        return actor:IsValid()
    end
    return true
end

local function is_player_actor(actor)
    if not is_valid_actor(actor) then return false end

    if type(actor.HasTag) == "function" then
        local ok, result = pcall(function()
            return actor:HasTag("player")
        end)
        if ok and result == true then
            return true
        end
    end

    if type(actor.IsPossessed) == "function" then
        local ok, result = pcall(function()
            return actor:IsPossessed()
        end)
        if ok and result == true then
            return true
        end
    end

    return type(actor.GetCharacterMovement) == "function" and actor:GetCharacterMovement() ~= nil
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

local function lock_player_for_ending()
    if Game ~= nil and Game.SetInputPossessed ~= nil then
        Game.SetInputPossessed(false)
        bInputLocked = true
    end

    local player = get_player_actor()
    if player == nil or player.GetCharacterMovement == nil or bMovementLocked then
        return
    end

    lockedMovement = player:GetCharacterMovement()
    if lockedMovement == nil then
        return
    end

    savedMaxWalkSpeed = lockedMovement:GetMaxWalkSpeed()
    savedSprintSpeedMultiplier = lockedMovement:GetSprintSpeedMultiplier()
    savedWallRunMaxSpeed = lockedMovement:GetWallRunMaxSpeed()
    lockedMovement:SetMaxWalkSpeed(0.0)
    lockedMovement:SetSprintSpeedMultiplier(0.0)
    lockedMovement:SetWallRunMaxSpeed(0.0)
    bMovementLocked = true
end

local function restore_player_after_ending()
    if bInputLocked and Game ~= nil and Game.SetInputPossessed ~= nil then
        Game.SetInputPossessed(true)
    end
    bInputLocked = false

    if bMovementLocked and lockedMovement ~= nil then
        if savedMaxWalkSpeed ~= nil then lockedMovement:SetMaxWalkSpeed(savedMaxWalkSpeed) end
        if savedSprintSpeedMultiplier ~= nil then lockedMovement:SetSprintSpeedMultiplier(savedSprintSpeedMultiplier) end
        if savedWallRunMaxSpeed ~= nil then lockedMovement:SetWallRunMaxSpeed(savedWallRunMaxSpeed) end
    end

    lockedMovement = nil
    savedMaxWalkSpeed = nil
    savedSprintSpeedMultiplier = nil
    savedWallRunMaxSpeed = nil
    bMovementLocked = false
end

local function set_weapon_hud_visible(visible)
    if WeaponHud ~= nil and WeaponHud.SetVisible ~= nil then
        WeaponHud.SetVisible(visible)
    end
end

local function stop_current_voice()
    if currentVoiceKey ~= nil and AudioManager ~= nil and AudioManager.Stop ~= nil then
        AudioManager.Stop(currentVoiceKey)
    end
    currentVoiceKey = nil
end

local function remove_cutscene_widget()
    if cutsceneWidget ~= nil and cutsceneWidget:IsInViewport() then
        cutsceneWidget:RemoveFromParent()
    end
    cutsceneWidget = nil
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

local function get_voice_entry(entry, index)
    if voiceEntries == nil or entry == nil then return nil end
    if voiceEntriesById ~= nil and entry.id ~= nil and voiceEntriesById[entry.id] ~= nil then
        return voiceEntriesById[entry.id]
    end
    return voiceEntries[index]
end

local function load_voice_manifest()
    voiceEntries = nil
    voiceEntriesById = nil
    loadedVoiceKeys = {}

    local ok, voices = pcall(require, VOICE_MODULE)
    if not ok or voices == nil or voices.entries == nil then
        return
    end

    voiceEntries = {}
    voiceEntriesById = voices.by_id or {}
    for _, voiceEntry in ipairs(voices.entries) do
        if voiceEntry.index ~= nil then
            voiceEntries[voiceEntry.index] = voiceEntry
        end
        if voiceEntry.id ~= nil then
            voiceEntriesById[voiceEntry.id] = voiceEntry
        end
    end
end

local function load_story()
    story = require(STORY_MODULE)
    local filtered = {}
    if story ~= nil and story.entries ~= nil then
        for _, entry in ipairs(story.entries) do
            table.insert(filtered, entry)
            if entry.id == LAST_DIALOGUE_ID then
                break
            end
        end
        story.entries = filtered
    end
    load_voice_manifest()
end

local function play_dialogue_voice(entry, index)
    if AudioManager == nil or AudioManager.Load == nil or AudioManager.Play == nil then
        return nil
    end

    local voiceEntry = get_voice_entry(entry, index)
    if voiceEntry == nil or voiceEntry.key == nil or voiceEntry.path == nil then
        return nil
    end

    stop_current_voice()
    if loadedVoiceKeys[voiceEntry.key] ~= true then
        if not AudioManager.Load(voiceEntry.key, voiceEntry.path, false) then
            return nil
        end
        loadedVoiceKeys[voiceEntry.key] = true
    end

    AudioManager.Play(voiceEntry.key, voiceEntry.volume or 1.0)
    currentVoiceKey = voiceEntry.key
    return voiceEntry
end

local function apply_dialogue_entry(entry)
    if cutsceneWidget == nil or entry == nil then return end

    local viewportWidth, viewportHeight = get_viewport_size()
    local width = DIALOGUE_BOX_WIDTH
    local height = DIALOGUE_BOX_HEIGHT
    local fontSize = entry.size or story.default_size or DIALOGUE_DEFAULT_FONT_SIZE
    local left = (viewportWidth - width) * 0.5
    local bottom = viewportHeight * 0.135 + 18.0

    cutsceneWidget:SetText("dialogue-line", get_dialogue_text(entry))
    cutsceneWidget:SetProperty("dialogue-box", "left", px(left))
    cutsceneWidget:SetProperty("dialogue-box", "bottom", px(bottom))
    cutsceneWidget:SetProperty("dialogue-box", "width", px(width))
    cutsceneWidget:SetProperty("dialogue-box", "height", px(height))
    cutsceneWidget:SetProperty("dialogue-box", "opacity", "0.0")
    cutsceneWidget:SetProperty("dialogue-line", "left", px(DIALOGUE_TEXT_LEFT))
    cutsceneWidget:SetProperty("dialogue-line", "top", "0px")
    cutsceneWidget:SetProperty("dialogue-line", "width", px(width - DIALOGUE_TEXT_LEFT * 2.0))
    cutsceneWidget:SetProperty("dialogue-line", "height", px(height))
    cutsceneWidget:SetProperty("dialogue-line", "font-family", entry.font or story.default_font or "Pretendard")
    cutsceneWidget:SetProperty("dialogue-line", "font-size", px(fontSize))
    cutsceneWidget:SetProperty("dialogue-line", "font-weight", tostring(entry.weight or story.default_weight or 400))
    cutsceneWidget:SetProperty("dialogue-line", "line-height", px(DIALOGUE_LINE_HEIGHT))
    cutsceneWidget:SetProperty("dialogue-line", "text-align", "left")
end

local function hide_dialogue_box()
    if cutsceneWidget == nil then return end
    cutsceneWidget:SetText("dialogue-line", "")
    cutsceneWidget:SetProperty("dialogue-box", "opacity", "0.0")
end

local function update_credits()
    if cutsceneWidget == nil then return end
    local count = #ENDING_CREDITS
    if count <= 0 then
        cutsceneWidget:SetProperty("producer-credit", "opacity", "0.0")
        return
    end

    local segment = CREDIT_DISPLAY_DURATION
    local localTime = sceneTime
    local index = math.floor(localTime / segment) % count + 1
    local segmentTime = localTime - math.floor(localTime / segment) * segment
    local fade = math.min(CREDIT_FADE_DURATION, segment * 0.3)
    local alpha = 1.0

    if segmentTime < fade then
        alpha = smoothstep(segmentTime / fade)
    elseif segmentTime > segment - fade then
        alpha = smoothstep((segment - segmentTime) / fade)
    end

    local credit = ENDING_CREDITS[index]
    local lines = credit.lines or {}
    cutsceneWidget:SetText("producer-credit-role", credit.role or "")
    cutsceneWidget:SetText("producer-credit-name", lines[1] or "")
    cutsceneWidget:SetText("producer-credit-name-line2", lines[2] or "")
    cutsceneWidget:SetProperty("producer-credit", "opacity", string.format("%.2f", alpha))
end

local function show_score_result()
    if scoreShown then return end
    if not scoreResultRequested then
        scoreResultRequested = true
        stop_current_voice()
        remove_cutscene_widget()
        set_weapon_hud_visible(false)
    end

    local opened = false
    if ScoreResult ~= nil and ScoreResult.Show ~= nil then
        local ok, result = pcall(ScoreResult.Show, "Ending", "FL_Title.Scene")
        if ok then
            opened = result == true
                or (ScoreResult.IsOpen ~= nil and ScoreResult.IsOpen() == true)
        else
            print("[Ending] ScoreResult.Show failed: " .. tostring(result))
        end
    else
        print("[Ending] ScoreResult module is unavailable.")
    end

    if opened then
        scoreShown = true
        scoreResultRetryTimer = 0.0
    else
        scoreResultRetryTimer = SCORE_RESULT_RETRY_INTERVAL
    end
end

local function update_score_result(dt)
    if scoreShown and ScoreResult ~= nil and ScoreResult.Update ~= nil then
        ScoreResult.Update(dt or 0.0)
    end
end

local function begin_fade_out()
    if fadeOutActive then return end
    fadeOutActive = true
    fadeOutTimer = 0.0
    activeEntry = nil
    hide_dialogue_box()
    stop_current_voice()
    if cutsceneWidget ~= nil then
        cutsceneWidget:SetProperty("producer-credit", "opacity", "0.0")
        cutsceneWidget:SetProperty("ending-fade-overlay", "display", "block")
        cutsceneWidget:SetProperty("ending-fade-overlay", "opacity", "0.0")
    end
end

local function show_next_entry()
    stop_current_voice()
    currentIndex = currentIndex + 1
    if story == nil or story.entries == nil or currentIndex > #story.entries then
        begin_fade_out()
        return
    end

    activeEntry = story.entries[currentIndex]
    entryTime = 0.0
    entryDuration = activeEntry.duration or story.default_duration or 3.4
    fadeInDuration = activeEntry.fade_in or story.default_fade_in or 0.3
    currentVoiceDuration = 0.0
    currentVoiceKey = nil
    apply_dialogue_entry(activeEntry)

    local voiceEntry = play_dialogue_voice(activeEntry, currentIndex)
    if voiceEntry ~= nil and voiceEntry.duration ~= nil and voiceEntry.duration > 0.0 then
        currentVoiceDuration = voiceEntry.duration
        entryDuration = currentVoiceDuration + VOICE_END_PADDING
    end
end

local function start_ending()
    if cutsceneStarted then return true end
    if not has_vantus_master_key() then
        return false
    end

    local ok, err = pcall(load_story)
    if not ok then
        print("[Ending] Failed to load post-combat story: " .. tostring(err))
        return false
    end

    if UI == nil or UI.CreateWidget == nil then
        print("[Ending] UI.CreateWidget is unavailable.")
        return false
    end

    cutsceneWidget = UI.CreateWidget(CUTSCENE_WIDGET_PATH)
    if cutsceneWidget == nil then
        print("[Ending] Failed to create cutscene widget: " .. CUTSCENE_WIDGET_PATH)
        return false
    end

    cutsceneStarted = true
    cutsceneFinished = false
    fadeOutActive = false
    fadeOutTimer = 0.0
    scoreShown = false
    scoreResultRequested = false
    scoreResultRetryTimer = 0.0
    sceneTime = 0.0
    currentIndex = 0
    activeEntry = nil
    entryTime = 0.0
    entryDuration = 0.0
    currentVoiceKey = nil
    currentVoiceDuration = 0.0

    lock_player_for_ending()
    set_weapon_hud_visible(false)

    cutsceneWidget:SetWantsMouse(false)
    cutsceneWidget:AddToViewportZ(CUTSCENE_Z_ORDER)
    cutsceneWidget:SetProperty("cutscene-skip-prompt", "display", "none")
    cutsceneWidget:SetProperty("skip-ring", "display", "none")
    cutsceneWidget:SetProperty("ending-fade-overlay", "display", "none")
    cutsceneWidget:SetProperty("ending-fade-overlay", "opacity", "0.0")

    show_next_entry()
    return true
end

function BeginPlay()
    cutsceneStarted = false
    cutsceneFinished = false
    fadeOutActive = false
    fadeOutTimer = 0.0
    scoreShown = false
    scoreResultRequested = false
    scoreResultRetryTimer = 0.0
end

function EndPlay()
    stop_current_voice()
    remove_cutscene_widget()
    restore_player_after_ending()
    set_weapon_hud_visible(true)
    if ScoreResult ~= nil and ScoreResult.Shutdown ~= nil then
        ScoreResult.Shutdown()
    end
end

function OnOverlap(OtherActor)
    if cutsceneStarted then return end
    if not is_player_actor(OtherActor) then return end
    start_ending()
end

function OnHit(OtherActor, HitComponent, OtherComp, NormalImpulse, HitResult)
    OnOverlap(OtherActor)
end

local function update_ending(dt)
    dt = dt or 0.0
    if scoreShown then
        update_score_result(dt)
        return
    end
    if scoreResultRequested then
        scoreResultRetryTimer = scoreResultRetryTimer - dt
        if scoreResultRetryTimer <= 0.0 then
            show_score_result()
        end
        return
    end
    if not cutsceneStarted or cutsceneFinished then
        return
    end

    sceneTime = sceneTime + dt
    lock_player_for_ending()

    if fadeOutActive then
        fadeOutTimer = fadeOutTimer + dt
        local alpha = clamp(fadeOutTimer / FADE_OUT_DURATION, 0.0, 1.0)
        if cutsceneWidget ~= nil then
            cutsceneWidget:SetProperty("ending-fade-overlay", "opacity", string.format("%.2f", alpha))
        end
        if fadeOutTimer >= FADE_OUT_DURATION then
            cutsceneFinished = true
            show_score_result()
        end
        return
    end

    update_credits()

    if activeEntry == nil or cutsceneWidget == nil then
        return
    end

    entryTime = entryTime + dt
    local alpha = 1.0
    if fadeInDuration > 0.0 then
        alpha = clamp(entryTime / fadeInDuration, 0.0, 1.0)
    end
    cutsceneWidget:SetProperty("dialogue-box", "opacity", string.format("%.2f", alpha))

    if entryTime >= entryDuration then
        show_next_entry()
    end
end

function Tick(dt)
    update_ending(dt)
end

function StartEndingCutscene()
    start_ending()
end

return {
    Start = start_ending,
    Update = update_ending,
    Shutdown = EndPlay,
    IsRunning = function()
        local scoreOpen = ScoreResult ~= nil and ScoreResult.IsOpen ~= nil and ScoreResult.IsOpen()
        return cutsceneStarted and (not cutsceneFinished or scoreOpen or scoreResultRequested)
    end,
}
