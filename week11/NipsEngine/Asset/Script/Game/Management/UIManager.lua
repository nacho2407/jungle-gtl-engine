local UIManager = {}
UIManager.__index = UIManager

function UIManager.new(context)
    return setmetatable({
        context = context,
        builtScreens = {},
        currentScreen = nil
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
    self.currentScreen = screenId
    Engine.API.UI.ShowScreen(screenId)
end

function UIManager:BuildScreenIfNeeded(screenId)
    if self.builtScreens[screenId] then
        return
    end

    if screenId == "Boot" then
        self:BuildBootScreen()
    elseif screenId == "Title" then
        self:BuildTitleScreen()
    elseif screenId == "HUD" then
        self:BuildHUDScreen()
    elseif screenId == "Pause" then
        self:BuildPauseScreen()
    elseif screenId == "Result" then
        self:BuildResultScreen()
    else
        Engine.API.Debug.Warn("[UIManager] Unknown screen: " .. tostring(screenId))
        return
    end

    self.builtScreens[screenId] = true
end

function UIManager:BuildBootScreen()
    Engine.API.UI.CreateText("Boot", "Boot.Title", "Loading...", 760, 500, 400, 60)
    Engine.API.UI.SetFontScale("Boot.Title", 1.4)
end

function UIManager:BuildTitleScreen()
    Engine.API.UI.CreateText("Title", "Title.Name", "SSAMURAI", 720, 260, 520, 80)
    Engine.API.UI.SetFontScale("Title.Name", 2.0)
    Engine.API.UI.CreateButton("Title", "Title.Start", "Start", "StartGame", 810, 430, 280, 64)
    Engine.API.UI.CreateButton("Title", "Title.Quit", "Quit", "QuitGame", 810, 510, 280, 64)
end

function UIManager:BuildHUDScreen()
    Engine.API.UI.CreateText("HUD", "HUD.Score", "Score: 0", 32, 28, 260, 36)
    Engine.API.UI.CreateText("HUD", "HUD.Time", "Time: 0.0", 32, 68, 260, 36)
    Engine.API.UI.CreateProgressBar("HUD", "HUD.Health", 1.0, 32, 112, 260, 24)
end

function UIManager:BuildPauseScreen()
    Engine.API.UI.CreatePanel("Pause", "Pause.Dim", 0, 0, 1920, 1080)
    Engine.API.UI.SetBackgroundColor("Pause.Dim", 0.0, 0.0, 0.0, 0.55)
    Engine.API.UI.CreateText("Pause", "Pause.Title", "Paused", 820, 300, 300, 64)
    Engine.API.UI.SetFontScale("Pause.Title", 1.6)
    Engine.API.UI.CreateButton("Pause", "Pause.Resume", "Resume", "ResumeGame", 820, 420, 280, 60)
    Engine.API.UI.CreateButton("Pause", "Pause.Restart", "Restart", "RestartGame", 820, 496, 280, 60)
    Engine.API.UI.CreateButton("Pause", "Pause.TitleButton", "Title", "BackToTitle", 820, 572, 280, 60)
end

function UIManager:BuildResultScreen()
    Engine.API.UI.CreateText("Result", "Result.Title", "Result", 820, 280, 320, 64)
    Engine.API.UI.SetFontScale("Result.Title", 1.6)
    Engine.API.UI.CreateText("Result", "Result.Score", "Score: 0", 780, 380, 420, 44)
    Engine.API.UI.CreateText("Result", "Result.Best", "Best: 0", 780, 430, 420, 44)
    Engine.API.UI.CreateButton("Result", "Result.Restart", "Restart", "RestartGame", 820, 530, 280, 60)
    Engine.API.UI.CreateButton("Result", "Result.TitleButton", "Title", "BackToTitle", 820, 606, 280, 60)
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
