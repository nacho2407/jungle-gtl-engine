local TitleState = {}
TitleState.__index = TitleState

function TitleState.new()
    return setmetatable({
        uiHandle = nil
    }, TitleState)
end

function TitleState:Enter(context)
    context.managers.UI:Show("Title")
    context.managers.UI:SetTitlePanel("Menu")
    Engine.API.Input.SetInputModeUIOnly()

    Engine.API.Debug.Log("Title State")

    context.managers.Sound:PlayBGM(context.managers.Sound.BGM.Title)

    self.uiHandle = context.eventBus:Subscribe("UI.Action", self, function(event)
        if event.name == "StartGame" then
            context.managers.Sound:PlaySFX(context.managers.Sound.SFX.Button)
            if context.root:OpenGameScene() then
                return
            end
            context.stateMachine:Change("Loading")
        elseif event.name == "ShowIntro" then
            context.managers.Sound:PlaySFX(context.managers.Sound.SFX.Button)
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

function TitleState:Exit(context)
    context.eventBus:Unsubscribe(self.uiHandle)
    self.uiHandle = nil
end

return TitleState
