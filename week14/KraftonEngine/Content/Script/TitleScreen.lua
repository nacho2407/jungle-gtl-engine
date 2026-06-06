local HoverDescription = require("HoverDescription")

local widget = nil
local quitWidget = nil
local statWidget = nil
local settingsWidget = nil
local creditsWidget = nil
local statVisible = false
local quitVisible = false
local settingsVisible = false
local creditsVisible = false
local show_stat_panel = nil
local show_quit_panel = nil
local show_settings_panel = nil
local show_credits_panel = nil
local lastViewportWidth = 0
local lastViewportHeight = 0
local settingsState = {
    gamma = 2.40,
    bgm = 80,
    sfx = 80,
    voice = 80,
    mouse = 0.20,
    zoom = "Hold",
    sprint = "Toggle",
}

local settingsDefaults = {
    gamma = 2.40,
    bgm = 80,
    sfx = 80,
    voice = 80,
    mouse = 0.20,
    zoom = "Hold",
    sprint = "Toggle",
}

local settingSliders = {
    gamma = { value = "settings-gamma-value", fill = "settings-gamma-fill", handle = "settings-gamma-handle", min = 1.00, max = 3.00, step = 0.01, format = "%.2f" },
    bgm = { value = "settings-bgm-value", fill = "settings-bgm-fill", handle = "settings-bgm-handle", min = 0, max = 100, step = 0.01, format = "%.2f" },
    sfx = { value = "settings-sfx-value", fill = "settings-sfx-fill", handle = "settings-sfx-handle", min = 0, max = 100, step = 0.01, format = "%.2f" },
    voice = { value = "settings-voice-value", fill = "settings-voice-fill", handle = "settings-voice-handle", min = 0, max = 100, step = 0.01, format = "%.2f" },
    mouse = { value = "settings-mouse-value", fill = "settings-mouse-fill", handle = "settings-mouse-handle", min = 0.01, max = 3.00, step = 0.01, format = "%.2f" },
}

local settingsDefaultDescription = {
    title = "GAME SETTINGS",
    body = "Adjust visual brightness, audio mix, and mouse feel before starting the campaign.",
    note = "Use - and +, drag a slider, or click toggle rows.",
}

local settingsDescriptions = {
    {
        id = "settings-gamma-row",
        title = "GAMMA",
        body = "Adjust screen brightness to keep dark scenes readable without washing out highlights.",
        note = "Range: 1.00 to 3.00.",
    },
    {
        id = "settings-bgm-row",
        title = "BGM VOLUME",
        body = "Control background music volume.",
        note = "This value is prepared for audio mix control.",
    },
    {
        id = "settings-sfx-row",
        title = "SFX VOLUME",
        body = "Control weapon, impact, movement, and interface sound effects.",
        note = "This value is prepared for audio mix control.",
    },
    {
        id = "settings-voice-row",
        title = "VOICE VOLUME",
        body = "Control dialogue and voice playback volume.",
        note = "This value is prepared for story and combat voice lines.",
    },
    {
        id = "settings-mouse-row",
        title = "MOUSE SENSITIVITY",
        body = "Adjust camera turn speed for mouse input.",
        note = "The value is applied to the current engine mouse sensitivity.",
    },
    {
        id = "settings-toggle-zoom-row",
        title = "ZOOM",
        body = "Choose whether zoom input should be held or toggled.",
        note = "GUI only for now.",
    },
    {
        id = "settings-toggle-sprint-row",
        title = "SPRINT",
        body = "Choose whether sprint input should be held or toggled.",
        note = "GUI only for now.",
    },
    {
        id = "settings-back-button",
        title = "BACK",
        body = "Close settings and return to the title screen.",
        note = "Current settings remain applied.",
    },
    {
        id = "settings-restore-button",
        title = "RESTORE DEFAULTS",
        body = "Restore settings to the values captured when this settings panel opened.",
        note = "Gamma and mouse sensitivity are applied immediately.",
    },
}

local settingsSliderWidth = 220.0
local settingsHandleWidth = 16.0
local activeSettingsSlider = nil

local function px(value)
    return string.format("%.2fpx", value)
end

local function clamp(value, minValue, maxValue)
    if value < minValue then
        return minValue
    end
    if value > maxValue then
        return maxValue
    end
    return value
end

local function round_to_step(value, step)
    return math.floor((value / step) + 0.5) * step
end

local function sync_settings_from_engine()
    if Engine ~= nil then
        if Engine.GetGamma ~= nil then
            settingsState.gamma = clamp(Engine.GetGamma(), settingSliders.gamma.min, settingSliders.gamma.max)
            settingsDefaults.gamma = settingsState.gamma
        end
        if Engine.GetMouseSensitivity ~= nil then
            settingsState.mouse = clamp(Engine.GetMouseSensitivity(), settingSliders.mouse.min, settingSliders.mouse.max)
            settingsDefaults.mouse = settingsState.mouse
        end
    end
end

local function apply_setting_slider(name)
    if settingsWidget == nil then
        return
    end

    local config = settingSliders[name]
    if config == nil then
        return
    end

    local value = clamp(settingsState[name], config.min, config.max)
    settingsState[name] = value

    local ratio = 0.0
    if config.max > config.min then
        ratio = (value - config.min) / (config.max - config.min)
    end

    local fillWidth = settingsSliderWidth * ratio
    local handleLeft = clamp(fillWidth - settingsHandleWidth * 0.5, 0.0, settingsSliderWidth - settingsHandleWidth)

    settingsWidget:SetText(config.value, string.format(config.format, value))
    settingsWidget:set_property(config.fill, "width", px(fillWidth))
    settingsWidget:set_property(config.handle, "left", px(handleLeft))

    if name == "gamma" and Engine ~= nil and Engine.SetGamma ~= nil then
        Engine.SetGamma(value)
    elseif name == "mouse" and Engine ~= nil and Engine.SetMouseSensitivity ~= nil then
        Engine.SetMouseSensitivity(value)
    end
end

local function apply_settings_to_ui()
    if settingsWidget == nil then
        return
    end

    apply_setting_slider("gamma")
    apply_setting_slider("bgm")
    apply_setting_slider("sfx")
    apply_setting_slider("voice")
    apply_setting_slider("mouse")
    settingsWidget:SetText("settings-toggle-zoom-value", settingsState.zoom)
    settingsWidget:SetText("settings-toggle-sprint-value", settingsState.sprint)
end

local function adjust_setting(name, direction)
    local config = settingSliders[name]
    if config == nil then
        return
    end

    settingsState[name] = clamp(settingsState[name] + config.step * direction, config.min, config.max)
    settingsState[name] = round_to_step(settingsState[name], config.step)
    apply_setting_slider(name)
end

local function set_setting_from_slider_ratio(name, ratio)
    local config = settingSliders[name]
    if config == nil then
        return
    end

    local clampedRatio = clamp(ratio, 0.0, 1.0)
    local value = config.min + (config.max - config.min) * clampedRatio
    settingsState[name] = clamp(round_to_step(value, config.step), config.min, config.max)
    apply_setting_slider(name)
end

local function update_active_settings_slider(mouseX)
    if activeSettingsSlider == nil then
        return
    end

    local width = activeSettingsSlider.width
    if width <= 0.0 then
        width = settingsSliderWidth
    end

    set_setting_from_slider_ratio(activeSettingsSlider.name, (mouseX - activeSettingsSlider.left) / width)
end

local function begin_settings_slider_drag(name, event)
    local left = event.element_left or 0.0
    local width = event.element_width or settingsSliderWidth
    activeSettingsSlider = {
        name = name,
        left = left,
        width = width,
    }
    update_active_settings_slider(event.mouse_x or Input.GetMouseClientX())
end

local function toggle_setting(name)
    if settingsState[name] == "Hold" then
        settingsState[name] = "Toggle"
    else
        settingsState[name] = "Hold"
    end
    apply_settings_to_ui()
end

local function restore_default_settings()
    settingsState.gamma = settingsDefaults.gamma
    settingsState.bgm = settingsDefaults.bgm
    settingsState.sfx = settingsDefaults.sfx
    settingsState.voice = settingsDefaults.voice
    settingsState.mouse = settingsDefaults.mouse
    settingsState.zoom = settingsDefaults.zoom
    settingsState.sprint = settingsDefaults.sprint
    apply_settings_to_ui()
end

local function start_prologue_scene()
    show_stat_panel(false)
    show_settings_panel(false)
    show_quit_panel(false)

    if Engine.TransitionToScene ~= nil then
        Engine.TransitionToScene("Prologue.Scene")
    else
        print("[Title] Engine.TransitionToScene is not available")
    end
end

local function update_title_background(force)
    if widget == nil then
        return
    end

    local size = Engine.GetViewportSize()
    local viewportWidth = size.Width or 0
    local viewportHeight = size.Height or 0
    if viewportWidth <= 0 or viewportHeight <= 0 then
        return
    end

    if not force and viewportWidth == lastViewportWidth and viewportHeight == lastViewportHeight then
        return
    end

    lastViewportWidth = viewportWidth
    lastViewportHeight = viewportHeight

    local imageAspect = 1920.0 / 1080.0
    local viewportAspect = viewportWidth / viewportHeight
    local backgroundWidth = viewportWidth
    local backgroundHeight = viewportHeight
    local backgroundLeft = 0.0
    local backgroundTop = 0.0

    if viewportAspect > imageAspect then
        backgroundWidth = viewportWidth
        backgroundHeight = viewportWidth / imageAspect
        backgroundTop = (viewportHeight - backgroundHeight) * 0.5
    else
        backgroundHeight = viewportHeight
        backgroundWidth = viewportHeight * imageAspect
        backgroundLeft = (viewportWidth - backgroundWidth) * 0.5
    end

    widget:set_property("title-background", "left", px(backgroundLeft))
    widget:set_property("title-background", "top", px(backgroundTop))
    widget:set_property("title-background", "width", px(backgroundWidth))
    widget:set_property("title-background", "height", px(backgroundHeight))
end

show_stat_panel = function(visible)
    statVisible = visible
    if visible then
        show_quit_panel(false)
        show_settings_panel(false)
        show_credits_panel(false)
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
        settingsVisible = false
        creditsVisible = false
        if statWidget ~= nil and statWidget:IsInViewport() then
            statWidget:RemoveFromParent()
        end
        if settingsWidget ~= nil and settingsWidget:IsInViewport() then
            settingsWidget:RemoveFromParent()
        end
        if creditsWidget ~= nil and creditsWidget:IsInViewport() then
            creditsWidget:RemoveFromParent()
        end
        if quitWidget ~= nil and not quitWidget:IsInViewport() then
            quitWidget:AddToViewportZ(200)
        end
    elseif quitWidget ~= nil and quitWidget:IsInViewport() then
        quitWidget:RemoveFromParent()
    end
end

show_settings_panel = function(visible)
    settingsVisible = visible
    if visible then
        sync_settings_from_engine()
        show_quit_panel(false)
        show_credits_panel(false)
        statVisible = false
        if statWidget ~= nil and statWidget:IsInViewport() then
            statWidget:RemoveFromParent()
        end
        if settingsWidget ~= nil and not settingsWidget:IsInViewport() then
            settingsWidget:AddToViewportZ(150)
        end
        apply_settings_to_ui()
    elseif settingsWidget ~= nil and settingsWidget:IsInViewport() then
        settingsWidget:RemoveFromParent()
    end
end

show_credits_panel = function(visible)
    creditsVisible = visible
    if visible then
        show_quit_panel(false)
        show_settings_panel(false)
        statVisible = false
        if statWidget ~= nil and statWidget:IsInViewport() then
            statWidget:RemoveFromParent()
        end
        if creditsWidget ~= nil and not creditsWidget:IsInViewport() then
            creditsWidget:AddToViewportZ(150)
        end
    elseif creditsWidget ~= nil and creditsWidget:IsInViewport() then
        creditsWidget:RemoveFromParent()
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

local function bind_stat_dialog_clicks()
    statWidget:bind_click("scoreboard-close-button", function()
        show_stat_panel(false)
    end)
end

local function bind_settings_dialog_clicks()
    HoverDescription.Bind(
        settingsWidget,
        { title = "settings-help-title", body = "settings-help-body", note = "settings-help-note" },
        settingsDefaultDescription,
        settingsDescriptions)

    settingsWidget:bind_click("settings-back-button", function()
        show_settings_panel(false)
    end)

    settingsWidget:bind_click("settings-restore-button", function()
        restore_default_settings()
    end)

    settingsWidget:bind_click("settings-gamma-minus", function()
        adjust_setting("gamma", -1)
    end)

    settingsWidget:bind_click("settings-gamma-plus", function()
        adjust_setting("gamma", 1)
    end)

    settingsWidget:bind_click("settings-bgm-minus", function()
        adjust_setting("bgm", -1)
    end)

    settingsWidget:bind_click("settings-bgm-plus", function()
        adjust_setting("bgm", 1)
    end)

    settingsWidget:bind_click("settings-sfx-minus", function()
        adjust_setting("sfx", -1)
    end)

    settingsWidget:bind_click("settings-sfx-plus", function()
        adjust_setting("sfx", 1)
    end)

    settingsWidget:bind_click("settings-voice-minus", function()
        adjust_setting("voice", -1)
    end)

    settingsWidget:bind_click("settings-voice-plus", function()
        adjust_setting("voice", 1)
    end)

    settingsWidget:bind_click("settings-mouse-minus", function()
        adjust_setting("mouse", -1)
    end)

    settingsWidget:bind_click("settings-mouse-plus", function()
        adjust_setting("mouse", 1)
    end)

    settingsWidget:bind_event("settings-gamma-slider", "mousedown", function(event)
        begin_settings_slider_drag("gamma", event)
    end)

    settingsWidget:bind_event("settings-bgm-slider", "mousedown", function(event)
        begin_settings_slider_drag("bgm", event)
    end)

    settingsWidget:bind_event("settings-sfx-slider", "mousedown", function(event)
        begin_settings_slider_drag("sfx", event)
    end)

    settingsWidget:bind_event("settings-voice-slider", "mousedown", function(event)
        begin_settings_slider_drag("voice", event)
    end)

    settingsWidget:bind_event("settings-mouse-slider", "mousedown", function(event)
        begin_settings_slider_drag("mouse", event)
    end)

    settingsWidget:bind_click("settings-toggle-zoom-row", function()
        toggle_setting("zoom")
    end)

    settingsWidget:bind_click("settings-toggle-sprint-row", function()
        toggle_setting("sprint")
    end)
end

local function bind_credits_dialog_clicks()
    creditsWidget:bind_click("credits-close-button", function()
        show_credits_panel(false)
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
        start_prologue_scene()
    end)

    widget:bind_click("continue-button", function()
        start_prologue_scene()
    end)

    widget:bind_click("settings-button", function()
        show_settings_panel(true)
    end)

    widget:bind_click("credits-button", function()
        show_credits_panel(true)
    end)
end

function BeginPlay()
    widget = UI.CreateWidget("Content/UI/Title/Title.rml")
    widget:SetWantsMouse(true)
    bind_clicks()
    widget:AddToViewportZ(100)
    update_title_background(true)

    quitWidget = UI.CreateWidget("Content/UI/Title/TitleQuitDialog.rml")
    quitWidget:SetWantsMouse(true)
    bind_quit_dialog_clicks()

    statWidget = UI.CreateWidget("Content/UI/Title/TitleScoreBoard.rml")
    statWidget:SetWantsMouse(true)
    bind_stat_dialog_clicks()

    settingsWidget = UI.CreateWidget("Content/UI/Title/TitleSettings.rml")
    settingsWidget:SetWantsMouse(true)
    bind_settings_dialog_clicks()
    sync_settings_from_engine()
    apply_settings_to_ui()

    creditsWidget = UI.CreateWidget("Content/UI/Title/TitleCredits.rml")
    creditsWidget:SetWantsMouse(true)
    bind_credits_dialog_clicks()

    show_stat_panel(false)
    show_settings_panel(false)
    show_credits_panel(false)
    show_quit_panel(false)

    Engine.SetOnEscape(function()
        if quitVisible then
            show_quit_panel(false)
        elseif settingsVisible then
            show_settings_panel(false)
        elseif creditsVisible then
            show_credits_panel(false)
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

    if settingsWidget ~= nil and settingsWidget:IsInViewport() then
        settingsWidget:RemoveFromParent()
    end
    settingsWidget = nil

    if creditsWidget ~= nil and creditsWidget:IsInViewport() then
        creditsWidget:RemoveFromParent()
    end
    creditsWidget = nil

    if widget ~= nil and widget:IsInViewport() then
        widget:RemoveFromParent()
    end
    widget = nil
end

function Tick(dt)
    update_title_background(false)
    if activeSettingsSlider ~= nil then
        if Input.GetKey(1) then
            update_active_settings_slider(Input.GetMouseClientX())
        else
            activeSettingsSlider = nil
        end
    end
end
