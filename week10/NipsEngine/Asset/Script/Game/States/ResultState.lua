local ResultState = {}
ResultState.__index = ResultState

function ResultState.new()
    return setmetatable({
        uiHandle = nil,
        snapshot = nil
    }, ResultState)
end

function ResultState:Enter(context, snapshot)
    self.snapshot = snapshot or {}
    Engine.API.Time.SetTimeScale(0.0)
    context.managers.Sound:StopBGM(0.5)
    context.managers.UI:ShowOverlay("Result")
    context.managers.UI:SetResult(self.snapshot)
    Engine.API.Input.SetInputModeUIOnly()

    self.uiHandle = context.eventBus:Subscribe("UI.Action", self, function(event)
        if event.name == "RestartGame" then
            Engine.API.Time.SetTimeScale(1.0)
            context.managers.Game:Restart()
        elseif event.name == "BackToTitle" then
            Engine.API.Time.SetTimeScale(1.0)
            if context.root:OpenMainScene() then
                return
            end
            context.stateMachine:Change("Title")
        elseif event.name == "SubmitScore" then
            context.managers.UI:SubmitResultScore(self.snapshot)
        elseif event.name == "RenewResultName" then
            context.managers.UI:RenewResultName()
        end
    end)
end

function ResultState:Exit(context)
    context.eventBus:Unsubscribe(self.uiHandle)
    self.uiHandle = nil
    self.snapshot = nil
    context.managers.UI:HideOverlay("Result")
    Engine.API.Time.SetTimeScale(1.0)
end

return ResultState
