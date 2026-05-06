local ActorSequenceController = require("Game.Core.ActorSequenceController")
local PresentationSequence = require("Game.Core.PresentationSequence")

local TitleState = {}
TitleState.__index = TitleState

local INITIAL_FADE_HOLD = 0.5
local INITIAL_FADE_IN_DURATION = 0.5
local WHITE_FLASH_HALF_DURATION = 0.25
local UI_DELAY_AFTER_LOGO = 1
local PLAYER_SEQUENCE_TIMEOUT = 8.0

local function GetPlayerController()
    return Engine.API.GetPlayerController()
end

local function StartCameraFade(fromAlpha, toAlpha, duration, r, g, b)
    local pc = GetPlayerController()
    if pc ~= nil then
        pc:StartCameraFade(fromAlpha, toAlpha, duration, r or 0.0, g or 0.0, b or 0.0)
    end
end

local function PlayPlayerSequence()
    local component = ActorSequenceController.Play(Engine.API.GetPossessedActor())
    if component ~= nil then
        return component
    end

    return ActorSequenceController.Play(GetPlayerController())
end

local function PlayTitleCutSequences()
    local taggedActors = Engine.API.World.FindActorsByTag("TitleCut")
    local components = {}

    if taggedActors ~= nil and #taggedActors > 0 then
        for _, actor in ipairs(taggedActors) do
            local component = ActorSequenceController.PlayCutTrigger(actor)
            if component ~= nil then
                table.insert(components, component)
            end
        end
        return components
    end

    return ActorSequenceController.PlayCutTriggersByType("AMainSceneDestructibleActor")
end

function TitleState.new()
    return setmetatable({
        uiHandle = nil,
        presentation = PresentationSequence.new(),
        playerSequence = nil,
        cutSequences = {}
    }, TitleState)
end

function TitleState:Enter(context)
    StartCameraFade(1.0, 1.0, 0.0, 0.0, 0.0, 0.0)

    local ui = context.managers.UI
    local sound = context.managers.Sound

    ui:Show("Title")
    ui:PrepareTitlePresentation()
    Engine.API.Input.SetInputModeUIOnly()
    Engine.API.Debug.Log("Title State")
    sound:PlayBGM(sound.BGM.Main)

    self.presentation:Clear()
    self.presentation
        :Delay(INITIAL_FADE_HOLD)
        :Call(function()
            StartCameraFade(1.0, 0.0, INITIAL_FADE_IN_DURATION, 0.0, 0.0, 0.0)
        end)
        :Delay(INITIAL_FADE_IN_DURATION)
        :Call(function()
            self.playerSequence = PlayPlayerSequence()
        end)
        :WaitUntil(function()
            return not ActorSequenceController.IsPlaying(self.playerSequence)
        end, PLAYER_SEQUENCE_TIMEOUT)
        :Call(function()
            StartCameraFade(0.0, 1.0, WHITE_FLASH_HALF_DURATION, 1.0, 1.0, 1.0)
        end)
        :Delay(WHITE_FLASH_HALF_DURATION)
        :Call(function()
            self.cutSequences = PlayTitleCutSequences()
            ui:StartTitleLogoAnimation()
            sound:PlayMainLogo()
            StartCameraFade(1.0, 0.0, WHITE_FLASH_HALF_DURATION, 1.0, 1.0, 1.0)
        end)
        :Delay(UI_DELAY_AFTER_LOGO)
        :Call(function()
            ui:StartTitleControlsFadeIn(1.0)
            self:SubscribeActions(context)
        end)
        :Play()
end

function TitleState:SubscribeActions(context)
    if self.uiHandle ~= nil then
        return
    end

    self.uiHandle = context.eventBus:Subscribe("UI.Action", self, function(event)
        if event.name == "StartGame" then
            if context.root:OpenGameScene() then
                return
            end
            context.stateMachine:Change("Loading")
        elseif event.name == "ShowIntro" then
            if context.root:OpenIntroScene() then
                return
            end
            context.stateMachine:Change("Intro", { returnTo = "Title" })
        elseif event.name == "QuitGame" then
            Engine.API.Application.QuitGame()
        elseif event.name == "ShowScoreBoard" then
            context.managers.UI:SetTitlePanel("ScoreBoard")
        elseif event.name == "ShowSettings" then
            context.managers.UI:SetTitlePanel("Settings")
        elseif event.name == "ShowCredits" then
            context.managers.UI:SetTitlePanel("Credits")
        elseif event.name == "BackToTitleMenu" then
            context.managers.UI:SetTitlePanel("Menu")
        elseif event.name == "BGMVolumeChanged" then
            context.managers.UI:ApplyVolumeFromSlider("BGM", "Title.Settings.BGMSlider")
        elseif event.name == "SFXVolumeChanged" then
            context.managers.UI:ApplyVolumeFromSlider("SFX", "Title.Settings.SFXSlider")
        elseif event.name == "BGMDown" then
            context.managers.Sound:AdjustBGMVolume(-0.1)
            context.managers.UI:RefreshSettings()
        elseif event.name == "BGMUp" then
            context.managers.Sound:AdjustBGMVolume(0.1)
            context.managers.UI:RefreshSettings()
        elseif event.name == "SFXDown" then
            context.managers.Sound:AdjustSFXVolume(-0.1)
            context.managers.UI:RefreshSettings()
        elseif event.name == "SFXUp" then
            context.managers.Sound:AdjustSFXVolume(0.1)
            context.managers.UI:RefreshSettings()
        end
    end)
end

function TitleState:Tick(context, dt)
    self.presentation:Tick(Engine.API.World.GetUnscaledDeltaTime() or dt)
end

function TitleState:Exit(context)
    self.presentation:Stop()
    self.playerSequence = nil
    self.cutSequences = {}

    if self.uiHandle then
        context.eventBus:Unsubscribe(self.uiHandle)
    end
    self.uiHandle = nil
end

return TitleState
