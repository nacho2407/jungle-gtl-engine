local TitleState = {}
TitleState.__index = TitleState

function TitleState.new()
    return setmetatable({
        uiHandle = nil
    }, TitleState)
end

function TitleState:Enter(context)
    context.managers.UI:Show("Title")
    Engine.API.Input.SetInputModeUIOnly()

    Engine.API.Debug.Log("Title State")

    context.managers.Sound:PlayBGM("Asset/Audio/BGM/TitleScreen.wav")

    self.uiHandle = context.eventBus:Subscribe("UI.Action", self, function(event)
        if event.name == "StartGame" then
            context.managers.Sound:PlaySFX(context.managers.Sound.SFX.Button)
            context.stateMachine:Change("Playing")
        elseif event.name == "QuitGame" then
            Engine.API.Application.QuitGame()
        end
    end)
end

function TitleState:Exit(context)
    context.eventBus:Unsubscribe(self.uiHandle)
    self.uiHandle = nil
end

return TitleState
