local TutorialSystem = require("Tutorial.TutorialSystem")

local triggered = false

local TAG_TO_GROUP = {
    ["tutorial.move"] = "move",
    ["tutorial.jump"] = "jump",
    ["tutorial.slide"] = "slide",
    ["tutorial.weapon"] = "weapon",
    ["tutorial.reload"] = "reload",
    ["tutorial.wallrun"] = "wallrun",

    ["tutorial_move"] = "move",
    ["tutorial_jump"] = "jump",
    ["tutorial_slide"] = "slide",
    ["tutorial_weapon"] = "weapon",
    ["tutorial_reload"] = "reload",
    ["tutorial_wallrun"] = "wallrun",

    ["tutorial.full"] = "__full",
    ["tutorial.start"] = "__full",
    ["tutorial.level1"] = "__full",
}

local function normalize(value)
    if value == nil then return "" end
    return string.lower(tostring(value))
end

local function resolve_tutorial_mode()
    if obj ~= nil and type(obj.GetTags) == "function" then
        local tags = obj:GetTags()
        if tags ~= nil then
            for _, tag in ipairs(tags) do
                local group = TAG_TO_GROUP[normalize(tag)]
                if group ~= nil then
                    return group
                end
            end
        end
    end

    local name = normalize(obj ~= nil and obj.Name or "")
    for tag, group in pairs(TAG_TO_GROUP) do
        if string.find(name, tag, 1, true) ~= nil then
            return group
        end
    end

    return "__full"
end

local function is_player_actor(actor)
    if actor == nil then return false end

    local ok, movement = pcall(function()
        return actor:GetCharacterMovement()
    end)
    if not ok then return false end
    if movement == nil then return false end

    local hasPlayerTag = false
    local tagOk, tagResult = pcall(function()
        return actor:HasTag("player")
    end)
    if tagOk then
        hasPlayerTag = tagResult == true
    end
    if hasPlayerTag then
        return true
    end

    local possessedOk, possessed = pcall(function()
        return actor:IsPossessed()
    end)
    if possessedOk then
        return possessed == true
    end

    return false
end

function BeginPlay()
    triggered = false
end

function EndPlay()
end

function OnOverlap(OtherActor)
    if triggered then return end
    if not is_player_actor(OtherActor) then return end

    local mode = resolve_tutorial_mode()
    local isFullSequence = mode == "__full"
    if TutorialSystem.IsRunning ~= nil and TutorialSystem.IsRunning() then
        TutorialSystem.Shutdown()
    end

    triggered = true
    TutorialSystem.Initialize({
        owner = OtherActor,
        movement = OtherActor:GetCharacterMovement(),
        startGroupId = isFullSequence and "move" or mode,
        continueToNextGroup = isFullSequence,
        playIntro = isFullSequence,
        overlayZOrder = 104,
        dialogueZOrder = 106,
    })
end

function Tick(dt)
end
