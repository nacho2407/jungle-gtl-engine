local story = nil
local cutsceneWidget = nil
local currentIndex = 0
local entryTime = 0.0
local entryDuration = 0.0
local fadeInDuration = 0.7
local activeEntry = nil
local skipHoldTime = 0.0
local voiceManifest = nil
local voiceEntries = nil
local voiceEntriesById = nil
local loadedVoiceKeys = {}

local SKIP_HOLD_DURATION = 3.0
local SKIP_RING_FRAMES = 24
local DIALOGUE_BOX_WIDTH = 980.0
local DIALOGUE_BOX_HEIGHT = 48.0
local DIALOGUE_LINE_HEIGHT = 48.0
local DIALOGUE_TEXT_LEFT = 16.0
local DIALOGUE_DEFAULT_FONT_SIZE = 18.0

local function px(value)
    return string.format("%.2fpx", value)
end

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function get_viewport_size()
    if Engine.GetViewportSize == nil then
        return 1280.0, 720.0
    end

    local size = Engine.GetViewportSize()
    return size.Width or 1280.0, size.Height or 720.0
end

local function get_dialogue_font_size(entry)
    if entry ~= nil and entry.size ~= nil then
        return entry.size
    end
    if story ~= nil and story.default_size ~= nil then
        return story.default_size
    end
    return DIALOGUE_DEFAULT_FONT_SIZE
end

local function estimate_dialogue_bounds(entry)
    return DIALOGUE_BOX_WIDTH, DIALOGUE_BOX_HEIGHT, get_dialogue_font_size(entry), DIALOGUE_LINE_HEIGHT
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

local function load_voice_manifest(moduleName)
    voiceManifest = nil
    voiceEntries = nil
    voiceEntriesById = nil
    loadedVoiceKeys = {}

    local ok, result = pcall(require, moduleName)
    if not ok or result == nil or result.entries == nil then
        return
    end

    voiceManifest = result
    voiceEntries = {}
    voiceEntriesById = result.by_id or {}
    for _, voiceEntry in ipairs(result.entries) do
        if voiceEntry.index ~= nil then
            voiceEntries[voiceEntry.index] = voiceEntry
        end
        if voiceEntry.id ~= nil then
            voiceEntriesById[voiceEntry.id] = voiceEntry
        end
    end
end

local function get_dialogue_entry_id(entry, index)
    if entry ~= nil and entry.id ~= nil then
        return entry.id
    end
    return string.format("%s_%04d", story.id or "Dialogue", index)
end

local function play_dialogue_voice(entry, index)
    if voiceEntries == nil or AudioManager == nil then return end

    local entryId = get_dialogue_entry_id(entry, index)
    local voiceEntry = nil
    if voiceEntriesById ~= nil then
        voiceEntry = voiceEntriesById[entryId]
    end
    if voiceEntry == nil then
        voiceEntry = voiceEntries[index]
    end
    if voiceEntry == nil or voiceEntry.key == nil or voiceEntry.path == nil then return end

    if not loadedVoiceKeys[voiceEntry.key] then
        local loaded = AudioManager.Load(voiceEntry.key, voiceEntry.path, false)
        if not loaded then return end
        loadedVoiceKeys[voiceEntry.key] = true
    end

    AudioManager.Play(voiceEntry.key, voiceEntry.volume or 1.0)
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
    cutsceneWidget:SetProperty("dialogue-line", "left", px(DIALOGUE_TEXT_LEFT))
    cutsceneWidget:SetProperty("dialogue-line", "top", "0px")
    cutsceneWidget:SetProperty("dialogue-line", "width", px(width - DIALOGUE_TEXT_LEFT * 2.0))
    cutsceneWidget:SetProperty("dialogue-line", "height", px(height))
    cutsceneWidget:SetProperty("dialogue-line", "font-family", entry.font or story.default_font or "Pretendard")
    cutsceneWidget:SetProperty("dialogue-line", "font-size", px(fontSize))
    cutsceneWidget:SetProperty("dialogue-line", "font-weight", tostring(entry.weight or story.default_weight or 400))
    cutsceneWidget:SetProperty("dialogue-line", "line-height", px(lineHeight))
    cutsceneWidget:SetProperty("dialogue-line", "text-align", "left")

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
    play_dialogue_voice(activeEntry, currentIndex)
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
    load_voice_manifest("Dialogue/Generated/" .. (story.id or "Prologue") .. ".voices")
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
    voiceManifest = nil
    voiceEntries = nil
    voiceEntriesById = nil
    loadedVoiceKeys = {}
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
