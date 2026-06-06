local EventBus = require("Game.LuaEventBus")
local State = require("Game.FractureState")

local Director = {
    _subscriptions = {},
    _context = nil,
}

local Events = {
    RuntimeReady = "RuntimeReady",
    TriggerEnter = "TriggerEnter",
    TriggerExit = "TriggerExit",
}

local function subscribe(eventName, callback)
    local token = EventBus.On(eventName, callback)
    if token ~= nil then
        Director._subscriptions[#Director._subscriptions + 1] = token
    end
end

local function clear_subscriptions()
    for _, token in ipairs(Director._subscriptions) do
        EventBus.Off(token)
    end
    Director._subscriptions = {}
end

function Director.Init(context)
    Director._context = context
    State.Reset()

    subscribe(Events.TriggerEnter, function(trigger, pawn, tag)
        Game.Log("TriggerEnter", tostring(tag))
        if tag == "Goal" then
            State.AddScore(1)
        elseif tag == "Restart" then
            Game.RestartLevel()
        end
    end)

    subscribe(Events.TriggerExit, function(trigger, pawn, tag)
        Game.Log("TriggerExit", tostring(tag))
    end)
end

function Director.BeginPlay(context)
    Director._context = context
    EventBus.Emit(Events.RuntimeReady, context)
    Game.Log("FractureDirector BeginPlay")
end

function Director.Tick(dt)
end

function Director.EndPlay()
    clear_subscriptions()
    Director._context = nil
end

Director.Events = Events

return Director
