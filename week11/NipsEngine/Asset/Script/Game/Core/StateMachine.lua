local StateMachine = {}
StateMachine.__index = StateMachine

function StateMachine.new(context)
    return setmetatable({
        context = context,
        states = {},
        stack = {}
    }, StateMachine)
end

function StateMachine:Register(name, state)
    if name == nil or state == nil then
        return
    end

    state.name = state.name or name
    self.states[name] = state
end

function StateMachine:GetCurrent()
    return self.stack[#self.stack]
end

function StateMachine:GetCurrentName()
    local current = self:GetCurrent()
    return current and current.name or nil
end

function StateMachine:Change(name, payload)
    for i = #self.stack, 1, -1 do
        local state = self.stack[i]
        if state and state.Exit then
            state:Exit(self.context, payload)
        end
    end

    self.stack = {}
    local nextState = self.states[name]
    if nextState == nil then
        Engine.API.Debug.Warn("[StateMachine] Unknown state: " .. tostring(name))
        return
    end

    table.insert(self.stack, nextState)
    if nextState.Enter then
        nextState:Enter(self.context, payload)
    end
end

function StateMachine:Push(name, payload)
    local current = self:GetCurrent()
    if current and current.Pause then
        current:Pause(self.context, payload)
    end

    local nextState = self.states[name]
    if nextState == nil then
        Engine.API.Debug.Warn("[StateMachine] Unknown state: " .. tostring(name))
        return
    end

    table.insert(self.stack, nextState)
    if nextState.Enter then
        nextState:Enter(self.context, payload)
    end
end

function StateMachine:Pop(payload)
    local current = self:GetCurrent()
    if current == nil then
        return
    end

    if current.Exit then
        current:Exit(self.context, payload)
    end
    table.remove(self.stack)

    local resumed = self:GetCurrent()
    if resumed and resumed.Resume then
        resumed:Resume(self.context, payload)
    end
end

function StateMachine:Tick(dt)
    local current = self:GetCurrent()
    if current and current.Tick then
        current:Tick(self.context, dt)
    end
end

function StateMachine:Clear()
    for i = #self.stack, 1, -1 do
        local state = self.stack[i]
        if state and state.Exit then
            state:Exit(self.context)
        end
    end
    self.stack = {}
end

return StateMachine
