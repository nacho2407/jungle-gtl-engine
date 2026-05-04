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
        uiCache = {}
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
    local events = Engine.API.UI.PollActionEvents()
    if events == nil then
        return
    end

    for _, eventName in ipairs(events) do
        self.context.eventBus:Emit("UI.Action", {
            name = eventName
        })
    end
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
    return self:LoadScreenDocument("HUD", "Asset/UI/Game/HUD.rml")
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
        self:RefreshScoreBoard("Title", 10)
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
    Engine.API.UI.SetText("Loading.Status", isReady and "Press Space to Start" or "Preparing Game Scene...")
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

    local totalSeconds = math.floor(snapshot.survivalTime or 0.0)
    local minutes = math.floor(totalSeconds / 60)
    local seconds = totalSeconds % 60
    self:SetCachedText("HUD.Time", string.format("%02d:%02d", minutes, seconds))

    local maxHealth = self.context.root.PlayerMaxHealth or 100.0
    local health = snapshot.playerHealth or maxHealth
    local healthRatio = 0.0
    if maxHealth > 0.0 then
        healthRatio = Clamp01(health / maxHealth)
    end

    self:SetCachedProgress("HUD.Health", healthRatio)
    self:SetCachedText("HUD.HealthText", tostring(math.floor(health)) .. "/" .. tostring(math.floor(maxHealth)))
end

function UIManager:SetCombo(snapshot)
    snapshot = snapshot or {}
    local isVisible = snapshot.isVisible == true or snapshot.isFading == true
    self:SetCachedVisible("HUD.ComboWrap", isVisible)
    if not isVisible then
        return
    end

    local count = math.max(0, math.floor(snapshot.count or 0))
    local bonus = math.max(0, math.floor(snapshot.bonus or 0))
    self:SetCachedText("HUD.ComboCount", tostring(count) .. " COMBO")
    self:SetCachedText("HUD.ComboBonus", "Bonus +" .. tostring(bonus))
    self:SetCachedAlpha("HUD.ComboWrap", snapshot.alpha or 1.0)
    self:SetCachedStyle("HUD.ComboWrap", "right", tostring(44 + (snapshot.shakeOffset or 0.0)) .. "px")
end

function UIManager:SetResult(snapshot)
    snapshot = snapshot or {}

    local data = self.context.managers.Data
    local bestScore = data and data:GetBestScore() or 0

    Engine.API.UI.SetText("Result.Score", "Score: " .. tostring(math.floor(snapshot.score or 0)))
    Engine.API.UI.SetText("Result.Best", "Best: " .. tostring(math.floor(bestScore)))
    Engine.API.UI.SetValue("Result.PlayerNameInput", "PLAYER")
    Engine.API.UI.SetText("Result.SubmitStatus", "")
    self:RefreshScoreBoard("Result", 10)
end

function UIManager:RefreshScoreBoard(prefix, limit)
    local data = self.context.managers.Data
    local records = data and data:GetScoreRecords(limit or 10) or {}
    local bestScore = data and data:GetBestScore() or 0

    if prefix == "Title" then
        Engine.API.UI.SetText("Title.BestScore", "Best Score: " .. tostring(math.floor(bestScore)))
    elseif prefix == "Result" then
        Engine.API.UI.SetText("Result.Best", "Best: " .. tostring(math.floor(bestScore)))
    end

    for index = 1, (limit or 10) do
        local elementId = prefix .. ".ScoreRow." .. tostring(index)
        local record = records[index]
        if record ~= nil then
            Engine.API.UI.SetText(elementId, string.format("%02d  %s  %d", index, tostring(record.name or "PLAYER"), math.floor(record.score or 0)))
        elseif index == 1 then
            Engine.API.UI.SetText(elementId, "--  No Records  0")
        else
            Engine.API.UI.SetText(elementId, "")
        end
    end
end

function UIManager:SubmitResultScore(snapshot)
    snapshot = snapshot or {}
    local data = self.context.managers.Data
    if data == nil then
        Engine.API.UI.SetText("Result.SubmitStatus", "Score data is not ready.")
        return
    end

    local userName = Engine.API.UI.GetValue("Result.PlayerNameInput")
    local ok, result = data:RegisterScore(userName, snapshot.score or 0)
    if ok then
        Engine.API.UI.SetText("Result.SubmitStatus", "Saved.")
        self:RefreshScoreBoard("Result", 10)
    elseif result == "InvalidName" then
        Engine.API.UI.SetText("Result.SubmitStatus", "Enter a player name.")
    else
        Engine.API.UI.SetText("Result.SubmitStatus", "Save failed.")
    end
end

function UIManager:RenewResultName()
    Engine.API.UI.SetValue("Result.PlayerNameInput", "")
    Engine.API.UI.SetText("Result.SubmitStatus", "")
    Engine.API.UI.FocusElement("Result.PlayerNameInput", true)
end

return UIManager
