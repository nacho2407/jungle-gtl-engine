local LoadingState = {}
LoadingState.__index = LoadingState

function LoadingState.new()
    return setmetatable({
        elapsed = 0.0,
        ready = false
    }, LoadingState)
end

function LoadingState:Enter(context)
    self.elapsed = 0.0
    self.ready = false

    context.managers.UI:Show("Loading")
    context.managers.UI:SetLoadingReady(false)
    Engine.API.Input.SetInputModeGameAndUI()
end

function LoadingState:Tick(context, dt)
    local realDt = Engine.API.Time.GetUnscaledDeltaTime()
    self.elapsed = self.elapsed + realDt

    if not self.ready and self.elapsed >= 0.6 then
        self.ready = true
        context.managers.UI:SetLoadingReady(true)
    end

    if self.ready and Engine.API.Input.IsKeyPressed("Space") then
        context.stateMachine:Change("Playing")
    end
end

return LoadingState
