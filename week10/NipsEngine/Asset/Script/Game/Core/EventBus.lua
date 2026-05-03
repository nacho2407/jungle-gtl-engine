local EventBus = {}
EventBus.__index = EventBus

function EventBus.new()
    return setmetatable({
        listeners = {},
        nextHandle = 0
    }, EventBus)
end

function EventBus:Subscribe(eventName, owner, callback)
    if eventName == nil or callback == nil then
        return nil
    end

    self.nextHandle = self.nextHandle + 1
    local handle = self.nextHandle

    if self.listeners[eventName] == nil then
        self.listeners[eventName] = {}
    end

    table.insert(self.listeners[eventName], {
        handle = handle,
        owner = owner,
        callback = callback
    })

    return handle
end

function EventBus:Unsubscribe(handle)
    if handle == nil then
        return
    end

    for _, list in pairs(self.listeners) do
        for i = #list, 1, -1 do
            if list[i].handle == handle then
                table.remove(list, i)
                return
            end
        end
    end
end

function EventBus:ClearOwner(owner)
    if owner == nil then
        return
    end

    for _, list in pairs(self.listeners) do
        for i = #list, 1, -1 do
            if list[i].owner == owner then
                table.remove(list, i)
            end
        end
    end
end

function EventBus:Emit(eventName, payload)
    local list = self.listeners[eventName]
    if list == nil then
        return
    end

    -- 콜백 중 구독 목록이 바뀌어도 현재 emit은 안정적으로 돌게 복사합니다.
    local snapshot = {}
    for i, listener in ipairs(list) do
        snapshot[i] = listener
    end

    for _, listener in ipairs(snapshot) do
        listener.callback(payload)
    end
end

function EventBus:Clear()
    self.listeners = {}
end

return EventBus
