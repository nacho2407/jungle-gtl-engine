local InGamePause = {}

local PAUSE_MENU_PATH = "Content/UI/InGame/PauseMenu.rml"
local SETTINGS_PATH = "Content/UI/Title/TitleSettings.rml"
local PAUSE_Z_ORDER = 180
local SETTINGS_Z_ORDER = 190
local RESTART_SCENE = "Default.Scene"
local MAIN_MENU_SCENE = "Title.Scene"

local pauseWidget = nil
local settingsWidget = nil
local pauseVisible = false
local settingsVisible = false
local activeSettingsSlider = nil

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

local settingsSliderWidth = 220.0
local settingsHandleWidth = 16.0

local function set_game_paused(paused)
    if Engine == nil then return end

    if paused then
        if Engine.PauseGame ~= nil then
            Engine.PauseGame()
        end
    else
        if Engine.ResumeGame ~= nil then
            Engine.ResumeGame()
        end
    end
end

local function px(value)
    return string.format("%.2fpx", value)
end

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function round_to_step(value, step)
    return math.floor((value / step) + 0.5) * step
end

local function sync_settings_from_engine()
    if Engine == nil then return end

    if Engine.GetGamma ~= nil then
        settingsState.gamma = clamp(Engine.GetGamma(), settingSliders.gamma.min, settingSliders.gamma.max)
        settingsDefaults.gamma = settingsState.gamma
    end
    if Engine.GetMouseSensitivity ~= nil then
        settingsState.mouse = clamp(Engine.GetMouseSensitivity(), settingSliders.mouse.min, settingSliders.mouse.max)
        settingsDefaults.mouse = settingsState.mouse
    end
end

local function apply_setting_slider(name)
    if settingsWidget == nil then return end

    local config = settingSliders[name]
    if config == nil then return end

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
    if settingsWidget == nil then return end

    apply_setting_slider("gamma")
    apply_setting_slider("bgm")
    apply_setting_slider("sfx")
    apply_setting_slider("voice")
    apply_setting_slider("mouse")
    settingsWidget:SetText("settings-toggle-zoom-value", settingsState.zoom)
    settingsWidget:SetText("settings-toggle-sprint-value", settingsState.sprint)
    settingsWidget:SetText("settings-back-label", "Close")
end

local function adjust_setting(name, direction)
    local config = settingSliders[name]
    if config == nil then return end

    settingsState[name] = clamp(settingsState[name] + config.step * direction, config.min, config.max)
    settingsState[name] = round_to_step(settingsState[name], config.step)
    apply_setting_slider(name)
end

local function set_setting_from_slider_ratio(name, ratio)
    local config = settingSliders[name]
    if config == nil then return end

    local clampedRatio = clamp(ratio, 0.0, 1.0)
    local value = config.min + (config.max - config.min) * clampedRatio
    settingsState[name] = clamp(round_to_step(value, config.step), config.min, config.max)
    apply_setting_slider(name)
end

local function update_active_settings_slider(mouseX)
    if activeSettingsSlider == nil then return end

    local width = activeSettingsSlider.width
    if width <= 0.0 then
        width = settingsSliderWidth
    end

    set_setting_from_slider_ratio(activeSettingsSlider.name, (mouseX - activeSettingsSlider.left) / width)
end

local function begin_settings_slider_drag(name, event)
    activeSettingsSlider = {
        name = name,
        left = event.element_left or 0.0,
        width = event.element_width or settingsSliderWidth,
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

local function close_settings_show_pause()
    set_game_paused(true)

    settingsVisible = false
    activeSettingsSlider = nil
    if settingsWidget ~= nil and settingsWidget:IsInViewport() then
        settingsWidget:RemoveFromParent()
    end

    pauseVisible = true
    if pauseWidget ~= nil and not pauseWidget:IsInViewport() then
        pauseWidget:AddToViewportZ(PAUSE_Z_ORDER)
    end
end

local function show_settings()
    set_game_paused(true)

    pauseVisible = false
    if pauseWidget ~= nil and pauseWidget:IsInViewport() then
        pauseWidget:RemoveFromParent()
    end

    settingsVisible = true
    sync_settings_from_engine()
    if settingsWidget ~= nil and not settingsWidget:IsInViewport() then
        settingsWidget:AddToViewportZ(SETTINGS_Z_ORDER)
    end
    apply_settings_to_ui()
end

local function close_all()
    set_game_paused(false)

    pauseVisible = false
    settingsVisible = false
    activeSettingsSlider = nil

    if pauseWidget ~= nil and pauseWidget:IsInViewport() then
        pauseWidget:RemoveFromParent()
    end
    if settingsWidget ~= nil and settingsWidget:IsInViewport() then
        settingsWidget:RemoveFromParent()
    end
end

local function transition_to(scene)
    close_all()
    if Engine ~= nil and Engine.TransitionToScene ~= nil then
        Engine.TransitionToScene(scene)
    end
end

local function bind_pause_menu()
    if pauseWidget == nil then return end

    pauseWidget:bind_click("pause-restart-button", function()
        transition_to(RESTART_SCENE)
    end)

    pauseWidget:bind_click("pause-settings-button", function()
        show_settings()
    end)

    pauseWidget:bind_click("pause-main-menu-button", function()
        transition_to(MAIN_MENU_SCENE)
    end)

    pauseWidget:bind_click("pause-quit-button", function()
        close_all()
        if Engine ~= nil and Engine.Exit ~= nil then
            Engine.Exit()
        end
    end)
end

local function bind_settings_menu()
    if settingsWidget == nil then return end

    settingsWidget:bind_click("settings-back-button", function()
        close_settings_show_pause()
    end)

    settingsWidget:bind_click("settings-restore-button", function()
        restore_default_settings()
    end)

    settingsWidget:bind_click("settings-gamma-minus", function() adjust_setting("gamma", -1) end)
    settingsWidget:bind_click("settings-gamma-plus", function() adjust_setting("gamma", 1) end)
    settingsWidget:bind_click("settings-bgm-minus", function() adjust_setting("bgm", -1) end)
    settingsWidget:bind_click("settings-bgm-plus", function() adjust_setting("bgm", 1) end)
    settingsWidget:bind_click("settings-sfx-minus", function() adjust_setting("sfx", -1) end)
    settingsWidget:bind_click("settings-sfx-plus", function() adjust_setting("sfx", 1) end)
    settingsWidget:bind_click("settings-voice-minus", function() adjust_setting("voice", -1) end)
    settingsWidget:bind_click("settings-voice-plus", function() adjust_setting("voice", 1) end)
    settingsWidget:bind_click("settings-mouse-minus", function() adjust_setting("mouse", -1) end)
    settingsWidget:bind_click("settings-mouse-plus", function() adjust_setting("mouse", 1) end)

    settingsWidget:bind_event("settings-gamma-slider", "mousedown", function(event) begin_settings_slider_drag("gamma", event) end)
    settingsWidget:bind_event("settings-bgm-slider", "mousedown", function(event) begin_settings_slider_drag("bgm", event) end)
    settingsWidget:bind_event("settings-sfx-slider", "mousedown", function(event) begin_settings_slider_drag("sfx", event) end)
    settingsWidget:bind_event("settings-voice-slider", "mousedown", function(event) begin_settings_slider_drag("voice", event) end)
    settingsWidget:bind_event("settings-mouse-slider", "mousedown", function(event) begin_settings_slider_drag("mouse", event) end)
    settingsWidget:bind_event("settings-screen", "mousemove", function(event)
        if activeSettingsSlider ~= nil then
            update_active_settings_slider(event.mouse_x or Input.GetMouseClientX())
        end
    end)
    settingsWidget:bind_event("settings-screen", "mouseup", function()
        activeSettingsSlider = nil
    end)

    settingsWidget:bind_click("settings-toggle-zoom-row", function() toggle_setting("zoom") end)
    settingsWidget:bind_click("settings-toggle-sprint-row", function() toggle_setting("sprint") end)
end

function InGamePause.Initialize()
    if pauseWidget == nil then
        pauseWidget = UI.CreateWidget(PAUSE_MENU_PATH)
        if pauseWidget ~= nil then
            pauseWidget:SetWantsMouse(true)
            bind_pause_menu()
        end
    end

    if settingsWidget == nil then
        settingsWidget = UI.CreateWidget(SETTINGS_PATH)
        if settingsWidget ~= nil then
            settingsWidget:SetWantsMouse(true)
            bind_settings_menu()
            sync_settings_from_engine()
            apply_settings_to_ui()
        end
    end
end

function InGamePause.Toggle()
    InGamePause.Initialize()

    if settingsVisible then
        close_settings_show_pause()
        return
    end

    if pauseVisible then
        close_all()
        return
    end

    pauseVisible = true
    set_game_paused(true)
    if pauseWidget ~= nil and not pauseWidget:IsInViewport() then
        pauseWidget:AddToViewportZ(PAUSE_Z_ORDER)
    end
end

function InGamePause.IsOpen()
    return pauseVisible or settingsVisible
end

function InGamePause.Tick()
    if activeSettingsSlider ~= nil then
        if Input.GetKey(1) then
            update_active_settings_slider(Input.GetMouseClientX())
        else
            activeSettingsSlider = nil
        end
    end
end

function InGamePause.Shutdown()
    close_all()
    pauseWidget = nil
    settingsWidget = nil
end

return InGamePause
