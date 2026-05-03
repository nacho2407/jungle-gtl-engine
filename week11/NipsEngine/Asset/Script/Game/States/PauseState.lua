local PauseState = {}
PauseState.__index = PauseState

function PauseState.new()
    return setmetatable({
        uiHandle = nil
    }, PauseState)
end

function PauseState:Enter(context)
    context.managers.UI:Show("Pause")
    Engine.API.Input.SetInputModeUIOnly()

    self.uiHandle = context.eventBus:Subscribe("UI.Action", self, function(event)
        if event.name == "ResumeGame" then
            context.stateMachine:Pop()
        elseif event.name == "RestartGame" then
            context.managers.Game:Restart()
        elseif event.name == "BackToTitle" then
            context.managers.Game:CancelRun("BackToTitle")
            context.stateMachine:Change("Title")
        end
    end)
end

function PauseState:Exit(context)
    context.eventBus:Unsubscribe(self.uiHandle)
    self.uiHandle = nil
end

return PauseState
