local PresentationSequence = {}
PresentationSequence.__index = PresentationSequence

function PresentationSequence.new()
    return setmetatable({
        steps = {},
        index = 1,
        waitRemaining = 0.0,
        active = false
    }, PresentationSequence)
end

function PresentationSequence:Clear()
    self.steps = {}
    self.index = 1
    self.waitRemaining = 0.0
    self.active = false
end

function PresentationSequence:Play()
    self.index = 1
    self.waitRemaining = 0.0
    self.active = true
end

function PresentationSequence:Stop()
    self.active = false
    self.waitRemaining = 0.0
end

function PresentationSequence:IsActive()
    return self.active == true
end

function PresentationSequence:Delay(seconds)
    table.insert(self.steps, {
        type = "delay",
        duration = tonumber(seconds) or 0.0
    })
    return self
end

function PresentationSequence:Call(callback)
    table.insert(self.steps, {
        type = "call",
        callback = callback
    })
    return self
end

function PresentationSequence:WaitUntil(predicate, timeoutSeconds)
    table.insert(self.steps, {
        type = "waitUntil",
        predicate = predicate,
        timeout = timeoutSeconds,
        elapsed = 0.0
    })
    return self
end

function PresentationSequence:Tick(dt)
    if not self.active then
        return
    end

    local delta = tonumber(dt) or 0.0

    while self.active do
        local step = self.steps[self.index]
        if step == nil then
            self.active = false
            return
        end

        if step.type == "delay" then
            if self.waitRemaining <= 0.0 then
                self.waitRemaining = step.duration
            end

            self.waitRemaining = self.waitRemaining - delta
            if self.waitRemaining > 0.0 then
                return
            end

            self.waitRemaining = 0.0
            self.index = self.index + 1
            delta = 0.0
        elseif step.type == "call" then
            if step.callback ~= nil then
                step.callback()
            end
            self.index = self.index + 1
        elseif step.type == "waitUntil" then
            if step.predicate == nil or step.predicate() then
                self.index = self.index + 1
            else
                step.elapsed = (step.elapsed or 0.0) + delta
                if step.timeout ~= nil and step.elapsed >= step.timeout then
                    self.index = self.index + 1
                end
                return
            end
        else
            self.index = self.index + 1
        end
    end
end

return PresentationSequence
