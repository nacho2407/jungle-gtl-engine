local LoadingState = {}
LoadingState.__index = LoadingState

function LoadingState.new()
    return setmetatable({
        elapsed = 0.0,
        ready = false,
        waitingForSceneOpen = false
    }, LoadingState)
end

function LoadingState:Enter(context, payload)
    self.elapsed = 0.0
    self.ready = false
    self.waitingForSceneOpen = false

    context.managers.UI:Show("Loading")
    context.managers.UI:SetLoadingReady(false)
    Engine.API.Input.SetInputModeGameAndUI()
    Engine.API.Time.SetTimeScale(1.0)

    if payload and payload.reopenGameScene then
        self.waitingForSceneOpen = context.root:OpenGameScene()
        if not self.waitingForSceneOpen then
            Engine.API.Debug.Warn("[LoadingState] Failed to reopen game scene. Continuing in current scene.")
        end
    end
end

function LoadingState:Tick(context, dt)
    if self.waitingForSceneOpen then
        if Engine.API.Scene.IsOpenPending() then
            return
        end
        self.waitingForSceneOpen = false
    end

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
