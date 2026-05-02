local UIManager = require("UIManager")
local ObjRegistry = require("ObjRegistry")
local gameState = nil
local HandleQuestPhaseChanged = nil

local function OnPhaseChanged(phase)
    print("Phase changed: " .. tostring(phase))

    if HandleQuestPhaseChanged ~= nil then
        HandleQuestPhaseChanged(phase)
    end

    if phase == ECarGamePhase.None then
        return
    end

    if phase == ECarGamePhase.CarWash then
        UIManager.FadeOut(0.5, function()
            CameraManager.PossessCamera(ObjRegistry.manCamera)

            UIManager.FadeIn(0.5)
        end)
        print("Run Lua logic for CarWash")
    elseif phase == ECarGamePhase.CarGas then
        print("Run Lua logic for CarGas")
    elseif phase == ECarGamePhase.EscapePolice then
        print("Run Lua logic for EscapePolice")
    elseif phase == ECarGamePhase.DodgeMeteor then
        print("Run Lua logic for DodgeMeteor")
    elseif phase == ECarGamePhase.Finished then
        print("Run Lua logic for Finished")
    end
end

local QuestState = {
    NotStarted = 0,
    WaitingAccept = 1,
    Active = 2,
    Completed = 3,
    AllCompleted = 4
}

local quests = {
    {
        id = "carWash",
        uiKey = "carWashQuest",
        targetName = "CarWashCenter",
        fallbackTargetName = "CarWashCenterTrigger",
        completionPhase = ECarGamePhase.CarWash
    },
    {
        id = "gas",
        uiKey = "gasQuest",
        targetName = "GasStation",
        fallbackTargetName = "CarWashTrigger",
        completionPhase = ECarGamePhase.CarGas
    }
}

local state = QuestState.NotStarted
local currentQuestIndex = 0
local car = nil
local activeTarget = nil

local function EnsureCar()
    if car ~= nil and car:IsValid() then
        return car
    end

    car = World.FindFirstActorByClass("ACarPawn")
    return car
end

local function GetQuestText(quest)
    if quest == nil then
        return "-"
    end

    if quest.id == "carWash" then
        return "세차하기"
    elseif quest.id == "gas" then
        return "주유하기"
    end

    return quest.id
end

local function GetArrowSymbolFromAngle(angle)
    if angle >= -22.5 and angle < 22.5 then
        return "&#8593;"
    elseif angle >= 22.5 and angle < 67.5 then
        return "&#8598;"
    elseif angle >= 67.5 and angle < 112.5 then
        return "&#8592;"
    elseif angle >= 112.5 and angle < 157.5 then
        return "&#8601;"
    elseif angle >= -67.5 and angle < -22.5 then
        return "&#8599;"
    elseif angle >= -112.5 and angle < -67.5 then
        return "&#8594;"
    elseif angle >= -157.5 and angle < -112.5 then
        return "&#8600;"
    end

    return "&#8595;"
end

local function GetCurrentQuest()
    return quests[currentQuestIndex]
end

local function StartCurrentQuest()
    local quest = GetCurrentQuest()
    if quest == nil then
        state = QuestState.AllCompleted
        UIManager.SetQuestHud("-", "&#8593;", false)
        return
    end

    activeTarget = World.FindActorByName(quest.targetName)
    if activeTarget == nil and quest.fallbackTargetName ~= nil then
        activeTarget = World.FindActorByName(quest.fallbackTargetName)
    end

    if activeTarget == nil then
        print("Quest target not found:", quest.targetName, quest.fallbackTargetName)
        return
    end

    if EnsureCar() == nil then
        print("Quest car actor not found.")
        return
    end

    UIManager.SetQuestHud(GetQuestText(quest), "&#8593;", true)
    state = QuestState.Active
end

local function ShowQuest(index)
    currentQuestIndex = index
    activeTarget = nil
    UIManager.SetQuestHud("-", "&#8593;", false)

    local quest = GetCurrentQuest()
    if quest == nil then
        state = QuestState.AllCompleted
        return
    end

    UIManager.Show(quest.uiKey)
    state = QuestState.WaitingAccept
end

local function CompleteCurrentQuest()
    local quest = GetCurrentQuest()
    if quest ~= nil then
        print("Quest completed:", quest.id)
    end

    state = QuestState.Completed
    UIManager.SetQuestHud("-", "&#8593;", false)

    if currentQuestIndex < #quests then
        ShowQuest(currentQuestIndex + 1)
    else
        state = QuestState.AllCompleted
        UIManager.SetQuestHud("완료", "&#8593;", false)
        print("All quests completed.")
    end
end

local function UpdateQuestHud()
    local ownerCar = EnsureCar()
    if ownerCar == nil or activeTarget == nil then
        UIManager.SetQuestHud("-", "&#8593;", false)
        return
    end

    local toTarget = activeTarget.Location - ownerCar.Location
    -- toTarget.Z = 0.0
    local toTarget2D = Vector.new(toTarget.X, toTarget.Y, 0.0)

    if toTarget2D:Length() <= 1.0 then
        UIManager.SetQuestHud(GetQuestText(GetCurrentQuest()), "&#8593;", true)
        return
    end

    local direction = toTarget2D:Normalized()
    local rawForward = ownerCar.Forward
    --forward.Z = 0.0
    local forward = Vector.new(rawForward.X, rawForward.Y, 0.0)

    if forward:Length() <= 1.0 then
        UIManager.SetQuestHud(GetQuestText(GetCurrentQuest()), "&#8593;", true)
        return
    end

    forward = forward:Normalized()

    local dot = forward:Dot(direction)
    if dot > 1.0 then dot = 1.0 end
    if dot < -1.0 then dot = -1.0 end

    local crossZ = forward.X * direction.Y - forward.Y * direction.X
    local angle = math.atan(crossZ, dot) * 180.0 / 3.14159265
    UIManager.SetQuestHud(GetQuestText(GetCurrentQuest()), GetArrowSymbolFromAngle(angle), true)
end

HandleQuestPhaseChanged = function(phase)
    local quest = GetCurrentQuest()
    if state ~= QuestState.Active or quest == nil then
        return
    end

    if phase == quest.completionPhase then
        CompleteCurrentQuest()
    end
end

function BeginPlay()
    UIManager.Init()
    gameState = GetGameState()
    if gameState ~= nil then
        gameState:BindPhaseChanged(OnPhaseChanged)
    end

    UIManager.SetStartGameCallback(function()
        print("Start Game!")
        UIManager.Show("gameOverlay")
        ShowQuest(1)
    end)
    UIManager.SetCarWashQuestOkCallback(function()
        if currentQuestIndex == 1 and state == QuestState.WaitingAccept then
            StartCurrentQuest()
        end
    end)
    UIManager.SetGasQuestOkCallback(function()
        if currentQuestIndex == 2 and state == QuestState.WaitingAccept then
            StartCurrentQuest()
        end
    end)

    UIManager.Show("intro")
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    UIManager.Tick(dt)
    UIManager.UpdateHUD()
    if state ~= QuestState.Active then
        return
    end

    UpdateQuestHud()
end
