local ScoreResult = {}

local WIDGET_PATH = "Content/UI/InGame/ScoreResult.rml"
local Z_ORDER = 240
local DEFAULT_NEXT_SCENE = "Title.Scene"

local widget = nil
local visible = false
local nextScene = DEFAULT_NEXT_SCENE

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

local function close_and_continue()
    visible = false
    if widget ~= nil and widget:IsInViewport() then
        widget:RemoveFromParent()
    end

    if Engine ~= nil and Engine.TransitionToScene ~= nil then
        Engine.TransitionToScene(nextScene)
    end
end

local function bind()
    if widget == nil then return end

    widget:bind_click("score-result-continue-button", function()
        close_and_continue()
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

local function fill(snapshot, savePath)
    snapshot = snapshot or {}
    set_text("score-result-score", string.format("%06d", snapshot.score or 0))
    set_text("score-result-time", format_time(snapshot.playTimeSeconds))
    set_text("score-result-kills", snapshot.enemyKills or 0)
    set_text("score-result-retry", snapshot.retryCount or 0)
    set_text("score-result-deaths", snapshot.deathCount or 0)
    set_text("score-result-accuracy", string.format("%d%%", math.floor((snapshot.accuracy or 0.0) * 100.0 + 0.5)))
    set_text("score-result-damage", math.floor(snapshot.damageDealt or 0))
    set_text("score-result-save-path", savePath or "Saves/scores.json")
end

function ScoreResult.Show(endingId, sceneAfterClose)
    if visible then return end
    ensure_widget()
    if widget == nil or ScoreManager == nil then
        return
    end

    nextScene = sceneAfterClose or DEFAULT_NEXT_SCENE
    local snapshot = ScoreManager.GetSnapshot ~= nil and ScoreManager.GetSnapshot() or {}
    local savePath = ScoreManager.GetSaveFilePath ~= nil and ScoreManager.GetSaveFilePath() or "Saves/scores.json"

    fill(snapshot, savePath)

    if ScoreManager.FinishRun ~= nil then
        ScoreManager.FinishRun(endingId or "Ending")
    elseif ScoreManager.SaveFinalScore ~= nil then
        ScoreManager.SaveFinalScore(endingId or "Ending")
    end

    visible = true
    if not widget:IsInViewport() then
        widget:AddToViewportZ(Z_ORDER)
    end
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
end

return ScoreResult
