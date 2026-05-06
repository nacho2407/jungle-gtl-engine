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
        playerMaxHealth = 100.0,
        invulnerableRemaining = 0.0,
        timeSlowActive = false,
        timeSlowRemaining = 0.0,
        timeSlowScale = 1.0,
        hitStopActive = false,
        hitStopRemaining = 0.0,
        hitStopScale = 1.0,
        finishReason = "None",
        isClear = false
    }, GameManager)
end

local function IsClearReason(reason)
    return reason == "Clear"
        or reason == "Victory"
        or reason == "Win"
        or reason == "Completed"
        or reason == "TimeUp"
end

function GameManager:RecalculateScore()
    self.score = self.killScore + self.comboBonusScore
end

function GameManager:GetPlayerHealthSnapshot()
    if _G.GameJam and _G.GameJam.GetPlayerHealthSnapshot then
        local snapshot = _G.GameJam.GetPlayerHealthSnapshot()
        if type(snapshot) == "table" then
            self.playerHealth = tonumber(snapshot.health) or self.playerHealth
            self.playerMaxHealth = tonumber(snapshot.maxHealth) or self.playerMaxHealth
            return snapshot
        end
    end

    return {
        health = self.playerHealth,
        maxHealth = self.playerMaxHealth,
        ratio = self.playerMaxHealth > 0.0 and (self.playerHealth / self.playerMaxHealth) or 0.0
    }
end

function GameManager:ResetPlayerHealth()
    local maxHealth = tonumber(self.context.root.PlayerMaxHealth) or self.playerMaxHealth or 100.0
    if _G.GameJam and _G.GameJam.ResetPlayerHealth then
        local snapshot = _G.GameJam.ResetPlayerHealth(maxHealth)
        if type(snapshot) == "table" then
            self.playerMaxHealth = tonumber(snapshot.maxHealth) or maxHealth
            self.playerHealth = tonumber(snapshot.health) or self.playerMaxHealth
            return snapshot
        end
    end

    self.playerMaxHealth = maxHealth
    self.playerHealth = self.playerMaxHealth
    return self:GetPlayerHealthSnapshot()
end

function GameManager:BeginPlay()
    self.context.eventBus:Subscribe("Enemy.Killed", self, function(payload)
        self:AddKill(payload)
    end)

    self.context.eventBus:Subscribe("Player.DamageRequested", self, function(payload)
        payload = payload or {}
        self:DamagePlayer(payload.amount, payload.source)
    end)
end

function GameManager:EndPlay()
    self.context.eventBus:ClearOwner(self)
end

function GameManager:StartRun()
    self:StopTimeSlow(false)
    Engine.API.World.SetTimeScale(1.0)

    self.isRunning = true
    self.isPaused = false
    self.survivalTime = 0.0
    self.killCount = 0
    self.score = 0
    self.killScore = 0
    self.comboBonusScore = 0
    self.playerMaxHealth = self.context.root.PlayerMaxHealth or 100.0
    self.playerHealth = self.playerMaxHealth
    self:ResetPlayerHealth()
    self.invulnerableRemaining = 0.0
    self.timeSlowActive = false
    self.timeSlowRemaining = 0.0
    self.timeSlowScale = 1.0
    self.finishReason = "None"
    self.isClear = false

    self.context.eventBus:Emit("Game.Started", self:GetSnapshot())
end

function GameManager:Pause()
    if not self.isRunning or self.isPaused then
        return
    end
    self.isPaused = true
    Engine.API.World.SetTimeScale(0.0)
    self.context.eventBus:Emit("Game.Paused", self:GetSnapshot())
end

function GameManager:Resume()
    if not self.isRunning or not self.isPaused then
        return
    end
    self.isPaused = false
    Engine.API.World.SetTimeScale(self.timeSlowActive and self.timeSlowScale or 1.0)
    self.context.eventBus:Emit("Game.Resumed", self:GetSnapshot())
end

function GameManager:FinishRun(reason)
    if not self.isRunning then
        return
    end

    local combo = self.context.managers.Combo
    if combo and combo.CloseCombo then
        combo:CloseCombo("Finished")
    end

    self.isRunning = false
    self.isPaused = false
    self.finishReason = reason or "Finished"
    self.isClear = IsClearReason(self.finishReason)
    self:StopTimeSlow(true)
    Engine.API.World.SetTimeScale(1.0)

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
    self.isClear = false
    self:StopTimeSlow(false)
    Engine.API.World.SetTimeScale(1.0)
    self.context.eventBus:Emit("Game.Canceled", self:GetSnapshot())
end

function GameManager:Restart()
    self:CancelRun("Restart")
    self.context.stateMachine:Change("Loading", { reopenGameScene = true })
end

function GameManager:AddKill(payload)
    if not self.isRunning then
        return false
    end

    payload = payload or {}

    local bonus = 1
    if payload.score then
        bonus = payload.score
    end
    bonus = math.max(0, math.floor(tonumber(bonus) or 1))

    self.killCount = self.killCount + 1
    self.killScore = self.killScore + bonus
    self:RecalculateScore()
    payload.score = bonus
    payload.killCount = self.killCount
    self.context.eventBus:Emit("Combo.Kill", payload)
    self.context.eventBus:Emit("Score.Changed", self:GetSnapshot())
    return true
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

function GameManager:DamagePlayer(amount, source)
    if not self.isRunning then
        return false
    end

    if self.invulnerableRemaining > 0.0 then
        self.context.eventBus:Emit("Player.DamageBlocked", {
            amount = amount or 0.0,
            source = source,
            invulnerableRemaining = self.invulnerableRemaining,
            snapshot = self:GetSnapshot()
        })
        return false
    end

    local damage = tonumber(amount) or tonumber(self.context.root.PlayerDamagePerHit) or 20.0
    if damage <= 0.0 then
        return false
    end

    local appliedDamage = damage
    if _G.GameJam and _G.GameJam.ApplyPlayerDamage then
        local actualDamage, snapshot = _G.GameJam.ApplyPlayerDamage(damage, source)
        appliedDamage = tonumber(actualDamage) or 0.0
        if type(snapshot) == "table" then
            self.playerHealth = tonumber(snapshot.health) or self.playerHealth
            self.playerMaxHealth = tonumber(snapshot.maxHealth) or self.playerMaxHealth
        else
            self:GetPlayerHealthSnapshot()
        end
    else
        self.playerHealth = math.max(0.0, self.playerHealth - damage)
    end

    if appliedDamage <= 0.0 then
        return false
    end

    if self.playerHealth > 0.0 then
        self.invulnerableRemaining = tonumber(self.context.root.PlayerInvulnerableSeconds) or 3.0
    else
        self.invulnerableRemaining = 0.0
    end

    self.context.eventBus:Emit("Player.Damaged", {
        amount = appliedDamage,
        source = source,
        invulnerableSeconds = self.invulnerableRemaining,
        snapshot = self:GetSnapshot()
    })
    if self.playerHealth <= 0.0 then
        self:FinishRun("Dead")
    end

    return true
end

function GameManager:RecoverPlayer(amount, source)
    if not self.isRunning then
        return false
    end

    local recoverAmount = tonumber(amount) or 0.0
    if recoverAmount <= 0.0 then
        return false
    end

    local actualRecover = recoverAmount
    if _G.GameJam and _G.GameJam.RecoverPlayerHealth then
        local recovered, snapshot = _G.GameJam.RecoverPlayerHealth(recoverAmount, source)
        actualRecover = tonumber(recovered) or 0.0
        if type(snapshot) == "table" then
            self.playerHealth = tonumber(snapshot.health) or self.playerHealth
            self.playerMaxHealth = tonumber(snapshot.maxHealth) or self.playerMaxHealth
        else
            self:GetPlayerHealthSnapshot()
        end
    else
        local oldHealth = self.playerHealth
        self.playerHealth = math.min(self.playerMaxHealth or 100.0, self.playerHealth + recoverAmount)
        actualRecover = self.playerHealth - oldHealth
    end

    if actualRecover <= 0.0 then
        return false
    end

    self.context.eventBus:Emit("Player.Recovered", {
        amount = actualRecover,
        source = source,
        snapshot = self:GetSnapshot()
    })
    return true
end

function GameManager:ActivateTimeSlow(duration, scale, source)
    if not self.isRunning or self.isPaused then
        return false
    end

    self.timeSlowScale = math.max(0.05, math.min(1.0, tonumber(scale) or 1.0))
    self.timeSlowRemaining = math.max(0.0, tonumber(duration) or 10.0)
    if self.timeSlowRemaining <= 0.0 then
        return false
    end

    local wasActive = self.timeSlowActive
    self.timeSlowActive = true
    Engine.API.World.SetTimeScale(self.timeSlowScale)
    Engine.API.World.ActivateSandervistan()

    if not wasActive then
        self.context.eventBus:Emit("TimeSlow.Started", {
            duration = self.timeSlowRemaining,
            scale = self.timeSlowScale,
            source = source,
            snapshot = self:GetSnapshot()
        })
    end

    return true
end

function GameManager:ActivateHitStop(duration, scale)
    local d = math.max(0.0, tonumber(duration) or 0.05)
    local s = math.max(0.0, math.min(1.0, tonumber(scale) or 0.0))
    if d <= 0.0 then
        return false
    end

    self.hitStopActive = true
    self.hitStopRemaining = d
    self.hitStopScale = s
    Engine.API.World.SetTimeScale(
        (self.timeSlowActive and self.timeSlowScale or 1.0) * s
    )
    return true
end

function GameManager:GetHitStopScale()
    return self.hitStopScale
end

function GameManager:GetSlowMotionScale()
    return self.timeSlowScale
end

function GameManager:StopTimeSlow(emitEvent)
    if self.timeSlowActive then
        self.timeSlowActive = false
        self.timeSlowRemaining = 0.0
        self.timeSlowScale = 1.0
        Engine.API.World.DeactivateSandervistan()
        Engine.API.World.SetTimeScale(
            self.hitStopActive and self.hitStopScale or 1.0
        )

        if emitEvent then
            self.context.eventBus:Emit("TimeSlow.Ended", self:GetSnapshot())
        end
    end
end

function GameManager:Tick(dt)
    local realDt = Engine.API.World.GetUnscaledDeltaTime()
    if realDt <= 0.0 then
        realDt = dt or 0.0
    end

    if self.hitStopActive then
        self.hitStopRemaining = math.max(0.0, self.hitStopRemaining - realDt)
        if self.hitStopRemaining <= 0.0 then
            self.hitStopActive = false
            self.hitStopScale = 1.0
            Engine.API.World.SetTimeScale(
                self.timeSlowActive and self.timeSlowScale or 1.0
            )
        end
    end

    if not self.isRunning or self.isPaused then
        return
    end

    local realDt = Engine.API.World.GetUnscaledDeltaTime()
    if realDt <= 0.0 then
        realDt = dt or 0.0
    end

    if self.timeSlowActive then
        self.timeSlowRemaining = math.max(0.0, self.timeSlowRemaining - realDt)
        if self.timeSlowRemaining <= 0.0 then
            self:StopTimeSlow(true)
        end
    end

    if self.invulnerableRemaining > 0.0 then
        self.invulnerableRemaining = math.max(0.0, self.invulnerableRemaining - realDt)
    end

    self.survivalTime = self.survivalTime + realDt
    self:RecalculateScore()

    local limit = self.context.root.SessionLimitSeconds or 5.0
    if limit > 0.0 and self.survivalTime >= limit then
        self:FinishRun("TimeUp")
    end
end

function GameManager:GetSnapshot()
    local limit = self.context.root.SessionLimitSeconds or 5.0
    self:GetPlayerHealthSnapshot()
    local maxHealth = self.playerMaxHealth or self.context.root.PlayerMaxHealth or 100.0
    return {
        isRunning = self.isRunning,
        isPaused = self.isPaused,
        survivalTime = self.survivalTime,
        sessionLimitSeconds = limit,
        killCount = self.killCount,
        score = self.score,
        killScore = self.killScore,
        comboBonusScore = self.comboBonusScore,
        playerHealth = self.playerHealth,
        playerMaxHealth = maxHealth,
        playerHealthRatio = maxHealth > 0.0 and (self.playerHealth / maxHealth) or 0.0,
        invulnerableRemaining = self.invulnerableRemaining,
        timeSlowActive = self.timeSlowActive,
        timeSlowRemaining = self.timeSlowRemaining,
        timeSlowScale = self.timeSlowScale,
        hitStopActive = false,
        hitStopRemaining = 0.0,
        hitStopScale = 1.0,
        finishReason = self.finishReason,
        isClear = self.isClear
    }
end

return GameManager
