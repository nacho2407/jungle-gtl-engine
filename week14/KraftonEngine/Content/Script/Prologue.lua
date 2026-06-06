local story = nil
local cutsceneWidget = nil
local currentIndex = 0
local entryTime = 0.0
local entryDuration = 0.0
local fadeInDuration = 0.7
local activeEntry = nil
local skipHoldTime = 0.0

local SKIP_HOLD_DURATION = 3.0
local SKIP_RING_FRAMES = 24
local DIALOGUE_FONT_SCALE = 0.5

local function px(value)
    return string.format("%.2fpx", value)
end

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function utf8_visual_units(value)
    local units = 0.0
    local i = 1
    local length = string.len(value or "")
    while i <= length do
        local byte = string.byte(value, i)
        local codepoint = 0
        if byte == nil then
            break
        elseif byte < 0x80 then
            codepoint = byte
            i = i + 1
        elseif byte < 0xE0 then
            local b2 = string.byte(value, i + 1) or 0
            codepoint = (byte - 0xC0) * 0x40 + (b2 - 0x80)
            i = i + 2
        elseif byte < 0xF0 then
            local b2 = string.byte(value, i + 1) or 0
            local b3 = string.byte(value, i + 2) or 0
            codepoint = (byte - 0xE0) * 0x1000 + (b2 - 0x80) * 0x40 + (b3 - 0x80)
            i = i + 3
        else
            local b2 = string.byte(value, i + 1) or 0
            local b3 = string.byte(value, i + 2) or 0
            local b4 = string.byte(value, i + 3) or 0
            codepoint = (byte - 0xF0) * 0x40000 + (b2 - 0x80) * 0x1000 + (b3 - 0x80) * 0x40 + (b4 - 0x80)
            i = i + 4
        end

        if codepoint >= 0xAC00 and codepoint <= 0xD7A3 then
            units = units + 1.0
        elseif codepoint >= 0x1100 and codepoint <= 0x11FF then
            units = units + 1.0
        elseif codepoint >= 0x3130 and codepoint <= 0x318F then
            units = units + 1.0
        elseif codepoint >= 0x2E80 and codepoint <= 0x9FFF then
            units = units + 1.0
        elseif codepoint == 0x20 then
            units = units + 0.33
        else
            units = units + 0.58
        end
    end
    return units
end

local function get_viewport_size()
    if Engine.GetViewportSize == nil then
        return 1280.0, 720.0
    end

    local size = Engine.GetViewportSize()
    return size.Width or 1280.0, size.Height or 720.0
end

local function estimate_dialogue_bounds(entry)
    local viewportWidth, viewportHeight = get_viewport_size()
    local fontSize = (entry.size or story.default_size or 24) * DIALOGUE_FONT_SCALE
    local textUnits = utf8_visual_units((entry.speaker or "") .. ": " .. (entry.text or ""))
    local textWidth = textUnits * fontSize
    local maxBoxWidth = viewportWidth * 0.68
    local width = clamp(textWidth + 32.0, 220.0, maxBoxWidth)
    local textContentWidth = math.max(1.0, width - 32.0)
    local lineCount = math.max(1, math.ceil(textWidth / textContentWidth))
    local lineHeight = fontSize + 5.0
    local height = 14.0 + lineHeight * lineCount

    height = clamp(height, 31.0, viewportHeight * 0.18)
    return width, height, fontSize, lineHeight
end

local function get_dialogue_text(entry)
    local speaker = entry.speaker or ""
    local text = entry.text or ""
    if speaker == "" then
        return text
    end
    return speaker .. ": " .. text
end

local function update_skip_ring(progress)
    if cutsceneWidget == nil then return end

    local clamped = clamp(progress or 0.0, 0.0, 1.0)
    cutsceneWidget:SetProperty("skip-ring", "opacity", clamped > 0.001 and "1.0" or "0.0")

    local activeFrame = 0
    if clamped > 0.0 then
        activeFrame = math.ceil(clamped * SKIP_RING_FRAMES)
    end
    activeFrame = clamp(activeFrame, 0, SKIP_RING_FRAMES)
    for index = 0, SKIP_RING_FRAMES do
        local id = string.format("skip-ring-frame-%02d", index)
        cutsceneWidget:SetProperty(id, "opacity", index == activeFrame and "1.0" or "0.0")
    end
end

local function go_to_next_scene()
    if Engine.TransitionToScene ~= nil then
        Engine.TransitionToScene(story.next_scene or "Default.Scene")
    end
end

local function apply_entry(entry)
    if cutsceneWidget == nil or entry == nil then
        return
    end

    local viewportWidth, viewportHeight = get_viewport_size()
    local width, height, fontSize, lineHeight = estimate_dialogue_bounds(entry)
    local left = (viewportWidth - width) * 0.5
    local bottom = viewportHeight * 0.135 + 18.0

    cutsceneWidget:SetText("dialogue-line", get_dialogue_text(entry))
    cutsceneWidget:SetProperty("dialogue-box", "left", px(left))
    cutsceneWidget:SetProperty("dialogue-box", "bottom", px(bottom))
    cutsceneWidget:SetProperty("dialogue-box", "width", px(width))
    cutsceneWidget:SetProperty("dialogue-box", "height", px(height))
    cutsceneWidget:SetProperty("dialogue-box", "opacity", "0.0")
    cutsceneWidget:SetProperty("dialogue-line", "font-family", entry.font or story.default_font or "Pretendard")
    cutsceneWidget:SetProperty("dialogue-line", "font-size", px(fontSize))
    cutsceneWidget:SetProperty("dialogue-line", "font-weight", tostring(entry.weight or story.default_weight or 400))
    cutsceneWidget:SetProperty("dialogue-line", "line-height", px(lineHeight))

    cutsceneWidget:SetProperty("skip-ring", "right", "54px")
    cutsceneWidget:SetProperty("skip-ring", "bottom", px(bottom + 6.0))
end

local function show_next_entry()
    currentIndex = currentIndex + 1
    if story == nil or story.entries == nil or currentIndex > #story.entries then
        go_to_next_scene()
        return
    end

    activeEntry = story.entries[currentIndex]
    entryTime = 0.0
    entryDuration = activeEntry.duration or story.default_duration or 3.4
    fadeInDuration = activeEntry.fade_in or story.default_fade_in or 0.7
    apply_entry(activeEntry)
end

local function should_advance_dialogue()
    if Input.GetKeyDown(Key.MouseLeft) then return true end
    if Input.GetKeyDown(Key.Space) then return true end
    if Key.Enter ~= nil and Input.GetKeyDown(Key.Enter) then return true end
    return false
end

local function update_scene_skip(dt)
    if Key.Ctrl == nil then return false end

    if Input.GetKey(Key.Ctrl) then
        skipHoldTime = skipHoldTime + dt
        local progress = clamp(skipHoldTime / SKIP_HOLD_DURATION, 0.0, 1.0)
        update_skip_ring(progress)
        if skipHoldTime >= SKIP_HOLD_DURATION then
            go_to_next_scene()
            return true
        end
    else
        skipHoldTime = 0.0
        update_skip_ring(0.0)
    end

    return false
end

local function play_story(moduleName)
    story = require(moduleName)
    currentIndex = 0
    activeEntry = nil
    show_next_entry()
end

function BeginPlay()
    cutsceneWidget = UI.CreateWidget("Content/UI/Cutscene/Cutscene.rml")
    if cutsceneWidget ~= nil then
        cutsceneWidget:AddToViewportZ(120)
        update_skip_ring(0.0)
    end
    play_story("Dialogue/Prologue.dialogue")
end

function EndPlay()
    if cutsceneWidget ~= nil and cutsceneWidget:IsInViewport() then
        cutsceneWidget:RemoveFromParent()
    end
    cutsceneWidget = nil
    story = nil
end

function Tick(dt)
    if cutsceneWidget == nil or activeEntry == nil then
        return
    end

    if update_scene_skip(dt) then
        return
    end

    if should_advance_dialogue() then
        show_next_entry()
        return
    end

    entryTime = entryTime + dt
    local alpha = 1.0
    if fadeInDuration > 0.0 then
        alpha = clamp(entryTime / fadeInDuration, 0.0, 1.0)
    end
    cutsceneWidget:SetProperty("dialogue-box", "opacity", string.format("%.2f", alpha))

    if entryTime >= entryDuration then
        show_next_entry()
    end
end
