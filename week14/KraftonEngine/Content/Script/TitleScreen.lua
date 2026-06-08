local HoverDescription = require("HoverDescription")
local LoadingScreen = require("LoadingScreen")

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
local activeSettingsValueEdit = nil
local titleBgmKey = "TitleIntroBGM"
local titleBgmLoaded = false
local KEY_LEFT = 37
local KEY_UP = 38
local KEY_RIGHT = 39
local KEY_DOWN = 40
local NAV_AXIS_THRESHOLD = 0.55
local selectedMainMenuIndex = 1
local selectedQuitDialogIndex = 1
local mainMenuItems = nil
local quitDialogItems = nil
local menuNavAxisHeld = {
    up = false,
    down = false,
    left = false,
    right = false,
}
local loadingToGame = false
local loadingTransitionRequested = false

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

local function get_key(keyName)
    if Key == nil then
        return nil
    end
    return Key[keyName]
end

local function is_input_pressed(key)
    return Input ~= nil
        and Input.GetKeyDown ~= nil
        and key ~= nil
        and Input.GetKeyDown(key)
end

local function is_key_pressed(keyCode)
    return is_input_pressed(keyCode)
end

local function get_gamepad_axis(axisName)
    if Input == nil or Input.GetGamepadAxis == nil or Axis == nil then
        return 0.0
    end

    local axisCode = Axis[axisName]
    if axisCode == nil then
        return 0.0
    end

    return Input.GetGamepadAxis(-1, axisCode)
end

local function consume_axis_press(name, down)
    local wasHeld = menuNavAxisHeld[name] == true
    menuNavAxisHeld[name] = down == true
    return down == true and not wasHeld
end

local function reset_menu_axis_state()
    menuNavAxisHeld.up = false
    menuNavAxisHeld.down = false
    menuNavAxisHeld.left = false
    menuNavAxisHeld.right = false
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
    activeSettingsValueEdit = nil
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

local function commit_settings_value_edit()
    if activeSettingsValueEdit == nil then
        return
    end

    local edit = activeSettingsValueEdit
    activeSettingsValueEdit = nil
    local config = settingSliders[edit.name]
    if config == nil then
        return
    end

    local value = tonumber(edit.buffer)
    if value ~= nil then
        value = clamp(round_to_step(value, config.step), config.min, config.max)
        settingsState[edit.name] = value
    end
    apply_setting_slider(edit.name)
end

local function cancel_settings_value_edit()
    if activeSettingsValueEdit == nil then
        return
    end

    local name = activeSettingsValueEdit.name
    activeSettingsValueEdit = nil
    apply_setting_slider(name)
end

local function begin_settings_value_edit(name)
    local config = settingSliders[name]
    if config == nil then
        return
    end

    activeSettingsSlider = nil
    activeSettingsValueEdit = {
        name = name,
        buffer = format_setting_value(name, settingsState[name]),
        replaceOnInput = true
    }
    settingsWidget:SetText(config.value, activeSettingsValueEdit.buffer)
end

local function append_settings_value_edit_char(ch)
    if activeSettingsValueEdit == nil then
        return
    end

    local edit = activeSettingsValueEdit
    if edit.replaceOnInput then
        edit.buffer = ""
        edit.replaceOnInput = false
    end
    if ch == "." and string.find(edit.buffer, ".", 1, true) ~= nil then
        return
    end
    if ch == "." and settingSliders[edit.name] ~= nil and settingSliders[edit.name].integer then
        return
    end
    if string.len(edit.buffer) >= 8 then
        return
    end

    edit.buffer = edit.buffer .. ch
    settingsWidget:SetText(settingSliders[edit.name].value, edit.buffer)
end

local function backspace_settings_value_edit()
    if activeSettingsValueEdit == nil then
        return
    end

    local edit = activeSettingsValueEdit
    edit.replaceOnInput = false
    local len = string.len(edit.buffer)
    if len > 0 then
        edit.buffer = string.sub(edit.buffer, 1, len - 1)
    end
    settingsWidget:SetText(settingSliders[edit.name].value, edit.buffer)
end

local function update_settings_value_edit()
    if activeSettingsValueEdit == nil then
        return
    end

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

local function play_title_bgm()
    if AudioManager == nil or AudioManager.Load == nil or AudioManager.PlayBGM == nil then
        return
    end

    if not titleBgmLoaded then
        titleBgmLoaded = AudioManager.Load(titleBgmKey, "Music/Intro.mp3", true)
    end

    if titleBgmLoaded then
        AudioManager.PlayBGM(titleBgmKey, 0.68)
    end
end

local function stop_title_bgm()
    if AudioManager ~= nil and AudioManager.StopBGM ~= nil then
        AudioManager.StopBGM()
    end
end

local function start_prologue_scene()
    if loadingToGame then
        return
    end

    loadingToGame = true
    loadingTransitionRequested = false
    show_stat_panel(false)
    show_settings_panel(false)
    show_quit_panel(false)
    show_credits_panel(false)
    if ScoreManager ~= nil and ScoreManager.ResetRun ~= nil then
        -- 타이틀에서 새 게임을 시작하면 이전 플레이의 점수 누적을 모두 초기화합니다.
        ScoreManager.ResetRun()
    end
    stop_title_bgm()
    LoadingScreen.Show({ duration = 1.0, zOrder = 240, keepVisibleOnComplete = true })
end

local function transition_to_game_scene()
    if Engine.TransitionToScene ~= nil then
        Engine.TransitionToScene("FL_Level1.Scene")
    else
        print("[Title] Engine.TransitionToScene is not available")
    end
end

local function set_button_selected(targetWidget, item, selected)
    if targetWidget == nil or item == nil then
        return
    end

    targetWidget:set_property(item.box, "opacity", selected and "1.0" or "0.0")
    targetWidget:set_property(item.label, "color", selected and "#111111" or "#858585E6")
end

local function refresh_main_menu_selection()
    if widget == nil or mainMenuItems == nil then
        return
    end

    for index, item in ipairs(mainMenuItems) do
        set_button_selected(widget, item, index == selectedMainMenuIndex)
    end
end

local function set_main_menu_selection(index)
    if mainMenuItems == nil or #mainMenuItems == 0 then
        return
    end

    if index < 1 then
        index = #mainMenuItems
    elseif index > #mainMenuItems then
        index = 1
    end

    selectedMainMenuIndex = index
    refresh_main_menu_selection()
end

local function move_main_menu_selection(direction)
    if mainMenuItems == nil then
        return
    end

    if direction == "left" or direction == "right" then
        if selectedMainMenuIndex == 1 then
            set_main_menu_selection(2)
        elseif selectedMainMenuIndex == 2 then
            set_main_menu_selection(1)
        end
        return
    end

    if direction == "down" then
        if selectedMainMenuIndex == 1 or selectedMainMenuIndex == 2 then
            set_main_menu_selection(3)
        else
            set_main_menu_selection(selectedMainMenuIndex + 1)
        end
    elseif direction == "up" then
        if selectedMainMenuIndex == 1 or selectedMainMenuIndex == 2 then
            set_main_menu_selection(#mainMenuItems)
        elseif selectedMainMenuIndex == 3 then
            set_main_menu_selection(1)
        else
            set_main_menu_selection(selectedMainMenuIndex - 1)
        end
    end
end

local function execute_main_menu_selection()
    if mainMenuItems == nil then
        return
    end

    local item = mainMenuItems[selectedMainMenuIndex]
    if item ~= nil and item.action ~= nil then
        item.action()
    end
end

local function refresh_quit_dialog_selection()
    if quitWidget == nil or quitDialogItems == nil then
        return
    end

    for index, item in ipairs(quitDialogItems) do
        set_button_selected(quitWidget, item, index == selectedQuitDialogIndex)
    end
end

local function set_quit_dialog_selection(index)
    if quitDialogItems == nil or #quitDialogItems == 0 then
        return
    end

    if index < 1 then
        index = #quitDialogItems
    elseif index > #quitDialogItems then
        index = 1
    end

    selectedQuitDialogIndex = index
    refresh_quit_dialog_selection()
end

local function execute_quit_dialog_selection()
    if quitDialogItems == nil then
        return
    end

    local item = quitDialogItems[selectedQuitDialogIndex]
    if item ~= nil and item.action ~= nil then
        item.action()
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
        set_quit_dialog_selection(1)
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
    for index, item in ipairs(quitDialogItems) do
        quitWidget:bind_click(item.id, function()
            set_quit_dialog_selection(index)
            execute_quit_dialog_selection()
        end)
        quitWidget:bind_event(item.id, "mouseover", function()
            set_quit_dialog_selection(index)
        end)
    end
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

    settingsWidget:bind_click("settings-gamma-value", function()
        begin_settings_value_edit("gamma")
    end)

    settingsWidget:bind_click("settings-bgm-value", function()
        begin_settings_value_edit("bgm")
    end)

    settingsWidget:bind_click("settings-sfx-value", function()
        begin_settings_value_edit("sfx")
    end)

    settingsWidget:bind_click("settings-voice-value", function()
        begin_settings_value_edit("voice")
    end)

    settingsWidget:bind_click("settings-mouse-value", function()
        begin_settings_value_edit("mouse")
    end)
end

local function bind_credits_dialog_clicks()
    creditsWidget:bind_click("credits-close-button", function()
        show_credits_panel(false)
    end)
end

local function initialize_navigation_items()
    mainMenuItems = {
        { id = "new-game-button", box = "new-game-box", label = "new-game-label", action = start_prologue_scene },
        { id = "continue-button", box = "continue-box", label = "continue-label", action = start_prologue_scene },
        { id = "settings-button", box = "settings-box", label = "settings-label", action = function() show_settings_panel(true) end },
        { id = "credits-button", box = "credits-box", label = "credits-label", action = function() show_credits_panel(true) end },
        { id = "quit-button", box = "quit-box", label = "quit-label", action = function() show_quit_panel(true) end },
    }

    quitDialogItems = {
        { id = "cancel-quit-button", box = "cancel-quit-box", label = "cancel-quit-label", action = function() show_quit_panel(false) end },
        { id = "confirm-quit-button", box = "confirm-quit-box", label = "confirm-quit-label", action = function() Engine.Exit() end },
    }
end

local function bind_clicks()
    widget:bind_click("stat-button", function()
        show_stat_panel(not statVisible)
    end)

    for index, item in ipairs(mainMenuItems) do
        widget:bind_click(item.id, function()
            set_main_menu_selection(index)
            execute_main_menu_selection()
        end)
        widget:bind_event(item.id, "mouseover", function()
            set_main_menu_selection(index)
        end)
    end
end

local function is_nav_up_pressed()
    return is_key_pressed(KEY_UP)
        or is_input_pressed(get_key("GamepadDPadUp"))
        or consume_axis_press("up", get_gamepad_axis("GamepadLeftY") > NAV_AXIS_THRESHOLD)
end

local function is_nav_down_pressed()
    return is_key_pressed(KEY_DOWN)
        or is_input_pressed(get_key("GamepadDPadDown"))
        or consume_axis_press("down", get_gamepad_axis("GamepadLeftY") < -NAV_AXIS_THRESHOLD)
end

local function is_nav_left_pressed()
    return is_key_pressed(KEY_LEFT)
        or is_input_pressed(get_key("GamepadDPadLeft"))
        or consume_axis_press("left", get_gamepad_axis("GamepadLeftX") < -NAV_AXIS_THRESHOLD)
end

local function is_nav_right_pressed()
    return is_key_pressed(KEY_RIGHT)
        or is_input_pressed(get_key("GamepadDPadRight"))
        or consume_axis_press("right", get_gamepad_axis("GamepadLeftX") > NAV_AXIS_THRESHOLD)
end

local function is_accept_pressed()
    return is_input_pressed(get_key("Enter"))
        or is_input_pressed(get_key("GamepadA"))
end

local function is_back_pressed()
    return is_input_pressed(get_key("GamepadB"))
end

local function update_quit_dialog_navigation()
    local moved = false
    if is_nav_up_pressed() or is_nav_left_pressed() then
        set_quit_dialog_selection(selectedQuitDialogIndex - 1)
        moved = true
    elseif is_nav_down_pressed() or is_nav_right_pressed() then
        set_quit_dialog_selection(selectedQuitDialogIndex + 1)
        moved = true
    end

    if moved then
        return
    end

    if is_accept_pressed() then
        execute_quit_dialog_selection()
    elseif is_back_pressed() then
        show_quit_panel(false)
    end
end

local function update_main_menu_navigation()
    if is_nav_up_pressed() then
        move_main_menu_selection("up")
    elseif is_nav_down_pressed() then
        move_main_menu_selection("down")
    elseif is_nav_left_pressed() then
        move_main_menu_selection("left")
    elseif is_nav_right_pressed() then
        move_main_menu_selection("right")
    elseif is_accept_pressed() then
        execute_main_menu_selection()
    end
end

local function update_title_navigation()
    if activeSettingsValueEdit ~= nil then
        return
    end

    if quitVisible then
        update_quit_dialog_navigation()
        return
    end

    if settingsVisible then
        if is_back_pressed() then
            show_settings_panel(false)
        end
        reset_menu_axis_state()
        return
    end

    if creditsVisible then
        if is_back_pressed() then
            show_credits_panel(false)
        end
        reset_menu_axis_state()
        return
    end

    if statVisible then
        if is_back_pressed() then
            show_stat_panel(false)
        end
        reset_menu_axis_state()
        return
    end

    update_main_menu_navigation()
end

function BeginPlay()
    play_title_bgm()

    widget = UI.CreateWidget("Content/UI/Title/Title.rml")
    widget:SetWantsMouse(true)
    initialize_navigation_items()
    bind_clicks()
    widget:AddToViewportZ(100)
    set_main_menu_selection(1)
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
    LoadingScreen.Hide()
    stop_title_bgm()

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
    loadingToGame = false
    loadingTransitionRequested = false
end

function Tick(dt)
    if loadingToGame then
        if LoadingScreen.Tick(dt or 0.0) and not loadingTransitionRequested then
            loadingTransitionRequested = true
            transition_to_game_scene()
        end
        return
    end

    update_title_background(false)
    update_settings_value_edit()
    update_title_navigation()
    if activeSettingsSlider ~= nil then
        if Input.GetKey(1) then
            update_active_settings_slider(Input.GetMouseClientX())
        else
            activeSettingsSlider = nil
        end
    end
end
