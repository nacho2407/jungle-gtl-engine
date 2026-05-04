local LoadingState = {}
LoadingState.__index = LoadingState

function LoadingState.new()
    return setmetatable({
        elapsed = 0.0,
        ready = false,
        waitingForSceneOpen = false,
        minDisplaySeconds = 3.0
    }, LoadingState)
end

function LoadingState:Enter(context, payload)
    self.elapsed = 0.0
    self.ready = false
    self.waitingForSceneOpen = false

    context.managers.UI:Show("Loading")
    context.managers.UI:SelectLoadingTip()
    context.managers.UI:SetLoadingReady(false)
    context.managers.UI:SetLoadingCycleRotation(0.0)
    context.managers.Sound:StopBGM(0.5)
    Engine.API.Input.SetInputModeUIOnly()
    Engine.API.World.SetTimeScale(0.0)

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

    local realDt = Engine.API.World.GetUnscaledDeltaTime()
    self.elapsed = self.elapsed + realDt
    context.managers.UI:SetLoadingCycleRotation((self.elapsed * 360.0) % 360.0)

    if not self.ready and self.elapsed >= self.minDisplaySeconds then
        self.ready = true
        context.managers.Sound:PlayLoadingDone()
        context.managers.UI:SetLoadingReady(true)
    end

    if self.ready and Engine.API.Input.IsUIKeyPressed("Space") then
        context.stateMachine:Change("Playing")
    end
end

return LoadingState
