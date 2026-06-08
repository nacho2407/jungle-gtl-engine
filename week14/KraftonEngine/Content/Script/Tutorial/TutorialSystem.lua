local TutorialSystem = {}

local OVERLAY_PATH = "Content/UI/Tutorial/TutorialOverlay.rml"
local DIALOGUE_PATH = "Content/UI/Common/DialogueBox.rml"
local STORY_MODULE = "Dialogue/TutorialLevel1.dialogue"
local VOICE_MODULE = "Dialogue/Generated/TutorialLevel1.voices"
local OVERLAY_Z_ORDER = 105
local DIALOGUE_Z_ORDER = 106

local overlayWidget = nil
local dialogueWidget = nil
local movement = nil
local owner = nil
local initialized = false
local currentStepIndex = 1
local dialogueStory = nil
local dialogueEntriesById = {}
local voiceEntriesById = nil
local loadedVoiceKeys = {}
local currentVoiceKey = nil
local activeDialogue = nil
local dialogueTimer = 0.0
local dialogueDuration = 0.0
local dialogueQueue = {}
local continueToNextGroup = true
local hasSeenWallRun = false
local completionHideTimer = 0.0
local objectivesStarted = false
local refresh_overlay = nil
local enemyKillDialoguePlayed = false

local objectiveGroups = {
    {
        id = "move",
        startDialogue = "TutorialLevel1_System_MobilityCheck",
        completeDialogue = nil,
        items = {
            { id = "move_w", key = "W", text = "앞으로 이동" },
            { id = "move_a", key = "A", text = "왼쪽으로 이동" },
            { id = "move_s", key = "S", text = "뒤로 이동" },
            { id = "move_d", key = "D", text = "오른쪽으로 이동" },
            { id = "sprint", key = "Shift", text = "달리기" },
        },
    },
    {
        id = "jump",
        startDialogue = "TutorialLevel1_System_JumpCheck",
        completeDialogue = nil,
        items = {
            { id = "jump", key = "Space", text = "점프" },
            { id = "double_jump", key = "Space", text = "공중 추진" },
        },
    },
    {
        id = "slide",
        startDialogue = "TutorialLevel1_System_SlideCheck",
        completeDialogue = nil,
        items = {
            { id = "crouch", key = "Ctrl", text = "저자세 이동" },
            { id = "slide", key = "Ctrl", text = "가속 중 슬라이딩" },
        },
    },
    {
        id = "weapon",
        startDialogue = "TutorialLevel1_System_WeaponCheck",
        completeDialogue = "TutorialLevel1_System_EnemyContact",
        items = {
            { id = "fire", key = "LMB", text = "사격" },
            { id = "zoom", key = "RMB", text = "조준 확대" },
        },
    },
    {
        id = "reload",
        startDialogue = "TutorialLevel1_System_ReloadCheck",
        completeDialogue = nil,
        items = {
            { id = "reload", key = "R", text = "재장전" },
        },
    },
    {
        id = "wallrun",
        startDialogue = "TutorialLevel1_System_WallRunCheck",
        completeDialogue = "TutorialLevel1_System_TrainingComplete",
        items = {
            { id = "wallrun", key = "이동", text = "수직면 기동" },
            { id = "walljump", key = "Space", text = "수직면 이탈" },
        },
    },
}

local function find_group_index(groupId)
    if groupId == nil or groupId == "" then return 1 end
    local lowered = string.lower(groupId)
    for index, group in ipairs(objectiveGroups) do
        if string.lower(group.id) == lowered then
            return index
        end
    end
    return 1
end

local function px(value)
    return string.format("%.2fpx", value)
end

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function play_event(name)
    if AudioManager == nil then return end
    if AudioManager.PlayOneShot ~= nil then
        AudioManager.PlayOneShot(name)
    elseif AudioManager.PlayEvent ~= nil then
        AudioManager.PlayEvent(name)
    end
end

local function find_dialogue_entry(id)
    if id == nil then return nil end
    return dialogueEntriesById[id]
end

local function get_dialogue_text(entry)
    if entry == nil then return "" end
    local speaker = entry.speaker or ""
    local text = entry.text or entry.text_en or ""
    if speaker == "" then
        return text
    end
    return speaker .. ": " .. text
end

local function get_voice_entry(entry)
    if entry == nil or voiceEntriesById == nil then return nil end
    return voiceEntriesById[entry.id]
end

local function stop_current_voice()
    if currentVoiceKey ~= nil and AudioManager ~= nil and AudioManager.Stop ~= nil then
        AudioManager.Stop(currentVoiceKey)
    end
    currentVoiceKey = nil
end

local function play_voice(entry)
    local voiceEntry = get_voice_entry(entry)
    if voiceEntry == nil or voiceEntry.key == nil or voiceEntry.path == nil then return nil end
    if AudioManager == nil or AudioManager.Load == nil or AudioManager.Play == nil then return nil end

    stop_current_voice()

    if not loadedVoiceKeys[voiceEntry.key] then
        if not AudioManager.Load(voiceEntry.key, voiceEntry.path, false) then
            return nil
        end
        loadedVoiceKeys[voiceEntry.key] = true
    end

    AudioManager.Play(voiceEntry.key, voiceEntry.volume or 1.0)
    currentVoiceKey = voiceEntry.key
    return voiceEntry
end

local function show_dialogue(id)
    if dialogueWidget == nil then return end
    local entry = find_dialogue_entry(id)
    if entry == nil then return end

    local fontSize = entry.size or dialogueStory.default_size or 22
    local lineHeight = fontSize + 26
    local text = get_dialogue_text(entry)
    local width = clamp(42.0 + string.len(text) * fontSize * 0.52, 520.0, 1280.0)
    local height = clamp(lineHeight, 46.0, 68.0)

    dialogueWidget:SetText("dialogue-line", text)
    dialogueWidget:SetProperty("dialogue-layer", "display", "block")
    dialogueWidget:SetProperty("dialogue-box", "display", "block")
    dialogueWidget:SetProperty("dialogue-box", "left", px(24.0))
    dialogueWidget:SetProperty("dialogue-box", "bottom", px(28.0))
    dialogueWidget:SetProperty("dialogue-box", "width", px(width))
    dialogueWidget:SetProperty("dialogue-box", "height", px(height))
    dialogueWidget:SetProperty("dialogue-box", "opacity", "0.0")
    dialogueWidget:SetProperty("dialogue-line", "left", "16px")
    dialogueWidget:SetProperty("dialogue-line", "top", "0px")
    dialogueWidget:SetProperty("dialogue-line", "width", px(width - 32.0))
    dialogueWidget:SetProperty("dialogue-line", "height", px(height))
    dialogueWidget:SetProperty("dialogue-line", "font-family", entry.font or dialogueStory.default_font or "Pretendard")
    dialogueWidget:SetProperty("dialogue-line", "font-size", px(fontSize))
    dialogueWidget:SetProperty("dialogue-line", "font-weight", tostring(entry.weight or dialogueStory.default_weight or 700))
    dialogueWidget:SetProperty("dialogue-line", "line-height", px(height))

    local voiceEntry = play_voice(entry)
    activeDialogue = entry
    dialogueTimer = 0.0
    dialogueDuration = (voiceEntry ~= nil and voiceEntry.duration ~= nil) and (voiceEntry.duration + 0.35) or (entry.duration or dialogueStory.default_duration or 3.2)
end

local function enqueue_dialogue(id)
    if id == nil then return end
    if activeDialogue == nil then
        show_dialogue(id)
        return
    end
    table.insert(dialogueQueue, id)
end

local function show_objectives()
    objectivesStarted = true
    if overlayWidget ~= nil then
        overlayWidget:SetProperty("tutorial-panel", "display", "block")
        overlayWidget:SetProperty("tutorial-panel", "opacity", "1.0")
    end
    play_event("tutorial.objective.appear")
    local group = objectiveGroups[currentStepIndex]
    if group ~= nil and group.startDialogue ~= nil then
        enqueue_dialogue(group.startDialogue)
    end
    refresh_overlay()
end

local function update_dialogue(dt)
    if dialogueWidget == nil or activeDialogue == nil then return end

    dialogueTimer = dialogueTimer + dt
    local fadeIn = activeDialogue.fade_in or dialogueStory.default_fade_in or 0.18
    local fadeOut = 0.35
    local alpha = 1.0
    if dialogueTimer < fadeIn then
        alpha = dialogueTimer / fadeIn
    elseif dialogueTimer > dialogueDuration - fadeOut then
        alpha = (dialogueDuration - dialogueTimer) / fadeOut
    end
    alpha = clamp(alpha, 0.0, 1.0)
    dialogueWidget:SetProperty("dialogue-box", "opacity", string.format("%.2f", alpha))

    if dialogueTimer >= dialogueDuration then
        dialogueWidget:SetProperty("dialogue-box", "opacity", "0.0")
        dialogueWidget:SetProperty("dialogue-box", "display", "none")
        dialogueWidget:SetText("dialogue-line", "")
        stop_current_voice()
        activeDialogue = nil
        if #dialogueQueue > 0 then
            local nextDialogue = table.remove(dialogueQueue, 1)
            show_dialogue(nextDialogue)
        elseif not objectivesStarted then
            show_objectives()
        end
    end
end

local function set_step_visual(index, item, status)
    if overlayWidget == nil then return end

    local base = "tutorial-step-" .. tostring(index)
    overlayWidget:SetProperty(base, "display", item ~= nil and "block" or "none")
    if item == nil then
        return
    end

    overlayWidget:SetText(base .. "-key", item.key or "")
    overlayWidget:SetText(base .. "-text", item.text or "")
    if status == "done" then
        overlayWidget:SetProperty(base, "opacity", "0.44")
        overlayWidget:SetText(base .. "-check", "✓")
        overlayWidget:SetProperty(base .. "-check", "border", "1px #f0c936")
    elseif status == "active" then
        overlayWidget:SetProperty(base, "opacity", "1.0")
        overlayWidget:SetText(base .. "-check", "")
        overlayWidget:SetProperty(base .. "-check", "border", "1px #ffffff")
    else
        overlayWidget:SetProperty(base, "opacity", "0.58")
        overlayWidget:SetText(base .. "-check", "")
        overlayWidget:SetProperty(base .. "-check", "border", "1px #777777")
    end
end

refresh_overlay = function()
    if overlayWidget == nil then return end
    if not objectivesStarted then
        overlayWidget:SetProperty("tutorial-panel", "display", "none")
        return
    end

    local group = objectiveGroups[currentStepIndex]
    for i = 1, 6 do
        local item = group ~= nil and group.items ~= nil and group.items[i] or nil
        if item == nil then
            set_step_visual(i, nil, "hidden")
        elseif item.completed then
            set_step_visual(i, item, "done")
        else
            set_step_visual(i, item, "active")
        end
    end
end

local function is_group_complete(group)
    if group == nil or group.items == nil then return false end
    for _, item in ipairs(group.items) do
        if not item.completed then
            return false
        end
    end
    return true
end

local function mark_item(group, itemId)
    if group == nil or group.items == nil then return end
    for _, item in ipairs(group.items) do
        if item.id == itemId then
            if not item.completed then
                play_event("tutorial.objective.complete")
            end
            item.completed = true
            return
        end
    end
end

local function advance_if_needed()
    local group = objectiveGroups[currentStepIndex]
    if group == nil or group.completed or not is_group_complete(group) then
        return
    end

    group.completed = true
    if group.completeDialogue ~= nil then
        enqueue_dialogue(group.completeDialogue)
    end

    local nextGroup = continueToNextGroup and objectiveGroups[currentStepIndex + 1] or nil
    if nextGroup ~= nil then
        currentStepIndex = currentStepIndex + 1
        play_event("tutorial.objective.appear")
        if nextGroup.startDialogue ~= nil then
            enqueue_dialogue(nextGroup.startDialogue)
        end
    else
        completionHideTimer = 4.0
    end
    refresh_overlay()
end

local function update_current_step()
    if not objectivesStarted then return end
    local group = objectiveGroups[currentStepIndex]
    if group == nil then return end

    local speed = 0.0
    local isSprinting = false
    local isCrouching = false
    local isWalking = false
    local isWallRunning = false
    local didAirJump = false

    if movement ~= nil then
        if movement.GetSpeed ~= nil then speed = movement:GetSpeed() end
        if movement.IsSprinting ~= nil then isSprinting = movement:IsSprinting() end
        if movement.IsCrouching ~= nil then isCrouching = movement:IsCrouching() end
        if movement.IsWalking ~= nil then isWalking = movement:IsWalking() end
        if movement.IsWallRunning ~= nil then isWallRunning = movement:IsWallRunning() end
        if movement.WasAirJumpConsumedThisFrame ~= nil then didAirJump = movement:WasAirJumpConsumedThisFrame() end
    end

    if group.id == "move" then
        if Input.GetKey(Key.W) and speed > 0.25 then mark_item(group, "move_w") end
        if Input.GetKey(Key.A) and speed > 0.25 then mark_item(group, "move_a") end
        if Input.GetKey(Key.S) and speed > 0.25 then mark_item(group, "move_s") end
        if Input.GetKey(Key.D) and speed > 0.25 then mark_item(group, "move_d") end
        if isSprinting or (Input.GetKey(Key.Shift) and speed > 0.5) then
            mark_item(group, "sprint")
        end
    elseif group.id == "jump" then
        if Input.GetKeyDown(Key.Space) then
            mark_item(group, "jump")
        end
        if didAirJump then
            mark_item(group, "double_jump")
        end
    elseif group.id == "slide" then
        if isCrouching or Input.GetKey(Key.Ctrl) or Input.GetKey(Key.LeftCtrl) or Input.GetKey(Key.RightCtrl) then
            mark_item(group, "crouch")
        end
        if isWalking and isCrouching and speed >= 3.0 then
            mark_item(group, "slide")
        end
    elseif group.id == "weapon" then
        if Input.GetKeyDown(Key.MouseLeft) then
            mark_item(group, "fire")
        end
        if Input.GetKey(Key.MouseRight) then
            mark_item(group, "zoom")
        end
    elseif group.id == "reload" then
        if Input.GetKeyDown(Key.R) then
            mark_item(group, "reload")
        end
    elseif group.id == "wallrun" then
        if isWallRunning then
            hasSeenWallRun = true
            mark_item(group, "wallrun")
        end
        if hasSeenWallRun and Input.GetKeyDown(Key.Space) then
            mark_item(group, "walljump")
        end
    end

    refresh_overlay()
    advance_if_needed()
end

local function load_dialogue_assets()
    dialogueStory = require(STORY_MODULE)
    dialogueEntriesById = {}
    if dialogueStory ~= nil and dialogueStory.entries ~= nil then
        for _, entry in ipairs(dialogueStory.entries) do
            if entry.id ~= nil then
                dialogueEntriesById[entry.id] = entry
            end
        end
    end

    local ok, voices = pcall(require, VOICE_MODULE)
    if ok and voices ~= nil then
        voiceEntriesById = voices.by_id or {}
    else
        voiceEntriesById = {}
    end
end

function TutorialSystem.Initialize(config)
    if initialized then return end
    initialized = true
    config = config or {}
    owner = config.owner
    movement = config.movement
    currentStepIndex = find_group_index(config.startGroupId)
    continueToNextGroup = config.continueToNextGroup ~= false
    hasSeenWallRun = false
    completionHideTimer = 0.0
    objectivesStarted = false
    enemyKillDialoguePlayed = false
    activeDialogue = nil
    dialogueTimer = 0.0
    dialogueDuration = 0.0
    dialogueQueue = {}
    loadedVoiceKeys = {}
    currentVoiceKey = nil
    dialogueEntriesById = {}

    for _, group in ipairs(objectiveGroups) do
        group.completed = false
        if group.items ~= nil then
            for _, item in ipairs(group.items) do
                item.completed = false
            end
        end
    end

    load_dialogue_assets()

    overlayWidget = UI.CreateWidget(OVERLAY_PATH)
    if overlayWidget ~= nil then
        overlayWidget:AddToViewportZ(config.overlayZOrder or OVERLAY_Z_ORDER)
    end

    dialogueWidget = UI.CreateWidget(DIALOGUE_PATH)
    if dialogueWidget ~= nil then
        dialogueWidget:AddToViewportZ(config.dialogueZOrder or DIALOGUE_Z_ORDER)
        dialogueWidget:SetText("dialogue-line", "")
        dialogueWidget:SetProperty("dialogue-box", "opacity", "0.0")
        dialogueWidget:SetProperty("dialogue-box", "display", "none")
    end

    refresh_overlay()
    if config.playIntro ~= false then
        enqueue_dialogue("TutorialLevel1_System_LandingProtocol")
    else
        show_objectives()
    end
end

function TutorialSystem.Shutdown()
    stop_current_voice()
    if overlayWidget ~= nil and overlayWidget:IsInViewport() then
        overlayWidget:RemoveFromParent()
    end
    if dialogueWidget ~= nil and dialogueWidget:IsInViewport() then
        dialogueWidget:RemoveFromParent()
    end
    overlayWidget = nil
    dialogueWidget = nil
    movement = nil
    owner = nil
    activeDialogue = nil
    dialogueStory = nil
    dialogueEntriesById = {}
    voiceEntriesById = nil
    loadedVoiceKeys = {}
    currentVoiceKey = nil
    dialogueQueue = {}
    continueToNextGroup = true
    objectivesStarted = false
    enemyKillDialoguePlayed = false
    initialized = false
end

function TutorialSystem.IsRunning()
    return initialized
end

function TutorialSystem.Tick(dt)
    if not initialized then return end
    dt = dt or 0.0

    update_dialogue(dt)
    if completionHideTimer > 0.0 then
        completionHideTimer = completionHideTimer - dt
        if completionHideTimer <= 0.0 and overlayWidget ~= nil then
            overlayWidget:SetProperty("tutorial-panel", "opacity", "0.0")
            TutorialSystem.Shutdown()
        end
        return
    end

    update_current_step()
end

function TutorialSystem.NotifyEnemyKilled()
    if initialized and objectivesStarted and not enemyKillDialoguePlayed then
        enemyKillDialoguePlayed = true
        enqueue_dialogue("TutorialLevel1_System_BioSignalLost")
    end
end

return TutorialSystem
