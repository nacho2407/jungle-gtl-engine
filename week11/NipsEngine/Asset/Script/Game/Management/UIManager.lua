local UIManager = {}
UIManager.__index = UIManager

function UIManager.new(context)
    return setmetatable({
        context = context,
        builtScreens = {},
        currentScreen = nil,
        visibleOverlays = {},
        titlePanel = "Menu"
    }, UIManager)
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

    if panelName == "ScoreBoard" then
        local data = self.context.managers.Data
        local bestScore = data and data:GetBestScore() or 0
        Engine.API.UI.SetText("Title.BestScore", "Best Score: " .. tostring(math.floor(bestScore)))
    elseif panelName == "Settings" then
        self:RefreshSettings()
    end
end

function UIManager:RefreshSettings()
    local sound = self.context.managers.Sound
    if sound == nil then
        return
    end

    local bgmPercent = math.floor((sound.bgmVolume or 1.0) * 100)
    local sfxPercent = math.floor((sound.sfxVolume or 1.0) * 100)
    Engine.API.UI.SetText("Title.Settings.BGMValue", tostring(bgmPercent) .. "%")
    Engine.API.UI.SetText("Title.Settings.SFXValue", tostring(sfxPercent) .. "%")
    Engine.API.UI.SetText("Pause.Settings.BGMValue", tostring(bgmPercent) .. "%")
    Engine.API.UI.SetText("Pause.Settings.SFXValue", tostring(sfxPercent) .. "%")
end

function UIManager:SetLoadingReady(isReady)
    Engine.API.UI.SetText("Loading.Status", isReady and "Press Space to Start" or "Preparing Game Scene...")
end

function UIManager:SetIntroText(text)
    Engine.API.UI.SetText("Intro.Text", text or "")
end

function UIManager:SetIntroProgress(pageIndex, pageCount)
    Engine.API.UI.SetText("Intro.Progress", tostring(pageIndex or 1) .. " / " .. tostring(pageCount or 1))
end

function UIManager:SetIntroContinueVisible(isVisible)
    Engine.API.UI.SetVisible("Intro.Continue", isVisible == true)
end

function UIManager:SetHUD(snapshot)
    snapshot = snapshot or {}
    Engine.API.UI.SetText("HUD.Score", "Score: " .. tostring(math.floor(snapshot.score or 0)))
    Engine.API.UI.SetText("HUD.Time", string.format("Time: %.1f", snapshot.survivalTime or 0.0))

    local maxHealth = self.context.root.PlayerMaxHealth or 100.0
    local health = snapshot.playerHealth or maxHealth
    Engine.API.UI.SetProgress("HUD.Health", health / maxHealth)
end

function UIManager:SetResult(snapshot)
    snapshot = snapshot or {}

    local data = self.context.managers.Data
    local bestScore = data and data:GetBestScore() or 0

    Engine.API.UI.SetText("Result.Score", "Score: " .. tostring(math.floor(snapshot.score or 0)))
    Engine.API.UI.SetText("Result.Best", "Best: " .. tostring(math.floor(bestScore)))
end

return UIManager
