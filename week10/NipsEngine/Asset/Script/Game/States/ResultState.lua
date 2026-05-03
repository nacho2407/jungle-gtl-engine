local ResultState = {}
ResultState.__index = ResultState

function ResultState.new()
    return setmetatable({
        uiHandle = nil
    }, ResultState)
end

function ResultState:Enter(context, snapshot)
    context.managers.Sound:StopBGM(0.5)
    context.managers.UI:Show("Result")
    context.managers.UI:SetResult(snapshot)
    Engine.API.Input.SetInputModeUIOnly()

    self.uiHandle = context.eventBus:Subscribe("UI.Action", self, function(event)
        if event.name == "RestartGame" then
            context.managers.Game:Restart()
        elseif event.name == "BackToTitle" then
            context.stateMachine:Change("Title")
        end
    end)
end

function ResultState:Exit(context)
    context.eventBus:Unsubscribe(self.uiHandle)
    self.uiHandle = nil
end

return ResultState
