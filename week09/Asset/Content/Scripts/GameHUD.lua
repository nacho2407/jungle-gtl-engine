local ui_document = nil

local hp_bar_width = 180
local max_hp_fallback = 100.0

local function clamp(value, min_value, max_value)
    if value < min_value then
        return min_value
    end

    if value > max_value then
        return max_value
    end

    return value
end

local function getPlayerState()
    return _G.PlayerState or {}
end

local function getTimer()
    if _G.GameManager ~= nil and _G.GameManager.GetTimer ~= nil then
        return _G.GameManager.GetTimer()
    end

    return 0.0
end

local function formatTimer(total_seconds)
    total_seconds = math.floor((total_seconds or 0.0) + 0.5)
    local minutes = math.floor(total_seconds / 60)
    local seconds = total_seconds % 60

    return string.format("%02d:%02d", minutes, seconds)
end

local function setHudText(element_id, text, red, green, blue, font_size, center_width)
    ui_document:SetTextStyle(element_id, text, red, green, blue, font_size, true, center_width)
end

local function refreshHud()
    if ui_document == nil or not ui_document:IsValid() then
        return
    end

    local player_state = getPlayerState()
    local hp = player_state.HP or 0.0
    local max_hp = player_state.MaxHP or max_hp_fallback
    local hp_ratio = 0.0
    if max_hp > 0.0 then
        hp_ratio = clamp(hp / max_hp, 0.0, 1.0)
    end

    ui_document:SetProperty("hp_bar_fill", "width", tostring(math.floor(hp_bar_width * hp_ratio + 0.5)) .. "px")
    setHudText("document_count_text", "x " .. tostring(player_state.DocumentCount or 0), 255, 255, 255, 24, 100)
    setHudText("timer_text", formatTimer(getTimer()), 255, 255, 255, 32, 220)
end

function BeginPlay()
    ui_document = UI.Load("UI/GameHUD.rml", "game_hud")

    if ui_document == nil or not ui_document:IsValid() then
        return
    end

    ui_document:SetPosition(0, 0)
    ui_document:SetZOrder(900)
    ui_document:SetTexture("battery_icon", "Textures/Battery.png")
    ui_document:SetTexture("document_icon", "Textures/Document.png")
    ui_document:Show()

    refreshHud()
end

function EndPlay()
    if ui_document ~= nil and ui_document:IsValid() then
        ui_document:Close()
    end

    ui_document = nil
end

function Tick(dt)
    refreshHud()
end
