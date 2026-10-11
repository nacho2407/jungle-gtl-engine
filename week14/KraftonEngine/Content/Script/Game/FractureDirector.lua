local EventBus = require("Game.LuaEventBus")
local CombatEvents = require("Game.CombatEvents")
local State = require("Game.FractureState")
local GameOver = require("GameOver")

local DEATH_ZONE_TAG = "DeathZone"

local Director = {
    _subscriptions = {},
    _context = nil,
    _bossDefeated = false,
}

local Events = {
    RuntimeReady = "RuntimeReady",
    TriggerEnter = "TriggerEnter",
    TriggerExit = "TriggerExit",
    BossDefeated = "BossDefeated",
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

local function add_death_score()
    if ScoreManager ~= nil and ScoreManager.AddDeath ~= nil then
        ScoreManager.AddDeath(1)
    end
end

local function actor_has_tag(actor, tag)
    return actor ~= nil
        and type(actor.IsValid) == "function"
        and actor:IsValid()
        and type(actor.HasTag) == "function"
        and actor:HasTag(tag)
end

local function is_boss_death(result)
    if result == nil then
        return false
    end

    if result.bBoss == true or result.boss == true or result.IsBoss == true or result.isBoss == true then
        return true
    end

    return actor_has_tag(result.Victim or result.victim, "boss")
end

local function trigger_boss_defeated(context, result)
    if Director._bossDefeated then
        return
    end

    Director._bossDefeated = true
    State.MarkEndingReached()
    EventBus.Emit(Events.BossDefeated, context, result)

    if Game ~= nil and Game.Log ~= nil then
        Game.Log("BossDefeated")
    end
end

local function trigger_game_over_by_death_zone()
    -- 게임 오버 UI가 이미 열린 상태에서 트리거가 반복 호출될 때의 중복 사망 집계 방지
    if GameOver.IsOpen ~= nil and GameOver.IsOpen() then
        return
    end

    -- HP 기반 사망과 동일한 점수 통계 경로 사용
    add_death_score()
    GameOver.Show()
end

function Director.Init(context)
    Director._context = context
    State.Reset()
    Director._bossDefeated = false

    subscribe(CombatEvents.Events.AttackKilled, function(context, result)
        if is_boss_death(result) then
            trigger_boss_defeated(context, result)
        end
    end)

    subscribe(Events.TriggerEnter, function(trigger, pawn, tag)
        Game.Log("TriggerEnter", tostring(tag))
        if tag == DEATH_ZONE_TAG then
            trigger_game_over_by_death_zone()
        elseif tag == "Goal" then
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
    Director._bossDefeated = false
end

Director.Events = Events

return Director
