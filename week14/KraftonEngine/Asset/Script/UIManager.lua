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

return UIManager
