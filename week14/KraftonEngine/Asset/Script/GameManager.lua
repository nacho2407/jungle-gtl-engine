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
local arrow = nil
local activeTarget = nil

local function EnsureCar()
    if car ~= nil and car:IsValid() then
        return car
    end

    car = World.FindFirstActorByClass("ACarPawn")
    return car
end

local function EnsureArrow()
    local ownerCar = EnsureCar()
    if ownerCar == nil then
        return nil
    end

    if arrow == nil then
        arrow = ownerCar:AddQuestArrowComponent()
    end

    arrow:SetVisibility(true)
    return arrow
end

local function HideArrow()
    if arrow ~= nil then
        arrow:SetVisibility(false)
    end
end

local function GetCurrentQuest()
    return quests[currentQuestIndex]
end

local function StartCurrentQuest()
    local quest = GetCurrentQuest()
    if quest == nil then
        state = QuestState.AllCompleted
        HideArrow()
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

    if EnsureArrow() == nil then
        print("Quest car actor not found.")
        return
    end

    state = QuestState.Active
end

local function ShowQuest(index)
    currentQuestIndex = index
    activeTarget = nil
    HideArrow()

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
    HideArrow()

    if currentQuestIndex < #quests then
        ShowQuest(currentQuestIndex + 1)
    else
        state = QuestState.AllCompleted
        print("All quests completed.")
    end
end

local function UpdateQuestArrow()
    local ownerCar = EnsureCar()
    if ownerCar == nil or activeTarget == nil then
        return
    end

    local questArrow = EnsureArrow()
    if questArrow == nil then
        return
    end

    local toTarget = activeTarget.Location - ownerCar.Location
    toTarget.Z = 0.0

    if toTarget:Length() <= 1.0 then
        return
    end

    local direction = toTarget:Normalized()
    local arrowLocation = ownerCar.Location + ownerCar.Forward * 2.7
    arrowLocation.Z = arrowLocation.Z + 0.7
    questArrow:SetWorldLocation(arrowLocation)
    questArrow:SetWorldDirection(direction)
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

    UpdateQuestArrow()
end
