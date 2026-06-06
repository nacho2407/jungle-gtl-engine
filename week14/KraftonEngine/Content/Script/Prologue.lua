local story = nil
local cutsceneWidget = nil
local currentIndex = 0
local entryTime = 0.0
local entryDuration = 0.0
local fadeInDuration = 0.7
local activeEntry = nil

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
    local fontSize = entry.size or story.default_size or 24
    local speakerSize = math.max(16, fontSize * 0.72)
    local textUnits = utf8_visual_units(entry.text or "")
    local speakerUnits = utf8_visual_units(entry.speaker or "")
    local textWidth = textUnits * fontSize
    local speakerWidth = speakerUnits * speakerSize
    local maxBoxWidth = viewportWidth * 0.84
    local width = clamp(math.max(textWidth, speakerWidth) + 84.0, 360.0, maxBoxWidth)
    local textContentWidth = math.max(1.0, width - 84.0)
    local lineCount = math.max(1, math.ceil(textWidth / textContentWidth))
    local lineHeight = fontSize + 10.0
    local height = speakerSize + 30.0 + lineHeight * lineCount

    height = clamp(height, 78.0, viewportHeight * 0.42)
    return width, height, fontSize, speakerSize, lineHeight
end

local function apply_entry(entry)
    if cutsceneWidget == nil or entry == nil then
        return
    end

    local width, height, fontSize, speakerSize, lineHeight = estimate_dialogue_bounds(entry)
    cutsceneWidget:SetText("dialogue-speaker", entry.speaker or "")
    cutsceneWidget:SetText("dialogue-line", entry.text or "")
    cutsceneWidget:SetProperty("dialogue-box", "width", px(width))
    cutsceneWidget:SetProperty("dialogue-box", "height", px(height))
    cutsceneWidget:SetProperty("dialogue-box", "opacity", "0.0")
    cutsceneWidget:SetProperty("dialogue-speaker", "font-family", entry.font or story.default_font or "Pretendard")
    cutsceneWidget:SetProperty("dialogue-speaker", "font-size", px(speakerSize))
    cutsceneWidget:SetProperty("dialogue-speaker", "line-height", px(speakerSize + 6.0))
    cutsceneWidget:SetProperty("dialogue-line", "font-family", entry.font or story.default_font or "Pretendard")
    cutsceneWidget:SetProperty("dialogue-line", "font-size", px(fontSize))
    cutsceneWidget:SetProperty("dialogue-line", "line-height", px(lineHeight))
end

local function show_next_entry()
    currentIndex = currentIndex + 1
    if story == nil or story.entries == nil or currentIndex > #story.entries then
        if Engine.TransitionToScene ~= nil then
            Engine.TransitionToScene(story.next_scene or "Default.Scene")
        end
        return
    end

    activeEntry = story.entries[currentIndex]
    entryTime = 0.0
    entryDuration = activeEntry.duration or story.default_duration or 3.4
    fadeInDuration = activeEntry.fade_in or story.default_fade_in or 0.7
    apply_entry(activeEntry)
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
