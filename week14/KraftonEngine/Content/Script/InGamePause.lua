local InGamePause = {}
local HoverDescription = require("HoverDescription")

local PAUSE_MENU_PATH = "Content/UI/InGame/PauseMenu.rml"
local SETTINGS_PATH = "Content/UI/Title/TitleSettings.rml"
local QUIT_DIALOG_PATH = "Content/UI/Title/TitleQuitDialog.rml"
local PAUSE_Z_ORDER = 180
local SETTINGS_Z_ORDER = 190
local QUIT_DIALOG_Z_ORDER = 200
local MAIN_MENU_SCENE = "Title.Scene"

local pauseWidget = nil
local settingsWidget = nil
local quitWidget = nil
local pauseVisible = false
local settingsVisible = false
local quitVisible = false
local activeSettingsSlider = nil
local activeSettingsValueEdit = nil

local settingsState = {
    gamma = 2.40,
    bgm = 80,
    sfx = 80,
    voice = 80,
    mouse = 1.00,
    zoom = "Hold",
    sprint = "Toggle",
}

local settingsDefaults = {
    gamma = 2.40,
    bgm = 80,
    sfx = 80,
    voice = 80,
    mouse = 1.00,
    zoom = "Hold",
    sprint = "Toggle",
}

local settingSliders = {
    gamma = { value = "settings-gamma-value", fill = "settings-gamma-fill", handle = "settings-gamma-handle", min = 1.00, max = 3.00, step = 0.01, format = "%.2f" },
    bgm = { value = "settings-bgm-value", fill = "settings-bgm-fill", handle = "settings-bgm-handle", min = 0, max = 100, step = 1.0, format = "%.0f", integer = true },
    sfx = { value = "settings-sfx-value", fill = "settings-sfx-fill", handle = "settings-sfx-handle", min = 0, max = 100, step = 1.0, format = "%.0f", integer = true },
    voice = { value = "settings-voice-value", fill = "settings-voice-fill", handle = "settings-voice-handle", min = 0, max = 100, step = 1.0, format = "%.0f", integer = true },
    mouse = { value = "settings-mouse-value", fill = "settings-mouse-fill", handle = "settings-mouse-handle", min = 0.05, max = 15.00, step = 0.05, format = "%.2f" },
}

local settingsSliderWidth = 220.0
local settingsHandleWidth = 16.0

local pauseDefaultDescription = {
    title = "PAUSE MENU",
    body = "Choose how to continue from the paused game.",
    note = "Hover a menu item to preview what it does.",
}

local pauseDescriptions = {
    {
        id = "pause-resume-button",
        title = "RESUME",
        body = "Close the pause menu and return to the current game.",
        note = "You can also press the pause key again.",
    },
    {
        id = "pause-restart-button",
        title = "RESTART",
        body = "Restart the current playable scene from the beginning.",
        note = "Unsaved progress in the current run will be lost.",
    },
    {
        id = "pause-settings-button",
        title = "SETTINGS",
        body = "Open gameplay, audio, and control options without leaving the game.",
        note = "Close returns to the paused menu.",
    },
    {
        id = "pause-main-menu-button",
        title = "RETURN TO MAIN MENU",
        body = "Leave the current session and return to the title screen.",
        note = "Use this when you want to stop the current run.",
    },
    {
        id = "pause-quit-button",
        title = "QUIT",
        body = "Exit the game application.",
        note = "In editor play mode this exits the running play session.",
    },
}

local settingsDefaultDescription = {
    title = "GAME SETTINGS",
    body = "Adjust visual brightness, audio mix, and mouse feel before returning to play.",
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
        note = "This value is applied to the BGM audio bus immediately.",
    },
    {
        id = "settings-sfx-row",
        title = "SFX VOLUME",
        body = "Control weapon, impact, movement, and interface sound effects.",
        note = "This value is applied to the SFX and UI audio buses immediately.",
    },
    {
        id = "settings-voice-row",
        title = "VOICE VOLUME",
        body = "Control dialogue and voice playback volume.",
        note = "This value is applied to the Voice audio bus immediately.",
    },
    {
        id = "settings-mouse-row",
        title = "MOUSE SENSITIVITY",
        body = "Adjust camera turn speed for mouse input.",
        note = "1.00 matches the default mouse sensitivity.",
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
        title = "CLOSE",
        body = "Close settings and return to the paused menu.",
        note = "Current settings remain applied.",
    },
    {
        id = "settings-restore-button",
        title = "RESTORE DEFAULTS",
        body = "Restore settings to the values captured when this settings panel opened.",
        note = "Gamma and mouse sensitivity are applied immediately.",
    },
}

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

local function format_setting_value(name, value)
    local config = settingSliders[name]
    if config == nil then
        return tostring(value)
    end
    return string.format(config.format, value)
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
    if AudioManager ~= nil and AudioManager.GetBusVolume ~= nil then
        settingsState.bgm = clamp(AudioManager.GetBusVolume("BGM") * 100.0, settingSliders.bgm.min, settingSliders.bgm.max)
        settingsState.sfx = clamp(AudioManager.GetBusVolume("SFX") * 100.0, settingSliders.sfx.min, settingSliders.sfx.max)
        settingsState.voice = clamp(AudioManager.GetBusVolume("Voice") * 100.0, settingSliders.voice.min, settingSliders.voice.max)
        settingsDefaults.bgm = settingsState.bgm
        settingsDefaults.sfx = settingsState.sfx
        settingsDefaults.voice = settingsState.voice
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

    if activeSettingsValueEdit == nil or activeSettingsValueEdit.name ~= name then
        settingsWidget:SetText(config.value, format_setting_value(name, value))
    end
    settingsWidget:set_property(config.fill, "width", px(fillWidth))
    settingsWidget:set_property(config.handle, "left", px(handleLeft))

    if name == "gamma" and Engine ~= nil and Engine.SetGamma ~= nil then
        Engine.SetGamma(value)
    elseif name == "mouse" and Engine ~= nil and Engine.SetMouseSensitivity ~= nil then
        Engine.SetMouseSensitivity(value)
    elseif name == "bgm" and AudioManager ~= nil and AudioManager.SetBusVolume ~= nil then
        AudioManager.SetBusVolume("BGM", value / 100.0)
    elseif name == "sfx" and AudioManager ~= nil and AudioManager.SetBusVolume ~= nil then
        AudioManager.SetBusVolume("SFX", value / 100.0)
    elseif name == "voice" and AudioManager ~= nil and AudioManager.SetBusVolume ~= nil then
        AudioManager.SetBusVolume("Voice", value / 100.0)
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

    activeSettingsValueEdit = nil
    local value = settingsState[name]
    if config.integer then
        if direction > 0 then
            local ceiled = math.ceil(value)
            value = (math.abs(value - ceiled) < 0.0001) and (ceiled + 1.0) or ceiled
        else
            local floored = math.floor(value)
            value = (math.abs(value - floored) < 0.0001) and (floored - 1.0) or floored
        end
    else
        value = round_to_step(value + config.step * direction, config.step)
    end

    settingsState[name] = clamp(value, config.min, config.max)
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
    activeSettingsValueEdit = nil
    activeSettingsSlider = {
        name = name,
        left = event.element_left or 0.0,
        width = event.element_width or settingsSliderWidth,
    }
    update_active_settings_slider(event.mouse_x or Input.GetMouseClientX())
end

local function commit_settings_value_edit()
    if activeSettingsValueEdit == nil then return end

    local edit = activeSettingsValueEdit
    activeSettingsValueEdit = nil
    local config = settingSliders[edit.name]
    if config == nil then return end

    local value = tonumber(edit.buffer)
    if value ~= nil then
        settingsState[edit.name] = clamp(round_to_step(value, config.step), config.min, config.max)
    end
    apply_setting_slider(edit.name)
end

local function cancel_settings_value_edit()
    if activeSettingsValueEdit == nil then return end

    local name = activeSettingsValueEdit.name
    activeSettingsValueEdit = nil
    apply_setting_slider(name)
end

local function begin_settings_value_edit(name)
    local config = settingSliders[name]
    if config == nil then return end

    activeSettingsSlider = nil
    activeSettingsValueEdit = {
        name = name,
        buffer = format_setting_value(name, settingsState[name]),
        replaceOnInput = true
    }
    settingsWidget:SetText(config.value, activeSettingsValueEdit.buffer)
end

local function append_settings_value_edit_char(ch)
    if activeSettingsValueEdit == nil then return end

    local edit = activeSettingsValueEdit
    local config = settingSliders[edit.name]
    if edit.replaceOnInput then
        edit.buffer = ""
        edit.replaceOnInput = false
    end
    if ch == "." and (config == nil or config.integer or string.find(edit.buffer, ".", 1, true) ~= nil) then
        return
    end
    if string.len(edit.buffer) >= 8 then
        return
    end

    edit.buffer = edit.buffer .. ch
    settingsWidget:SetText(config.value, edit.buffer)
end

local function backspace_settings_value_edit()
    if activeSettingsValueEdit == nil then return end

    local edit = activeSettingsValueEdit
    local config = settingSliders[edit.name]
    edit.replaceOnInput = false
    local len = string.len(edit.buffer)
    if len > 0 then
        edit.buffer = string.sub(edit.buffer, 1, len - 1)
    end
    settingsWidget:SetText(config.value, edit.buffer)
end

local function update_settings_value_edit()
    if activeSettingsValueEdit == nil then return end

    for i = 0, 9 do
        if Input.GetKeyDown(48 + i) or Input.GetKeyDown(96 + i) then
            append_settings_value_edit_char(tostring(i))
        end
    end

    if Input.GetKeyDown(190) or Input.GetKeyDown(110) then
        append_settings_value_edit_char(".")
    end
    if Input.GetKeyDown(8) then
        backspace_settings_value_edit()
    end
    if Input.GetKeyDown(Key.Enter) then
        commit_settings_value_edit()
    end
    if Input.GetKeyDown(Key.Escape) then
        cancel_settings_value_edit()
    end
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
    quitVisible = false
    activeSettingsSlider = nil
    activeSettingsValueEdit = nil
    if settingsWidget ~= nil and settingsWidget:IsInViewport() then
        settingsWidget:RemoveFromParent()
    end
    if quitWidget ~= nil and quitWidget:IsInViewport() then
        quitWidget:RemoveFromParent()
    end

    pauseVisible = true
    if pauseWidget ~= nil and not pauseWidget:IsInViewport() then
        pauseWidget:AddToViewportZ(PAUSE_Z_ORDER)
    end
end

local function show_settings()
    set_game_paused(true)

    pauseVisible = false
    quitVisible = false
    if pauseWidget ~= nil and pauseWidget:IsInViewport() then
        pauseWidget:RemoveFromParent()
    end
    if quitWidget ~= nil and quitWidget:IsInViewport() then
        quitWidget:RemoveFromParent()
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
    quitVisible = false
    activeSettingsSlider = nil
    activeSettingsValueEdit = nil

    if pauseWidget ~= nil and pauseWidget:IsInViewport() then
        pauseWidget:RemoveFromParent()
    end
    if settingsWidget ~= nil and settingsWidget:IsInViewport() then
        settingsWidget:RemoveFromParent()
    end
    if quitWidget ~= nil and quitWidget:IsInViewport() then
        quitWidget:RemoveFromParent()
    end
end

local function transition_to(scene)
    close_all()
    if Engine ~= nil and Engine.TransitionToScene ~= nil then
        Engine.TransitionToScene(scene)
    end
end

local function show_quit_dialog()
    set_game_paused(true)

    quitVisible = true
    if quitWidget ~= nil and not quitWidget:IsInViewport() then
        quitWidget:AddToViewportZ(QUIT_DIALOG_Z_ORDER)
    end
end

local function bind_pause_menu()
    if pauseWidget == nil then return end

    HoverDescription.Bind(
        pauseWidget,
        { title = "pause-help-title", body = "pause-help-body", note = "pause-help-note" },
        pauseDefaultDescription,
        pauseDescriptions)

    pauseWidget:bind_click("pause-resume-button", function()
        close_all()
    end)

    pauseWidget:bind_click("pause-restart-button", function()
        close_all()
        if Game ~= nil and Game.RestartLevel ~= nil then
            Game.RestartLevel()
        end
    end)

    pauseWidget:bind_click("pause-settings-button", function()
        show_settings()
    end)

    pauseWidget:bind_click("pause-main-menu-button", function()
        transition_to(MAIN_MENU_SCENE)
    end)

    pauseWidget:bind_click("pause-quit-button", function()
        show_quit_dialog()
    end)
end

local function bind_quit_dialog()
    if quitWidget == nil then return end

    quitWidget:bind_click("cancel-quit-button", function()
        quitVisible = false
        if quitWidget ~= nil and quitWidget:IsInViewport() then
            quitWidget:RemoveFromParent()
        end
    end)

    quitWidget:bind_click("confirm-quit-button", function()
        close_all()
        if Engine ~= nil and Engine.Exit ~= nil then
            Engine.Exit()
        end
    end)
end

local function bind_settings_menu()
    if settingsWidget == nil then return end

    HoverDescription.Bind(
        settingsWidget,
        { title = "settings-help-title", body = "settings-help-body", note = "settings-help-note" },
        settingsDefaultDescription,
        settingsDescriptions)

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
    settingsWidget:bind_click("settings-gamma-value", function() begin_settings_value_edit("gamma") end)
    settingsWidget:bind_click("settings-bgm-value", function() begin_settings_value_edit("bgm") end)
    settingsWidget:bind_click("settings-sfx-value", function() begin_settings_value_edit("sfx") end)
    settingsWidget:bind_click("settings-voice-value", function() begin_settings_value_edit("voice") end)
    settingsWidget:bind_click("settings-mouse-value", function() begin_settings_value_edit("mouse") end)
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

    if quitWidget == nil then
        quitWidget = UI.CreateWidget(QUIT_DIALOG_PATH)
        if quitWidget ~= nil then
            quitWidget:SetWantsMouse(true)
            bind_quit_dialog()
        end
    end
end

function InGamePause.Toggle()
    InGamePause.Initialize()

    if quitVisible then
        quitVisible = false
        if quitWidget ~= nil and quitWidget:IsInViewport() then
            quitWidget:RemoveFromParent()
        end
        return
    end

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
    return pauseVisible or settingsVisible or quitVisible
end

function InGamePause.Tick()
    update_settings_value_edit()
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
    quitWidget = nil
end

return InGamePause
