local PauseState = {}
PauseState.__index = PauseState

function PauseState.new()
    return setmetatable({
        uiHandle = nil
    }, PauseState)
end

function PauseState:Enter(context)
    context.managers.UI:ShowOverlay("Pause")
    context.managers.UI:RefreshSettings()
    Engine.API.Input.SetInputModeUIOnly()
    Engine.API.Time.SetTimeScale(0)

    self.uiHandle = context.eventBus:Subscribe("UI.Action", self, function(event)
        if event.name == "ResumeGame" then
            context.stateMachine:Pop()
        elseif event.name == "RestartGame" then
            context.managers.Game:Restart()
        elseif event.name == "BackToTitle" then
            context.managers.Game:CancelRun("BackToTitle")
            if context.root:OpenMainScene() then
                return
            end
            context.stateMachine:Change("Title")
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

function PauseState:Exit(context)
    context.eventBus:Unsubscribe(self.uiHandle)
    self.uiHandle = nil
    context.managers.UI:HideOverlay("Pause")
end

return PauseState
