local EventBus = {
    _nextToken = 1,
    _listeners = {},
}

local function ensure_list(eventName)
    local list = EventBus._listeners[eventName]
    if list == nil then
        list = {}
        EventBus._listeners[eventName] = list
    end
    return list
end

function EventBus.On(eventName, callback)
    if eventName == nil or callback == nil then
        return nil
    end

    local token = EventBus._nextToken
    EventBus._nextToken = EventBus._nextToken + 1

    local list = ensure_list(eventName)
    list[token] = callback
    return {
        EventName = eventName,
        Token = token,
    }
end

function EventBus.Off(subscription)
    if subscription == nil then
        return
    end

    local eventName = subscription.EventName
    local token = subscription.Token
    local list = EventBus._listeners[eventName]
    if list ~= nil then
        list[token] = nil
    end
end

function EventBus.Emit(eventName, ...)
    local list = EventBus._listeners[eventName]
    if list == nil then
        return
    end

    -- 콜백 중 구독 해제가 일어나도 순회가 흔들리지 않도록 token만 복사합니다.
    local tokens = {}
    for token, _ in pairs(list) do
        tokens[#tokens + 1] = token
    end

    for _, token in ipairs(tokens) do
        local callback = list[token]
        if callback ~= nil then
            callback(...)
        end
    end
end

function EventBus.Clear()
    EventBus._listeners = {}
end

return EventBus
