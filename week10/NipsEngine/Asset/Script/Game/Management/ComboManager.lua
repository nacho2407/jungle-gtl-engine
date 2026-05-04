local ComboManager = {}
ComboManager.__index = ComboManager

local function Clamp01(value)
    value = tonumber(value) or 0.0
    if value < 0.0 then
        return 0.0
    elseif value > 1.0 then
        return 1.0
    end
    return value
end

function ComboManager.new(context)
    return setmetatable({
        context = context,
        comboCount = 0,
        displayCount = 0,
        pendingScore = 0,
        lastBonus = 0,
        remaining = 0.0,
        alpha = 0.0,
        isVisible = false,
        isFading = false,
        shakeTime = 0.0,
        timeoutSeconds = 2.4,
        fadeSeconds = 0.45,
        shakeSeconds = 0.18,
        shakeMagnitude = 10.0,
        bonusRatio = 0.25
    }, ComboManager)
end

function ComboManager:BeginPlay()
    local root = self.context.root
    self.timeoutSeconds = root.ComboTimeoutSeconds or self.timeoutSeconds
    self.bonusRatio = root.ComboBonusRatio or self.bonusRatio

    self.context.eventBus:Subscribe("Game.Started", self, function()
        self:Reset()
    end)

    self.context.eventBus:Subscribe("Game.Finished", self, function()
        self:Reset()
    end)

    self.context.eventBus:Subscribe("Game.Canceled", self, function()
        self:Reset()
    end)

    self.context.eventBus:Subscribe("Combo.Kill", self, function(payload)
        self:AddKill(payload)
    end)

    self.context.eventBus:Subscribe("Player.Damaged", self, function()
        self:CloseCombo("Damaged")
    end)
end

function ComboManager:EndPlay()
    self.context.eventBus:ClearOwner(self)
end

function ComboManager:Reset()
    self.comboCount = 0
    self.displayCount = 0
    self.pendingScore = 0
    self.lastBonus = 0
    self.remaining = 0.0
    self.alpha = 0.0
    self.isVisible = false
    self.isFading = false
    self.shakeTime = 0.0
end

function ComboManager:AddKill(payload)
    local game = self.context.managers.Game
    if game == nil or not game.isRunning or game.isPaused then
        return
    end

    local score = 100
    if payload and payload.score then
        score = payload.score
    end

    self.comboCount = self.comboCount + 1
    self.displayCount = self.comboCount
    self.pendingScore = self.pendingScore + score
    self.lastBonus = self.comboCount >= 2 and math.floor(self.pendingScore * self.bonusRatio) or 0
    self.remaining = self.timeoutSeconds
    self.alpha = 1.0
    self.isVisible = true
    self.isFading = false
    self.shakeTime = self.shakeSeconds

    self.context.eventBus:Emit("Combo.Changed", self:GetSnapshot())
end

function ComboManager:CloseCombo(reason)
    if self.comboCount <= 0 then
        return
    end

    local bonus = 0
    if self.comboCount >= 2 then
        bonus = math.floor(self.pendingScore * self.bonusRatio)
    end

    if bonus > 0 and self.context.managers.Game then
        self.context.managers.Game:AddScoreBonus(bonus, reason or "Combo")
    end

    self.displayCount = self.comboCount
    self.lastBonus = bonus
    self.comboCount = 0
    self.pendingScore = 0
    self.remaining = 0.0
    self.isFading = true
    self.context.eventBus:Emit("Combo.Closed", {
        reason = reason or "Timeout",
        bonus = bonus
    })
end

function ComboManager:Tick(dt)
    local game = self.context.managers.Game
    if game == nil or not game.isRunning or game.isPaused then
        self:UpdateUI()
        return
    end

    if self.comboCount > 0 then
        self.remaining = self.remaining - (dt or 0.0)
        if self.remaining <= 0.0 then
            self:CloseCombo("Timeout")
        end
    elseif self.isFading then
        self.alpha = self.alpha - ((dt or 0.0) / self.fadeSeconds)
        if self.alpha <= 0.0 then
            self:Reset()
        end
    end

    if self.shakeTime > 0.0 then
        self.shakeTime = math.max(0.0, self.shakeTime - (dt or 0.0))
    end

    self:UpdateUI()
end

function ComboManager:GetShakeOffset()
    if self.shakeTime <= 0.0 then
        return 0.0
    end

    local normalized = self.shakeTime / self.shakeSeconds
    local wave = math.sin(normalized * 54.0)
    return wave * self.shakeMagnitude * normalized
end

function ComboManager:GetSnapshot()
    return {
        isVisible = self.isVisible,
        isFading = self.isFading,
        count = self.displayCount,
        bonus = self.lastBonus,
        alpha = Clamp01(self.alpha),
        remaining = self.remaining,
        shakeOffset = self:GetShakeOffset()
    }
end

function ComboManager:UpdateUI()
    local ui = self.context.managers.UI
    if ui and ui.SetCombo then
        ui:SetCombo(self:GetSnapshot())
    end
end

return ComboManager
