local UIManager = {}
local widgets = {}

local onStartGame = nil

function UIManager.SetStartGameCallback(callback)
    onStartGame = callback
end

function UIManager.Init()
    local introWidget = UI.CreateWidget("Asset/UI/IntroWidget.rml")
    introWidget:bind_click("start-button", function()
        UIManager.Hide("intro")

        if onStartGame ~= nil then
            onStartGame()
        end
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
    UIManager.Register("gameOver", UI.CreateWidget("Asset/UI/GameOverWidget.rml"))
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

return UIManager
