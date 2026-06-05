local widget = nil
local statVisible = false
local quitVisible = false

local function set_visible(elementId, visible)
    if widget == nil then
        return
    end

    widget:SetProperty(elementId, "display", visible and "block" or "none")
end

local function show_stat_panel(visible)
    statVisible = visible
    if visible then
        quitVisible = false
        set_visible("quit-panel", false)
    end
    set_visible("stat-panel", visible)
end

local function show_quit_panel(visible)
    quitVisible = visible
    if visible then
        statVisible = false
        set_visible("stat-panel", false)
    end
    set_visible("quit-panel", visible)
end

local function bind_clicks()
    widget:bind_click("stat-button", function()
        show_stat_panel(not statVisible)
    end)

    widget:bind_click("quit-button", function()
        show_quit_panel(true)
    end)

    widget:bind_click("cancel-quit-button", function()
        show_quit_panel(false)
    end)

    widget:bind_click("confirm-quit-button", function()
        Engine.Exit()
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
    if widget ~= nil and widget:IsInViewport() then
        widget:RemoveFromParent()
    end
    widget = nil
end

function Tick(dt)
end
