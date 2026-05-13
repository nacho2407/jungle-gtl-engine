local PlayingState = {}
PlayingState.__index = PlayingState

function PlayingState.new()
    return setmetatable({
        finishHandle = nil,
        pendingResult = nil,
        resultDelayRemaining = 0.0
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
        if self.pendingResult ~= nil then
            return
        end

        self.pendingResult = snapshot or {}
        context.managers.UI:SetHUD(self.pendingResult)
        self.resultDelayRemaining = 2.0
        context.managers.Sound:StopBGM(2.0)
    end)
end

function PlayingState:Exit(context)
    context.eventBus:Unsubscribe(self.finishHandle)
    self.finishHandle = nil
    self.pendingResult = nil
    self.resultDelayRemaining = 0.0
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
    if self.pendingResult ~= nil then
        context.managers.UI:SetHUD(self.pendingResult)
        local realDt = Engine.API.World.GetUnscaledDeltaTime()
        self.resultDelayRemaining = self.resultDelayRemaining - realDt
        if self.resultDelayRemaining <= 0.0 then
            local snapshot = self.pendingResult
            self.pendingResult = nil
            self.resultDelayRemaining = 0.0
            if snapshot ~= nil and snapshot.isClear == true then
                context.stateMachine:Change("Ending", snapshot)
            else
                context.stateMachine:Change("Result", snapshot)
            end
        end
        return
    end

    local game = context.managers.Game
    context.managers.UI:SetHUD(game:GetSnapshot())

    if Engine.API.Input.WasActionStarted("Pause") then
        context.stateMachine:Push("Pause")
    end
end

return PlayingState
