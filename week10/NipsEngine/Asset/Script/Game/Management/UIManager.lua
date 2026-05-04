local UIManager = {}
UIManager.__index = UIManager

function UIManager.new(context)
    return setmetatable({
        context = context,
        builtScreens = {},
        currentScreen = nil,
        visibleOverlays = {},
        titlePanel = "Menu",
        pausePanel = "Menu",
        uiCache = {},
        titleLogoAnimation = nil,
        loadingTipIndex = 0,
        resultNameFocusFrames = 0,
        resultNameBuffer = "",
        resultNameEditing = false,
        displayedHealth = nil,
        healthLerp = nil,
        scoreBonus = {
            amount = 0,
            elapsed = 0.0,
            duration = 1.4,
            fadeSeconds = 0.35,
            visible = false
        }
    }, UIManager)
end

function UIManager:SetCachedText(elementId, text)
    text = tostring(text or "")
    local key = "text:" .. elementId
    if self.uiCache[key] == text then
        return
    end
    if Engine.API.UI.SetText(elementId, text) then
        self.uiCache[key] = text
    end
end

function UIManager:SetCachedVisible(elementId, isVisible)
    local value = isVisible == true
    local key = "visible:" .. elementId
    if self.uiCache[key] == value then
        return
    end
    if Engine.API.UI.SetVisible(elementId, value) then
        self.uiCache[key] = value
    end
end

function UIManager:SetCachedValue(elementId, value)
    value = tostring(value or "")
    local key = "value:" .. elementId
    if self.uiCache[key] == value then
        return
    end
    if Engine.API.UI.SetValue(elementId, value) then
        self.uiCache[key] = value
    end
end

function UIManager:SetCachedProgress(elementId, value)
    local numericValue = tonumber(value) or 0.0
    local keyValue = string.format("%.4f", numericValue)
    local key = "progress:" .. elementId
    if self.uiCache[key] == keyValue then
        return
    end
    if Engine.API.UI.SetProgress(elementId, numericValue) then
        self.uiCache[key] = keyValue
    end
end

function UIManager:SetCachedAlpha(elementId, value)
    local numericValue = tonumber(value) or 1.0
    local keyValue = string.format("%.4f", numericValue)
    local key = "alpha:" .. elementId
    if self.uiCache[key] == keyValue then
        return
    end
    if Engine.API.UI.SetAlpha(elementId, numericValue) then
        self.uiCache[key] = keyValue
    end
end

function UIManager:SetCachedStyle(elementId, name, value)
    value = tostring(value or "")
    local key = "style:" .. elementId .. ":" .. name
    if self.uiCache[key] == value then
        return
    end
    if Engine.API.UI.SetElementStyle(elementId, name, value) then
        self.uiCache[key] = value
    end
end

function UIManager:Tick(dt)
    self:TickTitleLogoAnimation(dt)
    self:TickHealthLerp(dt)
    self:TickScoreBonus(dt)
    self:TickResultNameInputFocus()
    self:TickResultNameInputText()

    local events = Engine.API.UI.PollActionEvents()
    if events == nil then
        return
    end

    for _, eventName in ipairs(events) do
        local sound = self.context.managers.Sound
        if sound ~= nil then
            sound:PlayUIAction(eventName)
        end

        self.context.eventBus:Emit("UI.Action", {
            name = eventName
        })
    end
end

function UIManager:BeginPlay()
    self.context.eventBus:Subscribe("Game.Started", self, function(snapshot)
        self:ResetHUDAnimation(snapshot)
    end)

    self.context.eventBus:Subscribe("Score.Bonus", self, function(payload)
        payload = payload or {}
        self:ShowScoreBonus(payload.amount)
    end)
end

function UIManager:EndPlay()
    self.context.eventBus:ClearOwner(self)
end

local TITLE_LOGO_LAYERS = {
    { mark = "Title.Cyber.Mark1", logo = "Title.Cyber.Logo1", finalLeft = 170, startLeft = 70, color = { 0, 255, 255 } },
    { mark = "Title.Cyber.Mark2", logo = "Title.Cyber.Logo2", finalLeft = 160, startLeft = 60, color = { 0, 183, 255 } },
    { mark = "Title.Cyber.Mark3", logo = "Title.Cyber.Logo3", finalLeft = 165, startLeft = 65, color = { 255, 237, 72 } },
    { mark = "Title.Psycho.Mark1", logo = "Title.Psycho.Logo1", finalLeft = 330, startLeft = 430, color = { 0, 255, 255 } },
    { mark = "Title.Psycho.Mark2", logo = "Title.Psycho.Logo2", finalLeft = 320, startLeft = 420, color = { 0, 183, 255 } },
    { mark = "Title.Psycho.Mark3", logo = "Title.Psycho.Logo3", finalLeft = 325, startLeft = 425, color = { 255, 237, 72 } },
}

local function Lerp(a, b, t)
    return a + (b - a) * t
end

local function EaseOutCubic(t)
    t = tonumber(t) or 0.0
    if t < 0.0 then
        t = 0.0
    elseif t > 1.0 then
        t = 1.0
    end

    local inverse = 1.0 - t
    return 1.0 - inverse * inverse * inverse
end

local function RgbString(r, g, b)
    return string.format("rgb(%d, %d, %d)", math.floor(r + 0.5), math.floor(g + 0.5), math.floor(b + 0.5))
end

function UIManager:ApplyTitleLogoAnimation(t)
    local eased = EaseOutCubic(t)

    for _, layer in ipairs(TITLE_LOGO_LAYERS) do
        local left = Lerp(layer.startLeft, layer.finalLeft, eased)
        local r = Lerp(255, layer.color[1], eased)
        local g = Lerp(255, layer.color[2], eased)
        local b = Lerp(255, layer.color[3], eased)

        self:SetCachedStyle(layer.mark, "left", string.format("%.1fpx", left))
        self:SetCachedAlpha(layer.logo, eased)
        self:SetCachedStyle(layer.logo, "color", RgbString(r, g, b))
    end
end

function UIManager:StartTitleLogoAnimation()
    self.titleLogoAnimation = {
        elapsed = 0.0,
        duration = 1.0
    }

    self:ApplyTitleLogoAnimation(0.0)
end

function UIManager:TickTitleLogoAnimation(dt)
    local animation = self.titleLogoAnimation
    if animation == nil then
        return
    end

    local realDt = Engine.API.World.GetUnscaledDeltaTime()
    if realDt == nil or realDt <= 0.0 then
        realDt = dt or 0.0
    end

    animation.elapsed = animation.elapsed + realDt
    local t = animation.elapsed / animation.duration
    if t >= 1.0 then
        self:ApplyTitleLogoAnimation(1.0)
        self.titleLogoAnimation = nil
        return
    end

    self:ApplyTitleLogoAnimation(t)
end

function UIManager:Show(screenId)
    self:BuildScreenIfNeeded(screenId)

    if self.currentScreen ~= nil and self.currentScreen ~= screenId then
        Engine.API.UI.HideDocument(self.currentScreen)
    end

    self.currentScreen = screenId
    Engine.API.UI.ShowDocument(screenId)
end

function UIManager:Hide(screenId)
    if screenId == nil then
        return
    end

    Engine.API.UI.HideDocument(screenId)
    self.visibleOverlays[screenId] = nil

    if self.currentScreen == screenId then
        self.currentScreen = nil
    end
end

function UIManager:ShowOverlay(screenId)
    self:BuildScreenIfNeeded(screenId)
    self.visibleOverlays[screenId] = true
    Engine.API.UI.ShowDocument(screenId)
end

function UIManager:HideOverlay(screenId)
    self.visibleOverlays[screenId] = nil
    Engine.API.UI.HideDocument(screenId)
end

function UIManager:HideAllOverlays()
    for screenId, _ in pairs(self.visibleOverlays) do
        Engine.API.UI.HideDocument(screenId)
    end
    self.visibleOverlays = {}
end

function UIManager:BuildScreenIfNeeded(screenId)
    if self.builtScreens[screenId] then
        return
    end

    local loaded = false
    if screenId == "Boot" then
        loaded = self:BuildBootScreen()
    elseif screenId == "Intro" then
        loaded = self:BuildIntroScreen()
    elseif screenId == "Title" then
        loaded = self:BuildTitleScreen()
    elseif screenId == "HUD" then
        loaded = self:BuildHUDScreen()
    elseif screenId == "Loading" then
        loaded = self:BuildLoadingScreen()
    elseif screenId == "Pause" then
        loaded = self:BuildPauseScreen()
    elseif screenId == "Result" then
        loaded = self:BuildResultScreen()
    else
        Engine.API.Debug.Warn("[UIManager] Unknown screen: " .. tostring(screenId))
        return
    end

    self.builtScreens[screenId] = loaded
end

function UIManager:BuildBootScreen()
    return self:LoadScreenDocument("Boot", "Asset/UI/Game/Boot.rml")
end

function UIManager:BuildIntroScreen()
    return self:LoadScreenDocument("Intro", "Asset/UI/Game/Intro.rml")
end

function UIManager:BuildTitleScreen()
    return self:LoadScreenDocument("Title", "Asset/UI/Game/Title.rml")
end

function UIManager:BuildHUDScreen()
    local loaded = self:LoadScreenDocument("HUD", "Asset/UI/Game/HUD.rml")
    if loaded then
        Engine.API.UI.SetVisible("HUD.ComboWrap", false)
        Engine.API.UI.SetVisible("HUD.BonusText", false)
    end
    return loaded
end

function UIManager:BuildLoadingScreen()
    return self:LoadScreenDocument("Loading", "Asset/UI/Game/Loading.rml")
end

function UIManager:BuildPauseScreen()
    return self:LoadScreenDocument("Pause", "Asset/UI/Game/Pause.rml")
end

function UIManager:BuildResultScreen()
    return self:LoadScreenDocument("Result", "Asset/UI/Game/Result.rml")
end

function UIManager:LoadScreenDocument(screenId, path)
    if not Engine.API.UI.LoadDocument(screenId, path) then
        Engine.API.Debug.Warn("[UIManager] Failed to load RML screen: " .. tostring(screenId) .. " path=" .. tostring(path))
        return false
    end

    Engine.API.UI.HideDocument(screenId)
    return true
end

function UIManager:SetTitlePanel(panelName)
    panelName = panelName or "Menu"
    self.titlePanel = panelName

    local panels = {
        "Menu",
        "ScoreBoard",
        "Settings",
        "Credits"
    }

    for _, name in ipairs(panels) do
        Engine.API.UI.SetVisible("Title." .. name .. "Panel", name == panelName)
    end

    local isModalOpen = panelName ~= "Menu"
    Engine.API.UI.SetVisible("Title.ModalOverlay", isModalOpen)
    Engine.API.UI.SetVisible("Title.ModalClose", isModalOpen)

    if panelName == "ScoreBoard" then
        self:RefreshScoreBoard("Title")
    elseif panelName == "Settings" then
        self:RefreshSettings()
    end
end

function UIManager:SetPausePanel(panelName)
    panelName = panelName or "Menu"
    self.pausePanel = panelName

    Engine.API.UI.SetVisible("Pause.MenuPanel", panelName == "Menu")
    Engine.API.UI.SetVisible("Pause.SettingsPanel", panelName == "Settings")

    if panelName == "Settings" then
        self:RefreshSettings()
    end
end

local function Clamp01(value)
    value = tonumber(value) or 0.0
    if value < 0.0 then
        return 0.0
    elseif value > 1.0 then
        return 1.0
    end
    return value
end

local function FormatPercent(value)
    return tostring(math.floor(Clamp01(value) * 100 + 0.5)) .. "%"
end

function UIManager:ApplyHealthVisual(health, maxHealth)
    maxHealth = tonumber(maxHealth) or self.context.root.PlayerMaxHealth or 100.0
    health = math.max(0.0, tonumber(health) or maxHealth)

    local healthRatio = 0.0
    if maxHealth > 0.0 then
        healthRatio = Clamp01(health / maxHealth)
    end

    self:SetCachedProgress("HUD.Health", healthRatio)
    self:SetCachedText("HUD.HealthText", tostring(math.floor(health + 0.5)) .. "/" .. tostring(math.floor(maxHealth + 0.5)))
end

function UIManager:ResetHUDAnimation(snapshot)
    snapshot = snapshot or {}
    local maxHealth = tonumber(snapshot.playerMaxHealth) or self.context.root.PlayerMaxHealth or 100.0
    local health = tonumber(snapshot.playerHealth) or maxHealth
    self.displayedHealth = health
    self.healthLerp = nil
    self:ApplyHealthVisual(health, maxHealth)
    self:SetCachedVisible("HUD.BonusText", false)
    self:SetCachedAlpha("HUD.BonusText", 0.0)
end

function UIManager:SetHealthTarget(health, maxHealth)
    maxHealth = tonumber(maxHealth) or self.context.root.PlayerMaxHealth or 100.0
    health = math.max(0.0, tonumber(health) or maxHealth)

    if self.displayedHealth == nil then
        self.displayedHealth = health
        self.healthLerp = nil
        self:ApplyHealthVisual(health, maxHealth)
        return
    end

    local currentTarget = self.healthLerp and self.healthLerp.target or self.displayedHealth
    if math.abs(currentTarget - health) <= 0.001 then
        return
    end

    self.healthLerp = {
        start = self.displayedHealth,
        target = health,
        maxHealth = maxHealth,
        elapsed = 0.0,
        duration = 1.0
    }
end

function UIManager:TickHealthLerp(dt)
    if self.healthLerp == nil then
        return
    end

    local lerpState = self.healthLerp
    lerpState.elapsed = lerpState.elapsed + (dt or 0.0)
    local t = Clamp01(lerpState.elapsed / lerpState.duration)
    self.displayedHealth = Lerp(lerpState.start, lerpState.target, t)
    self:ApplyHealthVisual(self.displayedHealth, lerpState.maxHealth)

    if t >= 1.0 then
        self.displayedHealth = lerpState.target
        self.healthLerp = nil
        self:ApplyHealthVisual(self.displayedHealth, lerpState.maxHealth)
    end
end

function UIManager:ShowScoreBonus(amount)
    amount = math.floor(tonumber(amount) or 0)
    if amount <= 0 then
        return
    end

    self.scoreBonus.amount = amount
    self.scoreBonus.elapsed = 0.0
    self.scoreBonus.visible = true
    self:SetCachedText("HUD.BonusText", "Bonus + " .. tostring(amount))
    self:SetCachedVisible("HUD.BonusText", true)
    self:SetCachedAlpha("HUD.BonusText", 1.0)
end

function UIManager:TickScoreBonus(dt)
    local bonus = self.scoreBonus
    if bonus == nil or bonus.visible ~= true then
        return
    end

    bonus.elapsed = bonus.elapsed + (dt or 0.0)
    local fadeStart = math.max(0.0, bonus.duration - bonus.fadeSeconds)
    local alpha = 1.0
    if bonus.elapsed >= fadeStart then
        alpha = 1.0 - Clamp01((bonus.elapsed - fadeStart) / bonus.fadeSeconds)
    end

    self:SetCachedAlpha("HUD.BonusText", alpha)
    if bonus.elapsed >= bonus.duration then
        bonus.visible = false
        self:SetCachedVisible("HUD.BonusText", false)
    end
end

local function EscapeRml(value)
    value = tostring(value or "")
    value = value:gsub("&", "&amp;")
    value = value:gsub("<", "&lt;")
    value = value:gsub(">", "&gt;")
    value = value:gsub("\"", "&quot;")
    value = value:gsub("'", "&#39;")
    return value
end

local function PopUtf8LastChar(value)
    value = tostring(value or "")
    local length = #value
    if length <= 0 then
        return ""
    end

    local index = length
    while index > 1 do
        local byte = string.byte(value, index)
        if byte == nil or byte < 128 or byte >= 192 then
            break
        end
        index = index - 1
    end

    return string.sub(value, 1, index - 1)
end

local RESULT_NAME_MAX_CHARS = 12

local function LimitUtf8Chars(value, maxChars)
    value = tostring(value or "")
    maxChars = tonumber(maxChars) or 0
    if maxChars <= 0 then
        return ""
    end

    local index = 1
    local count = 0
    while index <= #value and count < maxChars do
        local byte = string.byte(value, index)
        if byte == nil then
            break
        elseif byte < 0x80 then
            index = index + 1
        elseif byte < 0xE0 then
            index = index + 2
        elseif byte < 0xF0 then
            index = index + 3
        else
            index = index + 4
        end
        count = count + 1
    end

    return string.sub(value, 1, index - 1)
end

local function BuildScoreRowRml(index, record)
    local rank = string.format("%02d", index)
    local name = EscapeRml(record.name or "PLAYER")
    local score = tostring(math.floor(record.score or 0))
    return table.concat({
        "<div class=\"score-row\">",
        "<div class=\"score-rank\">", rank, "</div>",
        "<div class=\"score-name\">", name, "</div>",
        "<div class=\"score-value\">", score, "</div>",
        "</div>"
    }, "")
end

local LOADING_TIPS = {
    "[Tips] 저희 엔진으로 처음 만든 게임이에요.",
    "[Tips] 생각보다 괜찮죠? 저흰 안 괜찮아요.",
    "[Tips] 엔진은 사서 쓰세요. 제발",
    "[Tips] 지수님 호준님 감사합니다.",
    "[Tips] 엄마가 칼 가지고 장난치지 말랬지!"
}

function UIManager:RefreshSettings()
    local sound = self.context.managers.Sound
    if sound == nil then
        return
    end

    local bgmPercent = math.floor(Clamp01(sound.bgmVolume or 1.0) * 100 + 0.5)
    local sfxPercent = math.floor(Clamp01(sound.sfxVolume or 1.0) * 100 + 0.5)

    Engine.API.UI.SetText("Title.Settings.BGMValue", FormatPercent(sound.bgmVolume or 1.0))
    Engine.API.UI.SetText("Title.Settings.SFXValue", FormatPercent(sound.sfxVolume or 1.0))
    Engine.API.UI.SetText("Pause.Settings.BGMValue", FormatPercent(sound.bgmVolume or 1.0))
    Engine.API.UI.SetText("Pause.Settings.SFXValue", FormatPercent(sound.sfxVolume or 1.0))
    Engine.API.UI.SetValue("Title.Settings.BGMSlider", tostring(bgmPercent))
    Engine.API.UI.SetValue("Title.Settings.SFXSlider", tostring(sfxPercent))
    Engine.API.UI.SetValue("Pause.Settings.BGMSlider", tostring(bgmPercent))
    Engine.API.UI.SetValue("Pause.Settings.SFXSlider", tostring(sfxPercent))
end

function UIManager:ApplyVolumeFromSlider(kind, sliderElementId)
    local sound = self.context.managers.Sound
    if sound == nil then
        return
    end

    local value = tonumber(Engine.API.UI.GetValue(sliderElementId) or "100") or 100
    value = math.max(0, math.min(100, value)) / 100.0

    if kind == "BGM" then
        sound:SetBGMVolume(value)
    elseif kind == "SFX" then
        sound:SetSFXVolume(value)
    end

    self:RefreshSettings()
end

function UIManager:SetLoadingReady(isReady)
    self:SetCachedVisible("Loading.Cycle", isReady ~= true)
    self:SetCachedVisible("Loading.Status", isReady == true)
    self:SetCachedText("Loading.Status", "Press Space to Start")
end

function UIManager:SetLoadingCycleRotation(degrees)
    local value = (tonumber(degrees) or 0.0) % 360.0
    self:SetCachedStyle("Loading.Cycle", "transform", "rotate(" .. tostring(value) .. "deg)")
end

function UIManager:SelectLoadingTip()
    local count = #LOADING_TIPS
    if count <= 0 then
        return
    end

    local index = Engine.API.Random.RandomInt(1, count)
    if count > 1 and index == self.loadingTipIndex then
        index = (index % count) + 1
    end

    self.loadingTipIndex = index
    self:SetCachedText("Loading.Tip", LOADING_TIPS[index])
end

function UIManager:RequestResultNameInputFocus(frames)
    self.resultNameFocusFrames = frames or 1
end

function UIManager:TickResultNameInputFocus()
    if (self.resultNameFocusFrames or 0) <= 0 then
        return
    end

    if Engine.API.UI.IsElementFocused("Result.PlayerNameInput") then
        self.resultNameFocusFrames = 0
        return
    end

    Engine.API.UI.FocusElement("Result.PlayerNameInput", true)
    self.resultNameFocusFrames = self.resultNameFocusFrames - 1
end

function UIManager:TickResultNameInputText()
    local typed = Engine.API.Input.ConsumeTextInput()
    if self.resultNameEditing ~= true then
        return
    end

    local changed = false
    if typed ~= nil and typed ~= "" then
        self.resultNameBuffer = LimitUtf8Chars(tostring(self.resultNameBuffer or "") .. typed, RESULT_NAME_MAX_CHARS)
        changed = true
    end

    if Engine.API.Input.IsUIKeyPressed("Backspace") then
        self.resultNameBuffer = PopUtf8LastChar(self.resultNameBuffer)
        changed = true
    end

    if Engine.API.Input.IsUIKeyPressed("Enter") then
        self.context.eventBus:Emit("UI.Action", {
            name = "SubmitScore"
        })
    end

    if changed then
        Engine.API.UI.SetValue("Result.PlayerNameInput", self.resultNameBuffer)
        Engine.API.UI.SetText("Result.SubmitStatus", "")
        self:RequestResultNameInputFocus(1)
    end
end

function UIManager:EndResultNameInput()
    self.resultNameEditing = false
    self.resultNameFocusFrames = 0
end

function UIManager:SetIntroText(text)
    self:SetCachedText("Intro.Text", text or "")
end

function UIManager:SetIntroProgress(pageIndex, pageCount)
    -- Intro intentionally hides progress in the current game-jam UI.
end

function UIManager:SetIntroContinueVisible(isVisible)
    self:SetCachedVisible("Intro.Continue", isVisible == true)
end

function UIManager:SetHUD(snapshot)
    snapshot = snapshot or {}
    self:SetCachedText("HUD.Score", "Score " .. tostring(math.floor(snapshot.score or 0)))

    local limitSeconds = tonumber(snapshot.sessionLimitSeconds or self.context.root.SessionLimitSeconds) or 0.0
    local elapsedSeconds = tonumber(snapshot.survivalTime) or 0.0
    local totalSeconds = elapsedSeconds
    if limitSeconds > 0.0 then
        totalSeconds = math.max(0.0, limitSeconds - elapsedSeconds)
    end

    totalSeconds = math.ceil(totalSeconds)
    local minutes = math.floor(totalSeconds / 60)
    local seconds = totalSeconds % 60
    self:SetCachedText("HUD.Time", string.format("%02d:%02d", minutes, seconds))

    local maxHealth = snapshot.playerMaxHealth or self.context.root.PlayerMaxHealth or 100.0
    local health = snapshot.playerHealth or maxHealth
    self:SetHealthTarget(health, maxHealth)
end

function UIManager:SetCombo(snapshot)
    snapshot = snapshot or {}
    local isVisible = snapshot.isVisible == true or snapshot.isFading == true
    self:SetCachedVisible("HUD.ComboWrap", isVisible)
    if not isVisible then
        return
    end

    local count = math.max(0, math.floor(snapshot.count or 0))
    self:SetCachedText("HUD.ComboCount", tostring(count) .. " COMBO")
    self:SetCachedAlpha("HUD.ComboWrap", snapshot.alpha or 1.0)
    self:SetCachedStyle("HUD.ComboWrap", "right", tostring(44 + (snapshot.shakeOffset or 0.0)) .. "px")
    self:SetCachedStyle("HUD.ComboWrap", "top", tostring(94 + (snapshot.offsetY or 0.0)) .. "px")
end

function UIManager:SetResult(snapshot)
    snapshot = snapshot or {}

    local reason = snapshot.finishReason or ""
    local isClear = snapshot.isClear == true
        or reason == "Clear"
        or reason == "Victory"
        or reason == "Win"
        or reason == "Completed"
        or reason == "TimeUp"
    local outcome = isClear and "Victory" or "Defeat"
    self:SetCachedText("Result.Outcome", outcome)
    Engine.API.UI.SetText("Result.Score", "Score: " .. tostring(math.floor(snapshot.score or 0)))
    Engine.API.UI.SetText("Result.FinalScore", tostring(math.floor(snapshot.score or 0)))
    Engine.API.UI.SetText("Result.PlayerName", "PLAYER")
    Engine.API.Input.ConsumeTextInput()
    self.resultNameBuffer = ""
    self.resultNameEditing = true
    Engine.API.UI.SetValue("Result.PlayerNameInput", "")
    Engine.API.UI.SetText("Result.SubmitStatus", "")
    Engine.API.UI.SetVisible("Result.EntryForm", true)
    Engine.API.UI.SetVisible("Result.EntrySummary", false)
    self:RequestResultNameInputFocus(2)
    self:RefreshScoreBoard("Result")
end

function UIManager:RefreshScoreBoard(prefix, limit)
    local data = self.context.managers.Data
    local records = data and data:GetScoreRecords(limit) or {}

    local rows = {}
    for index, record in ipairs(records) do
        rows[#rows + 1] = BuildScoreRowRml(index, record)
    end

    if #rows == 0 then
        rows[1] = "<div class=\"score-empty\">No Records</div>"
    end

    Engine.API.UI.SetText(prefix .. ".ScoreRows", table.concat(rows, ""))
end

function UIManager:SubmitResultScore(snapshot)
    snapshot = snapshot or {}
    local data = self.context.managers.Data
    if data == nil then
        Engine.API.UI.SetText("Result.SubmitStatus", "Score data is not ready.")
        return
    end

    local userName = self.resultNameBuffer
    if userName == nil or userName == "" then
        userName = Engine.API.UI.GetValue("Result.PlayerNameInput")
    end

    local ok, result, record = data:RegisterScore(userName, snapshot.score or 0)
    if ok then
        local savedName = record and record.name or userName or "PLAYER"
        Engine.API.UI.SetText("Result.PlayerName", tostring(savedName))
        Engine.API.UI.SetText("Result.FinalScore", tostring(math.floor(snapshot.score or 0)))
        Engine.API.UI.SetVisible("Result.EntryForm", false)
        Engine.API.UI.SetVisible("Result.EntrySummary", true)

        if result == "Updated" then
            Engine.API.UI.SetText("Result.SubmitStatus", "Score updated.")
        elseif result == "Kept" then
            Engine.API.UI.SetText("Result.SubmitStatus", "Existing score is higher.")
        else
            Engine.API.UI.SetText("Result.SubmitStatus", "Saved.")
        end
        self.resultNameEditing = false
        self.resultNameFocusFrames = 0
        self:RefreshScoreBoard("Result")
    elseif result == "InvalidName" then
        Engine.API.UI.SetText("Result.SubmitStatus", "Enter a player name.")
        self.resultNameEditing = true
        self:RequestResultNameInputFocus(2)
    else
        Engine.API.UI.SetText("Result.SubmitStatus", "Save failed.")
        self.resultNameEditing = true
        self:RequestResultNameInputFocus(2)
    end
end

function UIManager:RenewResultName()
    Engine.API.Input.ConsumeTextInput()
    self.resultNameBuffer = ""
    self.resultNameEditing = true
    Engine.API.UI.SetValue("Result.PlayerNameInput", "")
    Engine.API.UI.SetText("Result.SubmitStatus", "")
    Engine.API.UI.SetVisible("Result.EntryForm", true)
    Engine.API.UI.SetVisible("Result.EntrySummary", false)
    self:RequestResultNameInputFocus(2)
end

return UIManager
