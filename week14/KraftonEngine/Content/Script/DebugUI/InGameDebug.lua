local InGameDebug = {}

local WIDGET_PATH = "Content/UI/DebugUI/InGameDebug.rml"
local DEFAULT_Z_ORDER = 220
local SPEED_STEP = 0.25
local DAMAGE_STEP = 5.0
local HEALTH_STEP = 5.0

local widget = nil
local callbacks = {}
local zOrder = DEFAULT_Z_ORDER

local speedMultiplier = 1.0
local damageAmount = 10.0
local isInvincible = false
local activeTab = "player"
local windowX = 40.0
local windowY = 48.0
local dragging = false
local dragOffsetX = 0.0
local dragOffsetY = 0.0

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function px(value)
    return string.format("%.2fpx", value)
end

local function format_number(value, decimals)
    return string.format("%." .. tostring(decimals) .. "f", value)
end

local function get_health()
    if callbacks.GetHealth ~= nil then
        return callbacks.GetHealth()
    end
    return 0.0
end

local function get_health_ratio()
    if callbacks.GetHealthRatio ~= nil then
        return callbacks.GetHealthRatio()
    end
    return 0.0
end

local function set_health(value)
    if callbacks.SetHealth ~= nil then
        callbacks.SetHealth(value)
    end
end

local function apply_damage(amount)
    if callbacks.ApplyDamage ~= nil then
        callbacks.ApplyDamage(amount)
    end
end

local function apply_speed_multiplier()
    if callbacks.ApplySpeedMultiplier ~= nil then
        callbacks.ApplySpeedMultiplier(speedMultiplier)
    end
end

local function set_text(id, text)
    if widget == nil then return end
    widget:SetText(id, text)
end

local function set_property(id, property, value)
    if widget == nil then return end
    widget:SetProperty(id, property, value)
end

local function update_values()
    if widget == nil then return end

    set_text("debug-speed-value", format_number(speedMultiplier, 2) .. "x")
    set_text("debug-invincible-toggle", isInvincible and "ON" or "OFF")
    set_text("debug-damage-value", tostring(math.floor(damageAmount + 0.5)))
    set_text("debug-health-value", tostring(math.floor(get_health() + 0.5)))
    set_text("debug-health-label", "Health " .. tostring(math.floor(get_health_ratio() * 100.0 + 0.5)) .. "%")
    set_property("debug-invincible-toggle", "color", isInvincible and "#111111" or "#d8d8d8")
    set_property("debug-invincible-toggle", "background-color", isInvincible and "#ffffff" or "#ffffff1F")
end

local function update_tab_visuals()
    if widget == nil then return end

    local tabs = { "player", "world", "ai", "render", "audio" }
    for _, tab in ipairs(tabs) do
        local selected = tab == activeTab
        set_property("debug-tab-" .. tab, "color", selected and "#ffffff" or "#808080")
        set_property("debug-panel-" .. tab, "display", selected and "block" or "none")
    end
end

local function set_tab(tab)
    activeTab = tab
    update_tab_visuals()
end

local function set_window_position(x, y)
    windowX = clamp(x, 0.0, 1600.0)
    windowY = clamp(y, 0.0, 900.0)
    set_property("debug-window", "left", px(windowX))
    set_property("debug-window", "top", px(windowY))
end

local function close()
    if widget ~= nil and widget:IsInViewport() then
        widget:RemoveFromParent()
        widget:SetWantsMouse(false)
        dragging = false
    end
end

local function bind_events()
    if widget == nil then return end

    widget:bind_click("debug-close", function()
        close()
    end)

    widget:bind_click("debug-tab-player", function() set_tab("player") end)
    widget:bind_click("debug-tab-world", function() set_tab("world") end)
    widget:bind_click("debug-tab-ai", function() set_tab("ai") end)
    widget:bind_click("debug-tab-render", function() set_tab("render") end)
    widget:bind_click("debug-tab-audio", function() set_tab("audio") end)

    widget:bind_click("debug-speed-minus", function()
        speedMultiplier = clamp(speedMultiplier - SPEED_STEP, 0.25, 8.0)
        apply_speed_multiplier()
        update_values()
    end)
    widget:bind_click("debug-speed-plus", function()
        speedMultiplier = clamp(speedMultiplier + SPEED_STEP, 0.25, 8.0)
        apply_speed_multiplier()
        update_values()
    end)
    widget:bind_click("debug-invincible-toggle", function()
        isInvincible = not isInvincible
        update_values()
    end)
    widget:bind_click("debug-damage-minus", function()
        damageAmount = clamp(damageAmount - DAMAGE_STEP, 0.0, 100.0)
        update_values()
    end)
    widget:bind_click("debug-damage-plus", function()
        damageAmount = clamp(damageAmount + DAMAGE_STEP, 0.0, 100.0)
        update_values()
    end)
    widget:bind_click("debug-apply-damage", function()
        apply_damage(damageAmount)
        update_values()
    end)
    widget:bind_click("debug-health-minus", function()
        set_health(get_health() - HEALTH_STEP)
        update_values()
    end)
    widget:bind_click("debug-health-plus", function()
        set_health(get_health() + HEALTH_STEP)
        update_values()
    end)

    widget:bind_event("debug-titlebar", "mousedown", function(event)
        dragging = true
        dragOffsetX = event.mouse_x - windowX
        dragOffsetY = event.mouse_y - windowY
    end)
    widget:bind_event("debug-titlebar", "mouseup", function()
        dragging = false
    end)
    widget:bind_event("debug-window", "mouseup", function()
        dragging = false
    end)
    widget:bind_event("debug-window", "mousemove", function(event)
        if not dragging then return end
        set_window_position(event.mouse_x - dragOffsetX, event.mouse_y - dragOffsetY)
    end)
end

local function ensure_widget()
    if widget ~= nil then return end

    widget = UI.CreateWidget(WIDGET_PATH)
    if widget == nil then return end

    widget:SetWantsMouse(false)
    bind_events()
    update_values()
    update_tab_visuals()
    set_window_position(windowX, windowY)
end

function InGameDebug.Initialize(config)
    callbacks = config or {}
    zOrder = callbacks.ZOrder or DEFAULT_Z_ORDER
    speedMultiplier = 1.0
    damageAmount = 10.0
    isInvincible = false
    activeTab = "player"
    dragging = false
    apply_speed_multiplier()
end

function InGameDebug.Toggle()
    ensure_widget()
    if widget == nil then return end

    if widget:IsInViewport() then
        close()
    else
        widget:AddToViewportZ(zOrder)
        widget:SetWantsMouse(true)
        update_values()
        update_tab_visuals()
        set_window_position(windowX, windowY)
    end
end

function InGameDebug.Tick()
    apply_speed_multiplier()
    if widget ~= nil and widget:IsInViewport() then
        update_values()
    end
end

function InGameDebug.Shutdown()
    close()
    widget = nil
    callbacks = {}
    dragging = false
end

function InGameDebug.IsInvincible()
    return isInvincible
end

function InGameDebug.GetSpeedMultiplier()
    return speedMultiplier
end

return InGameDebug
