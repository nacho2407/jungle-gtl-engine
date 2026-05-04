local EventBus = require("Game.Core.EventBus")
local StateMachine = require("Game.Core.StateMachine")

local DataManager = require("Game.Management.DataManager")
local ComboManager = require("Game.Management.ComboManager")
local GameManager = require("Game.Management.GameManager")
local SoundManager = require("Game.Management.SoundManager")
local UIManager = require("Game.Management.UIManager")

local StateModules = {
    Boot = "Game.States.BootState",
    Intro = "Game.States.IntroState",
    Title = "Game.States.TitleState",
    Loading = "Game.States.LoadingState",
    Playing = "Game.States.PlayingState",
    Pause = "Game.States.PauseState",
    Result = "Game.States.ResultState"
}

for _, moduleName in pairs(StateModules) do
    package.loaded[moduleName] = nil
end

local BootState = require(StateModules.Boot)
local IntroState = require(StateModules.Intro)
local TitleState = require(StateModules.Title)
local LoadingState = require(StateModules.Loading)
local PlayingState = require(StateModules.Playing)
local PauseState = require(StateModules.Pause)
local ResultState = require(StateModules.Result)

local Script = {}
Script.__index = Script

local ScenePaths = {
    Intro = "Asset/Scene/Intro.Scene",
    Main = "Asset/Scene/Main.Scene",
    Game = "Asset/Scene/MapScene.Scene"
}

-- ScriptComponent Details에 노출되는 값입니다.
-- 게임잼 중에는 C++ 수정 없이 여기 값을 바꿔 기본 흐름을 조율할 수 있게 둡니다.
Script.Properties = {
    AutoStart = {
        Type = "Bool",
        Default = true,
        Category = "Management"
    },
    StartingState = {
        Type = "String",
        Default = "Boot",
        Category = "Management"
    },
    SessionLimitSeconds = {
        Type = "Float",
        Default = 5.0,
        Category = "Management"
    },
    PlayerMaxHealth = {
        Type = "Float",
        Default = 100.0,
        Category = "Management"
    },
    ComboTimeoutSeconds = {
        Type = "Float",
        Default = 5.0,
        Category = "Management"
    },
    ComboBonusPerKill = {
        Type = "Float",
        Default = 2.0,
        Category = "Management"
    },
    PlayerDamagePerHit = {
        Type = "Float",
        Default = 20.0,
        Category = "Management"
    },
    PlayerInvulnerableSeconds = {
        Type = "Float",
        Default = 3.0,
        Category = "Management"
    }
}

local function ApplyProperties(target, properties)
    properties = properties or {}

    for key, desc in pairs(Script.Properties) do
        if properties[key] ~= nil then
            target[key] = properties[key]
        else
            target[key] = desc.Default
        end
    end
end

function Script.new(component, properties)
    local self = setmetatable({}, Script)

    self.component = component
    self.owner = component:GetOwner()
    ApplyProperties(self, properties)

    -- context는 모든 manager/state가 공유하는 작은 런타임 묶음입니다.
    -- 전역 싱글톤을 직접 늘리지 않고도 필요한 것들에 접근할 수 있게 합니다.
    self.context = {
        root = self,
        component = self.component,
        owner = self.owner,
        eventBus = EventBus.new(),
        managers = {},
        stateMachine = nil
    }

    self.context.stateMachine = StateMachine.new(self.context)
    self.context.managers.Data = DataManager.new(self.context)
    self.context.managers.Sound = SoundManager.new(self.context)
    self.context.managers.UI = UIManager.new(self.context)
    self.context.managers.Game = GameManager.new(self.context)
    self.context.managers.Combo = ComboManager.new(self.context)

    self.context.stateMachine:Register("Boot", BootState.new())
    self.context.stateMachine:Register("Intro", IntroState.new())
    self.context.stateMachine:Register("Title", TitleState.new())
    self.context.stateMachine:Register("Loading", LoadingState.new())
    self.context.stateMachine:Register("Playing", PlayingState.new())
    self.context.stateMachine:Register("Pause", PauseState.new())
    self.context.stateMachine:Register("Result", ResultState.new())

    return self
end

function Script:OpenMainScene()
    return Engine.API.Scene.Open(ScenePaths.Main)
end

function Script:OpenIntroScene()
    return Engine.API.Scene.Open(ScenePaths.Intro)
end

function Script:OpenGameScene()
    return Engine.API.Scene.Open(ScenePaths.Game)
end

function Script:NotifyEnemyKilled(score)
    self.context.eventBus:Emit("Enemy.Killed", {
        score = score or 1
    })
end

function Script:AddComboKill(score)
    self:NotifyEnemyKilled(score)
end

function Script:DamagePlayer(amount, source)
    local game = self.context.managers.Game
    if game and game.DamagePlayer then
        return game:DamagePlayer(amount or self.PlayerDamagePerHit, source)
    end

    return false
end

function Script:RecoverPlayer(amount, source)
    local game = self.context.managers.Game
    if game and game.RecoverPlayer then
        return game:RecoverPlayer(amount, source)
    end

    return false
end

function Script:InstallGameJamBridge()
    _G.GameJam = _G.GameJam or {}
    _G.GameJam.Manager = self
    _G.GameJam.EventBus = self.context.eventBus
    _G.GameJam.NotifyEnemyKilled = function(payload)
        if self.context == nil or self.context.eventBus == nil then
            return false
        end

        if type(payload) == "table" then
            self.context.eventBus:Emit("Enemy.Killed", payload)
        else
            self:NotifyEnemyKilled(payload)
        end

        return true
    end
    _G.GameJam.DamagePlayer = function(amount, source)
        return self:DamagePlayer(amount, source)
    end
    _G.GameJam.RecoverPlayer = function(amount, source)
        return self:RecoverPlayer(amount, source)
    end
    _G.GameJam.NotifyPlayerAttackStarted = function(attackId)
        self.context.eventBus:Emit("Player.AttackStarted", { attackId = tostring(attackId or "") })
    end
    _G.GameJam.NotifyPlayerAttackHit = function(attackId)
        self.context.eventBus:Emit("Player.AttackHit", { attackId = tostring(attackId or "") })
    end
    _G.GameJam.NotifyPlayerAttackFinished = function(attackId)
        self.context.eventBus:Emit("Player.AttackFinished", { attackId = tostring(attackId or "") })
    end
    _G.GameJam.NotifyPlayerAttackGround = function(payload)
        self.context.eventBus:Emit("Player.AttackGround", payload or {})
    end
    _G.GameJam.NotifyTimeSlowStarted = function()
        self.context.eventBus:Emit("TimeSlow.Started", {})
    end
    _G.GameJam.NotifyTimeSlowEnded = function()
        self.context.eventBus:Emit("TimeSlow.Ended", {})
    end
end

function Script:BeginPlay()
    Engine.API.Debug.Log("[GeneralManager] BeginPlay")
    self:InstallGameJamBridge()

    for _, manager in pairs(self.context.managers) do
        if manager.BeginPlay then
            manager:BeginPlay()
        end
    end

    if self.AutoStart then
        self.context.stateMachine:Change(self.StartingState)
    end
end

function Script:Tick(dt)
    local managers = self.context.managers

    -- UI 이벤트는 먼저 수집해서 State/GameManager가 같은 프레임에 반응할 수 있게 합니다.
    if managers.UI and managers.UI.Tick then
        managers.UI:Tick(dt)
    end

    if managers.Game and managers.Game.Tick then
        managers.Game:Tick(dt)
    end

    if managers.Combo and managers.Combo.Tick then
        managers.Combo:Tick(dt)
    end

    self.context.stateMachine:Tick(dt)
end

function Script:EndPlay()
    Engine.API.Debug.Log("[GeneralManager] EndPlay")

    self.context.stateMachine:Clear()

    for _, manager in pairs(self.context.managers) do
        if manager.EndPlay then
            manager:EndPlay()
        end
    end

    self.context.eventBus:Clear()

    if _G.GameJam and _G.GameJam.Manager == self then
        _G.GameJam.Manager = nil
        _G.GameJam.EventBus = nil
        _G.GameJam.NotifyEnemyKilled = nil
        _G.GameJam.DamagePlayer = nil
        _G.GameJam.RecoverPlayer = nil
        _G.GameJam.NotifyPlayerAttackStarted = nil
        _G.GameJam.NotifyPlayerAttackHit = nil
        _G.GameJam.NotifyPlayerAttackFinished = nil
        _G.GameJam.NotifyPlayerAttackGround = nil
        _G.GameJam.NotifyTimeSlowStarted = nil
        _G.GameJam.NotifyTimeSlowEnded = nil
    end
end

return Script
