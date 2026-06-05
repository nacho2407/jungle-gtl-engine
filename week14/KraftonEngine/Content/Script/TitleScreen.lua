local widget = nil
local quitWidget = nil
local statWidget = nil
local statVisible = false
local quitVisible = false
local show_quit_panel = nil

local function show_stat_panel(visible)
    statVisible = visible
    if visible then
        show_quit_panel(false)
        if statWidget ~= nil and not statWidget:IsInViewport() then
            statWidget:AddToViewportZ(150)
        end
    elseif statWidget ~= nil and statWidget:IsInViewport() then
        statWidget:RemoveFromParent()
    end
end

show_quit_panel = function(visible)
    quitVisible = visible
    if visible then
        statVisible = false
        if statWidget ~= nil and statWidget:IsInViewport() then
            statWidget:RemoveFromParent()
        end
        if quitWidget ~= nil and not quitWidget:IsInViewport() then
            quitWidget:AddToViewportZ(200)
        end
    elseif quitWidget ~= nil and quitWidget:IsInViewport() then
        quitWidget:RemoveFromParent()
    end
end

local function bind_quit_dialog_clicks()
    quitWidget:bind_click("cancel-quit-button", function()
        show_quit_panel(false)
    end)

    quitWidget:bind_click("confirm-quit-button", function()
        Engine.Exit()
    end)
end

local function bind_clicks()
    widget:bind_click("stat-button", function()
        show_stat_panel(not statVisible)
    end)

    widget:bind_click("quit-button", function()
        show_quit_panel(true)
    end)

    widget:bind_click("new-game-button", function()
        print("[Title] New Game selected")
    end)

    widget:bind_click("continue-button", function()
        print("[Title] Continue selected")
    end)

    widget:bind_click("settings-button", function()
        print("[Title] Settings selected")
    end)

    widget:bind_click("credits-button", function()
        print("[Title] Credits selected")
    end)
end

function BeginPlay()
    widget = UI.CreateWidget("Content/UI/Title/Title.rml")
    widget:SetWantsMouse(true)
    bind_clicks()
    widget:AddToViewportZ(100)

    quitWidget = UI.CreateWidget("Content/UI/Title/TitleQuitDialog.rml")
    quitWidget:SetWantsMouse(true)
    bind_quit_dialog_clicks()

    statWidget = UI.CreateWidget("Content/UI/Title/TitleScoreBoard.rml")

    show_stat_panel(false)
    show_quit_panel(false)

    Engine.SetOnEscape(function()
        if quitVisible then
            show_quit_panel(false)
        elseif statVisible then
            show_stat_panel(false)
        else
            show_quit_panel(true)
        end
    end)
end

function EndPlay()
    if quitWidget ~= nil and quitWidget:IsInViewport() then
        quitWidget:RemoveFromParent()
    end
    quitWidget = nil

    if statWidget ~= nil and statWidget:IsInViewport() then
        statWidget:RemoveFromParent()
    end
    statWidget = nil

    if widget ~= nil and widget:IsInViewport() then
        widget:RemoveFromParent()
    end
    widget = nil
end

function Tick(dt)
end
