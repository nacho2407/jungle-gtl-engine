local GameManager = {}
GameManager.__index = GameManager

function GameManager.new(context)
    return setmetatable({
        context = context,
        isRunning = false,
        isPaused = false,
        survivalTime = 0.0,
        killCount = 0,
        score = 0,
        killScore = 0,
        comboBonusScore = 0,
        playerHealth = 100.0,
        finishReason = "None"
    }, GameManager)
end

function GameManager:RecalculateScore()
    self.score = self.killScore + self.comboBonusScore + math.floor(self.survivalTime)
end

function GameManager:BeginPlay()
    self.context.eventBus:Subscribe("Enemy.Killed", self, function(payload)
        self:AddKill(payload)
    end)
end

function GameManager:EndPlay()
    self.context.eventBus:ClearOwner(self)
end

function GameManager:StartRun()
    Engine.API.Time.SetTimeScale(1.0)

    self.isRunning = true
    self.isPaused = false
    self.survivalTime = 0.0
    self.killCount = 0
    self.score = 0
    self.killScore = 0
    self.comboBonusScore = 0
    self.playerHealth = self.context.root.PlayerMaxHealth or 100.0
    self.finishReason = "None"

    self.context.eventBus:Emit("Game.Started", self:GetSnapshot())
end

function GameManager:Pause()
    if not self.isRunning or self.isPaused then
        return
    end
    self.isPaused = true
    Engine.API.Time.SetTimeScale(0.0)
    self.context.eventBus:Emit("Game.Paused", self:GetSnapshot())
end

function GameManager:Resume()
    if not self.isRunning or not self.isPaused then
        return
    end
    self.isPaused = false
    Engine.API.Time.SetTimeScale(1.0)
    self.context.eventBus:Emit("Game.Resumed", self:GetSnapshot())
end

function GameManager:FinishRun(reason)
    if not self.isRunning then
        return
    end

    self.isRunning = false
    self.isPaused = false
    self.finishReason = reason or "Finished"
    Engine.API.Time.SetTimeScale(1.0)

    local combo = self.context.managers.Combo
    if combo and combo.CloseCombo then
        combo:CloseCombo("Finished")
    end

    local data = self.context.managers.Data
    if data then
        data:SaveBestScore(self.score)
    end

    self.context.eventBus:Emit("Game.Finished", self:GetSnapshot())
end

function GameManager:CancelRun(reason)
    self.isRunning = false
    self.isPaused = false
    self.finishReason = reason or "Canceled"
    Engine.API.Time.SetTimeScale(1.0)
    self.context.eventBus:Emit("Game.Canceled", self:GetSnapshot())
end

function GameManager:Restart()
    self:CancelRun("Restart")
    self.context.stateMachine:Change("Loading", { reopenGameScene = true })
end

function GameManager:AddKill(payload)
    if not self.isRunning then
        return
    end

    local bonus = 100
    if payload and payload.score then
        bonus = payload.score
    end

    self.killCount = self.killCount + 1
    self.killScore = self.killScore + bonus
    self:RecalculateScore()
    self.context.eventBus:Emit("Combo.Kill", payload or { score = bonus })
    self.context.eventBus:Emit("Score.Changed", self:GetSnapshot())
end

function GameManager:AddScoreBonus(amount, reason)
    if not self.isRunning then
        return
    end

    amount = math.floor(amount or 0)
    if amount <= 0 then
        return
    end

    self.comboBonusScore = self.comboBonusScore + amount
    self:RecalculateScore()
    self.context.eventBus:Emit("Score.Bonus", {
        amount = amount,
        reason = reason or "Bonus",
        snapshot = self:GetSnapshot()
    })
    self.context.eventBus:Emit("Score.Changed", self:GetSnapshot())
end

function GameManager:DamagePlayer(amount)
    if not self.isRunning then
        return
    end

    self.playerHealth = math.max(0.0, self.playerHealth - (amount or 0.0))
    self.context.eventBus:Emit("Player.Damaged", {
        amount = amount or 0.0,
        snapshot = self:GetSnapshot()
    })
    if self.playerHealth <= 0.0 then
        self:FinishRun("Dead")
    end
end

function GameManager:Tick(dt)
    if not self.isRunning or self.isPaused then
        return
    end

    self.survivalTime = self.survivalTime + dt
    self:RecalculateScore()

    local limit = self.context.root.SessionLimitSeconds or 300.0
    if limit > 0.0 and self.survivalTime >= limit then
        self:FinishRun("TimeUp")
    end
end

function GameManager:GetSnapshot()
    return {
        isRunning = self.isRunning,
        isPaused = self.isPaused,
        survivalTime = self.survivalTime,
        killCount = self.killCount,
        score = self.score,
        killScore = self.killScore,
        comboBonusScore = self.comboBonusScore,
        playerHealth = self.playerHealth,
        finishReason = self.finishReason
    }
end

return GameManager
