local GameOver = {}

local WIDGET_PATH = "Content/UI/InGame/GameOver.rml"
local GAME_OVER_Z_ORDER = 230
local TITLE_SCENE = "Title.Scene"
local GAME_OVER_AUDIO_KEY = "GameOverBGM"
local GAME_OVER_AUDIO_PATH = "GameOver.wav"
local LEVEL1_SCENE_NAME = "FL_Level1"

local widget = nil
local visible = false
local audioLoaded = false

local function set_game_paused(paused)
    if Engine == nil then return end

    if paused then
        if Engine.PauseGame ~= nil then
            Engine.PauseGame()
        end
    else
        if Engine.ResumeGame ~= nil then
            Engine.ResumeGame()
        end
    end
end

local function stop_game_over_audio()
    if AudioManager ~= nil and AudioManager.Stop ~= nil then
        AudioManager.Stop(GAME_OVER_AUDIO_KEY)
    end
    if AudioManager ~= nil and AudioManager.StopBGM ~= nil then
        AudioManager.StopBGM()
    end
end

local function play_game_over_audio()
    if AudioManager == nil then return end

    if not audioLoaded and AudioManager.Load ~= nil then
        audioLoaded = AudioManager.Load(GAME_OVER_AUDIO_KEY, GAME_OVER_AUDIO_PATH, false) == true
    end

    if AudioManager.StopBGM ~= nil then
        AudioManager.StopBGM()
    end

    if audioLoaded and AudioManager.Play ~= nil then
        AudioManager.Play(GAME_OVER_AUDIO_KEY, 0.78)
    end
end

local function transition_to(scene)
    visible = false
    stop_game_over_audio()
    set_game_paused(false)

    if widget ~= nil and widget:IsInViewport() then
        widget:RemoveFromParent()
    end

    if Engine ~= nil and Engine.TransitionToScene ~= nil then
        Engine.TransitionToScene(scene)
    end
end

local function normalize_scene_name(scene)
    if scene == nil then return "" end

    local name = tostring(scene)
    name = string.gsub(name, "\\", "/")
    name = string.match(name, "([^/]+)$") or name
    name = string.gsub(name, "%.Scene$", "")
    return string.lower(name)
end

local function mark_level1_intro_skip_for_retry()
    if Game == nil or Game.GetCurrentSceneName == nil then
        return
    end

    -- GameOver Retry는 "현재 레벨 재시작"이므로, Level 1에서는 이미 본 착륙 인트로를 1회 스킵합니다.
    local currentScene = normalize_scene_name(Game.GetCurrentSceneName())
    if currentScene ~= normalize_scene_name(LEVEL1_SCENE_NAME) then
        return
    end

    Game.SkipLevel1IntroOnNextStart = true
    Game.SkipLevel1IntroSceneName = LEVEL1_SCENE_NAME
end

local function retry()
    if ScoreManager ~= nil and ScoreManager.AddRetry ~= nil then
        ScoreManager.AddRetry(1)
    end

    visible = false
    stop_game_over_audio()
    set_game_paused(false)

    if widget ~= nil and widget:IsInViewport() then
        widget:RemoveFromParent()
    end

    if Game ~= nil and Game.RestartLevel ~= nil then
        mark_level1_intro_skip_for_retry()
        Game.RestartLevel()
    elseif Engine ~= nil and Engine.TransitionToScene ~= nil then
        Engine.TransitionToScene("Default.Scene")
    end
end

local function bind_game_over()
    if widget == nil then return end

    widget:bind_click("game-over-retry-button", function()
        retry()
    end)

    widget:bind_click("game-over-title-button", function()
        transition_to(TITLE_SCENE)
    end)
end

function GameOver.Initialize()
    if widget == nil then
        widget = UI.CreateWidget(WIDGET_PATH)
        if widget ~= nil then
            widget:SetWantsMouse(true)
            bind_game_over()
        end
    end
end

function GameOver.Show()
    GameOver.Initialize()
    if visible then return end

    visible = true
    set_game_paused(true)
    play_game_over_audio()

    if widget ~= nil and not widget:IsInViewport() then
        widget:AddToViewportZ(GAME_OVER_Z_ORDER)
    end
end

function GameOver.IsOpen()
    return visible
end

function GameOver.Shutdown()
    visible = false
    stop_game_over_audio()
    if widget ~= nil and widget:IsInViewport() then
        widget:RemoveFromParent()
    end
    widget = nil
    audioLoaded = false
end

return GameOver
