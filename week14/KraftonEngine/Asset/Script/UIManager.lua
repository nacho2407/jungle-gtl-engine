local UIManager = {}
local widgets = {}

local onStartGame = nil
local fade = {
    active = false,
    elapsed = 0,
    duration = 0,
    from = 0,
    to = 0,
    onComplete = nil,
    hideWhenDone = false
}

local function Clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function SetFadeOpacity(opacity)
    local fadeWidget = widgets["fade"]
    if fadeWidget == nil then
        return false
    end

    if not fadeWidget:IsInViewport() then
        fadeWidget:AddToViewportZ(10000)
    end

    opacity = Clamp(opacity, 0, 1)
    return fadeWidget:set_property("fade-screen", "opacity", tostring(opacity))
end

function UIManager.SetStartGameCallback(callback)
    onStartGame = callback
end

function UIManager.Init()
    local introWidget = UI.CreateWidget("Asset/UI/IntroWidget.rml")
    introWidget:bind_click("start-button", function()
        UIManager.FadeOut(0.5, function()
            UIManager.Hide("intro")

            if onStartGame ~= nil then
                onStartGame()
            end

            UIManager.FadeIn(0.5)
        end)
    end)
    introWidget:bind_click("exit-button", function()
        UIManager.Hide("intro")
    end)

    local contributorWidget = UI.CreateWidget("Asset/UI/ContributorWidget.rml")
    contributorWidget:bind_click("contributor-close-button", function()
        UIManager.Hide("contributor")
    end)

    UIManager.Register("intro", introWidget)
    UIManager.Register("whiteBox", UI.CreateWidget("Asset/UI/PIEWhiteBox.rml"))
    UIManager.Register("contributor", contributorWidget)
    UIManager.Register("gameOverlay", UI.CreateWidget("Asset/UI/GameOverlayWidget.rml"))
    UIManager.Register("gasWidget", UI.CreateWidget("Asset/UI/GasWidget.rml"))
    UIManager.Register("gameOver", UI.CreateWidget("Asset/UI/GameOverWidget.rml"))
    UIManager.Register("fade", UI.CreateWidget("Asset/UI/FadeWidget.rml"))
end

function UIManager.Register(key, widget)
    widgets[key] = widget
end

function UIManager.Show(key)
    local widget = widgets[key]
    if widget ~= nil then
        widget:show()
        return true
    end
    return false
end

function UIManager.Hide(key)
    local widget = widgets[key]
    if widget ~= nil then
        widget:hide()
        return true
    end
    return false
end

function UIManager.Toggle(key)
    local widget = widgets[key]
    if widget == nil then
        return false
    end

    if widget:IsInViewport() then
        widget:hide()
    else
        widget:show()
    end
    return true
end

function UIManager.FadeTo(fromOpacity, toOpacity, duration, onComplete, hideWhenDone)
    fade.active = true
    fade.elapsed = 0
    fade.duration = duration or 1
    fade.from = Clamp(fromOpacity or 0, 0, 1)
    fade.to = Clamp(toOpacity or 0, 0, 1)
    fade.onComplete = onComplete
    fade.hideWhenDone = hideWhenDone == true

    if fade.duration <= 0 then
        SetFadeOpacity(fade.to)
        fade.active = false
        if fade.hideWhenDone then
            UIManager.Hide("fade")
        end
        if fade.onComplete ~= nil then
            fade.onComplete()
        end
        return
    end

    SetFadeOpacity(fade.from)
end

function UIManager.FadeOut(duration, onComplete)
    UIManager.FadeTo(0, 1, duration, onComplete, false)
end

function UIManager.FadeIn(duration, onComplete)
    UIManager.FadeTo(1, 0, duration, onComplete, true)
end

function UIManager.IsFading()
    return fade.active
end

function UIManager.Tick(dt)
    if not fade.active then
        return
    end

    fade.elapsed = fade.elapsed + dt
    local alpha = Clamp(fade.elapsed / fade.duration, 0, 1)
    local opacity = fade.from + (fade.to - fade.from) * alpha
    SetFadeOpacity(opacity)

    if alpha >= 1 then
        local onComplete = fade.onComplete
        local hideWhenDone = fade.hideWhenDone

        fade.active = false
        fade.onComplete = nil
        fade.hideWhenDone = false

        if hideWhenDone then
            UIManager.Hide("fade")
        end

        if onComplete ~= nil then
            onComplete()
        end
    end
end

function UIManager.GetWidget(key)
    return widgets[key]
end

-- ============================================================
-- HUD (GameOverlayWidget) 갱신
--
-- 매 프레임 폴링: time-value(매치 카운트다운), combo-value(페이즈 카운트다운).
-- 페이즈 전환 / 클리어 시 OnPhaseChanged 콜백에서 UpdateHUD 호출 → 즉시 라벨 갱신.
-- 게임오버레이가 viewport 에 없으면 no-op (intro 화면 동안).
-- ============================================================

local PHASE_OBJECTIVE = {
    [ECarGamePhase.None]         = "EXPLORE THE CITY",
    [ECarGamePhase.CarWash]      = "WASH THE CAR",
    [ECarGamePhase.CarGas]       = "FILL UP GAS",
    [ECarGamePhase.EscapePolice] = "ESCAPE THE POLICE",
    [ECarGamePhase.DodgeMeteor]  = "DODGE METEORS",
    [ECarGamePhase.Finished]     = "MATCH COMPLETE",
}

local PHASE_NAME = {
    [ECarGamePhase.CarWash]      = "CAR WASH",
    [ECarGamePhase.CarGas]       = "GAS FILL",
    [ECarGamePhase.EscapePolice] = "ESCAPE",
    [ECarGamePhase.DodgeMeteor]  = "METEOR DODGE",
}

local function FormatTime(seconds)
    if seconds == nil or seconds < 0 then seconds = 0 end
    local m = math.floor(seconds / 60)
    local s = math.floor(seconds % 60)
    return string.format("%02d:%02d", m, s)
end

local function PopCount(mask)
    local n = 0
    while mask > 0 do
        if mask % 2 == 1 then n = n + 1 end
        mask = math.floor(mask / 2)
    end
    return n
end

local function GetObjectiveText(phase, lastEnded, lastResult)
    if phase == ECarGamePhase.Result then
        local name = PHASE_NAME[lastEnded] or "PHASE"
        if lastResult == EPhaseResult.Success then
            return name .. " CLEARED!"
        elseif lastResult == EPhaseResult.Failed then
            return name .. " FAILED"
        end
    end
    return PHASE_OBJECTIVE[phase] or ""
end

function UIManager.UpdateHUD()
    local gs = GetGameState()
    if gs == nil then return end
    local widget = widgets["gameOverlay"]
    if widget == nil or not widget:IsInViewport() then return end

    local phase = gs:GetPhase()

    -- 매치 전체 카운트다운 (항상 표시)
    widget:set_text("time-value", FormatTime(gs:GetRemainingMatchTime()))

    -- 페이즈 카운트다운 — 활성/Result 일 때만, 그 외 "--"
    if phase == ECarGamePhase.None or phase == ECarGamePhase.Finished then
        widget:set_text("combo-value", "--")
    else
        widget:set_text("combo-value", FormatTime(gs:GetRemainingPhaseTime()))
    end

    -- objective + 클리어 카운트 — 매 프레임 호출 비용 미미하므로 같이 갱신
    widget:set_text("objective-value", GetObjectiveText(phase, gs:GetLastEndedPhase(), gs:GetLastPhaseResult()))
    widget:set_text("score-value", PopCount(gs:GetClearedPhasesMask()) .. "/4")
end

return UIManager
