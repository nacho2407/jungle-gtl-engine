local BootState = {}
BootState.__index = BootState

function BootState.new()
    return setmetatable({
        elapsed = 0.0
    }, BootState)
end

function BootState:Enter(context)
    self.elapsed = 0.0
    context.managers.UI:Show("Boot")
end

function BootState:Tick(context, dt)
    self.elapsed = self.elapsed + dt
    if self.elapsed >= 0.1 then
        context.stateMachine:Change("Intro", { returnTo = "Title" })
    end
end

return BootState
