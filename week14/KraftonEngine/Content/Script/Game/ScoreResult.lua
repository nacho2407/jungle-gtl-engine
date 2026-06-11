local ScoreResult = {}

local WIDGET_PATH = "Content/UI/InGame/ScoreResult.rml"
local Z_ORDER = 240
local DEFAULT_NEXT_SCENE = "FL_Title.Scene"
local DEFAULT_PLAYER_ID = "PLAYER"
local MAX_PLAYER_ID_LENGTH = 16
local CARET_BLINK_INTERVAL = 0.45

local VK_BACKSPACE = 8
local VK_ENTER = 13
local VK_SHIFT = 16
local VK_LSHIFT = 160
local VK_RSHIFT = 161
local VK_OEM_MINUS = 189

local widget = nil
local visible = false
local nextScene = DEFAULT_NEXT_SCENE
local endingId = "Ending"
local currentSnapshot = {}
local savePath = "Saves/scores.json"
local playerId = ""
local hasSavedScore = false
local saveSucceeded = false
local caretTimer = 0.0
local caretVisible = true

local function format_time(seconds)
    seconds = math.max(0, math.floor(seconds or 0))
    local minutes = math.floor(seconds / 60)
    local remain = seconds % 60
    return string.format("%02d:%02d", minutes, remain)
end

local function set_text(id, value)
    if widget ~= nil then
        widget:SetText(id, tostring(value or ""))
    end
end

local function get_raw_key_down(key)
    return Input ~= nil and Input.GetRawKeyDown ~= nil and Input.GetRawKeyDown(key)
end

local function get_raw_key(key)
    return Input ~= nil and Input.GetRawKey ~= nil and Input.GetRawKey(key)
end

local function is_shift_down()
    return get_raw_key(VK_SHIFT) or get_raw_key(VK_LSHIFT) or get_raw_key(VK_RSHIFT)
end

local function sanitize_player_id(id)
    id = tostring(id or "")
    id = string.gsub(id, "[^%w_%-]", "")
    if string.len(id) > MAX_PLAYER_ID_LENGTH then
        id = string.sub(id, 1, MAX_PLAYER_ID_LENGTH)
    end
    if id == "" then
        return DEFAULT_PLAYER_ID
    end
    return id
end

local function display_player_id()
    local text = playerId
    if text == "" then
        text = "TYPE ID"
    end
    if visible and not hasSavedScore and caretVisible then
        text = text .. "_"
    end
    set_text("score-result-player-id", text)
end

local function set_status(text)
    set_text("score-result-save-status", text or "")
end

local function fill(snapshot, inSavePath)
    snapshot = snapshot or {}
    set_text("score-result-score", string.format("%06d", snapshot.score or 0))
    set_text("score-result-time", format_time(snapshot.playTimeSeconds))
    set_text("score-result-kills", snapshot.enemyKills or 0)
    set_text("score-result-boss-kills", snapshot.bossKills or 0)
    set_text("score-result-retry", snapshot.retryCount or 0)
    set_text("score-result-deaths", snapshot.deathCount or 0)
    set_text("score-result-save-path", inSavePath or "Saves/scores.json")
    display_player_id()
end

local function normalize_scene_name(scene)
    scene = scene or DEFAULT_NEXT_SCENE
    if scene == "Title.Scene" then
        return DEFAULT_NEXT_SCENE
    end
    return scene
end

local function refresh_current_snapshot()
    if ScoreManager == nil or ScoreManager.GetSnapshot == nil then
        currentSnapshot = currentSnapshot or {}
        return currentSnapshot
    end

    currentSnapshot = ScoreManager.GetSnapshot() or {}
    return currentSnapshot
end

local function save_score()
    if hasSavedScore then
        set_status(saveSucceeded and "SAVED" or "SAVE FAILED")
        return saveSucceeded
    end

    refresh_current_snapshot()
    fill(currentSnapshot, savePath)
    playerId = sanitize_player_id(playerId)

    local ok = false
    if ScoreManager ~= nil then
        if ScoreManager.FinishRunWithPlayerId ~= nil then
            ok = ScoreManager.FinishRunWithPlayerId(playerId, endingId) == true
        elseif ScoreManager.FinishRun ~= nil then
            ok = ScoreManager.FinishRun(endingId, playerId) == true
        elseif ScoreManager.SaveFinalScoreWithPlayerId ~= nil then
            ok = ScoreManager.SaveFinalScoreWithPlayerId(playerId, endingId) == true
        elseif ScoreManager.SaveFinalScore ~= nil then
            ok = ScoreManager.SaveFinalScore(endingId, playerId) == true
        end
    end

    hasSavedScore = ok
    saveSucceeded = ok
    caretVisible = not ok
    display_player_id()
    set_status(ok and "SAVED" or "SAVE FAILED")
    return ok
end

local function request_scene_transition(scene)
    scene = normalize_scene_name(scene)

    if Engine ~= nil and Engine.TransitionToScene ~= nil then
        Engine.TransitionToScene(scene)
        return true
    end

    if Game ~= nil and Game.TransitionToScene ~= nil then
        Game.TransitionToScene(scene)
        return true
    end

    print("[ScoreResult] TransitionToScene is not available: " .. tostring(scene))
    return false
end

local function close_and_return_title()
    if not hasSavedScore then
        local ok, err = pcall(function()
            save_score()
        end)

        if not ok then
            print("[ScoreResult] save_score failed before title transition: " .. tostring(err))
        end
    end

    visible = false

    if widget ~= nil and widget:IsInViewport() then
        widget:RemoveFromParent()
    end

    request_scene_transition(nextScene or DEFAULT_NEXT_SCENE)
end

local function bind()
    if widget == nil then return end

    widget:bind_click("score-result-save-button", function()
        save_score()
    end)

    widget:bind_click("score-result-title-button", function()
        close_and_return_title()
    end)

    widget:bind_click("score-result-player-id-box", function()
        caretTimer = 0.0
        caretVisible = true
        display_player_id()
    end)
end

local function ensure_widget()
    if widget == nil then
        widget = UI.CreateWidget(WIDGET_PATH)
        if widget ~= nil then
            widget:SetWantsMouse(true)
            bind()
        end
    end
end

local function append_char(ch)
    if hasSavedScore then
        return false
    end
    if string.len(playerId) >= MAX_PLAYER_ID_LENGTH then
        return false
    end
    playerId = playerId .. ch
    return true
end

local function update_text_input(dt)
    if hasSavedScore then
        return
    end

    caretTimer = caretTimer + (dt or 0.0)
    if caretTimer >= CARET_BLINK_INTERVAL then
        caretTimer = 0.0
        caretVisible = not caretVisible
    end

    local changed = false
    if get_raw_key_down(VK_BACKSPACE) then
        if string.len(playerId) > 0 then
            playerId = string.sub(playerId, 1, string.len(playerId) - 1)
            changed = true
        end
    end

    if get_raw_key_down(VK_ENTER) then
        save_score()
        return
    end

    if get_raw_key_down(VK_OEM_MINUS) then
        changed = append_char(is_shift_down() and "_" or "-") or changed
    end

    for key = 65, 90 do
        if get_raw_key_down(key) then
            changed = append_char(string.char(key)) or changed
        end
    end

    for key = 48, 57 do
        if get_raw_key_down(key) then
            changed = append_char(string.char(key)) or changed
        end
    end

    for key = 96, 105 do
        if get_raw_key_down(key) then
            changed = append_char(tostring(key - 96)) or changed
        end
    end

    if changed then
        caretTimer = 0.0
        caretVisible = true
        set_status("ENTER ID AND SAVE")
    end
    display_player_id()
end

function ScoreResult.Show(inEndingId, sceneAfterClose)
    if visible then return end
    ensure_widget()
    if widget == nil or ScoreManager == nil then
        return
    end

    endingId = inEndingId or "Ending"
    nextScene = normalize_scene_name(sceneAfterClose)
    currentSnapshot = refresh_current_snapshot()
    savePath = ScoreManager.GetSaveFilePath ~= nil and ScoreManager.GetSaveFilePath() or "Saves/scores.json"
    playerId = ""
    hasSavedScore = false
    saveSucceeded = false
    caretTimer = 0.0
    caretVisible = true

    fill(currentSnapshot, savePath)
    set_status("ENTER ID AND SAVE")

    visible = true
    if not widget:IsInViewport() then
        widget:AddToViewportZ(Z_ORDER)
    end
end

function ScoreResult.Update(dt)
    if not visible then
        return
    end
    update_text_input(dt or 0.0)
end

function ScoreResult.IsOpen()
    return visible
end

function ScoreResult.Shutdown()
    visible = false
    if widget ~= nil and widget:IsInViewport() then
        widget:RemoveFromParent()
    end
    widget = nil
    playerId = ""
    hasSavedScore = false
    saveSucceeded = false
end

return ScoreResult
