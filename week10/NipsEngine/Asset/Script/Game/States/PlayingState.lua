local PlayingState = {}
PlayingState.__index = PlayingState

function PlayingState.new()
    return setmetatable({
        finishHandle = nil
    }, PlayingState)
end

function PlayingState:Enter(context)
    context.managers.UI:HideAllOverlays()
    context.managers.UI:Hide("Loading")
    context.managers.Game:StartRun()
    context.managers.UI:Show("HUD")
    context.managers.Sound:PlayBGM(context.managers.Sound.BGM.Gameplay)
    Engine.API.Input.SetInputModeGameOnly()

    self.finishHandle = context.eventBus:Subscribe("Game.Finished", self, function(snapshot)
        context.stateMachine:Change("Result", snapshot)
    end)
end

function PlayingState:Exit(context)
    context.eventBus:Unsubscribe(self.finishHandle)
    self.finishHandle = nil
end

function PlayingState:Pause(context)
    context.managers.Game:Pause()
end

function PlayingState:Resume(context)
    context.managers.Game:Resume()
    context.managers.UI:HideOverlay("Pause")
    context.managers.UI:Show("HUD")
    Engine.API.Input.SetInputModeGameOnly()
end

function PlayingState:Tick(context, dt)
    local game = context.managers.Game
    context.managers.UI:SetHUD(game:GetSnapshot())

    if Engine.API.Input.IsKeyPressed("Escape") or Engine.API.Input.IsKeyPressed("Q") then
        context.stateMachine:Push("Pause")
    end
end

return PlayingState
