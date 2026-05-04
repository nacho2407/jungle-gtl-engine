local UIManager = require("UIManager")
local ObjRegistry = require("ObjRegistry")
local gameState = nil

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
        fallbackTargetName = "CarWashTrigger",
        phase = ECarGamePhase.CarWash
    },
    {
        id = "gas",
        uiKey = "gasQuest",
        targetName = "GasStation",
        fallbackTargetName = "GasStationTrigger",
        phase = ECarGamePhase.CarGas
    },
    {
        id = "hitPerson",
        uiKey = "personQuest",
        targetName = "EscapePolice",
        fallbackTargetName = nil,
        phase = ECarGamePhase.EscapePolice
    },
    {
        id = "meteor",
        uiKey = "meteorQuest",
        targetName = "MeteorVolume",
        fallbackTargetName = nil,
        phase = ECarGamePhase.DodgeMeteor
    },
    {
        id = "goal",
        uiKey = "goalQuest",
        targetName = "Goal",                 -- Map.Scene 의 트리거 액터 Name 과 일치
        fallbackTargetName = "GoalTrigger",  -- 옛 이름 fallback
        phase = ECarGamePhase.Goal
    }
}

local state = QuestState.NotStarted
local currentQuestIndex = 0

local function OnPhaseChanged(phase)
    print("Phase changed: " .. tostring(phase))

    if phase == ECarGamePhase.None then
        return
    end

    if phase == ECarGamePhase.CarWash then
        UIManager.FadeOut(0.5, function()
            if ObjRegistry.manObj ~= nil then
                ObjRegistry.manObj.Location = Vector.new(150, -23, 4)
            end

            if ObjRegistry.car ~= nil then
                ObjRegistry.car.Location = Vector.new(150, -30, 4)
                ObjRegistry.car.Rotation = Vector.new(0, 0, 0)
            end

            CameraManager.PossessCamera(ObjRegistry.manCamera)

            UIManager.FadeIn(0.5)
        end)
        print("Run Lua logic for CarWash")
    elseif phase == ECarGamePhase.CarGas then
        UIManager.FadeOut(0.5, function()
            if ObjRegistry.manObj ~= nil then
                ObjRegistry.manObj.Location = Vector.new(128, -100, 5)
            end

            if ObjRegistry.car ~= nil then
                ObjRegistry.car.Location = Vector.new(130, -100, 5)
            end

            CameraManager.PossessCamera(ObjRegistry.manCamera)

            UIManager.FadeIn(0.5)
        end)
        print("Run Lua logic for CarGas")
    elseif phase == ECarGamePhase.EscapePolice then
        print("Run Lua logic for EscapePolice")
    elseif phase == ECarGamePhase.DodgeMeteor then
        print("Run Lua logic for DodgeMeteor")
    elseif phase == ECarGamePhase.Goal then
        print("Run Lua logic for Goal")
    elseif phase == ECarGamePhase.Result then
        if CameraManager.GetPossessedCameraOwner().UUID ~= ObjRegistry.car.UUID then
            UIManager.FadeOut(0.5, function()
                CameraManager.PossessCamera(ObjRegistry.carCamera)
                UIManager.FadeIn(0.5)
            end)
        end
    elseif phase == ECarGamePhase.Finished then
        local outcome = EFinishOutcome.None
        local score = 0
        if gameState ~= nil then
            outcome = gameState:GetFinishOutcome()
            score = gameState:GetScore()
        end
        UIManager.ShowGameOver(outcome, score, function()
            Engine.PauseGame()
        end)
        print("Run Lua logic for Finished, outcome=" .. tostring(outcome) .. ", score=" .. tostring(score))
    end
end

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

    if quest.id == "hitPerson" then
        return "사람치기"
    end

    if quest.id == "meteor" then
        return "운석 피하기"
    end

    if quest.id == "goal" then
        return "골인 지점으로"
    end

    return quest.id
end

local function GetArrowSymbolFromAngle(angle)
    if angle >= -22.5 and angle < 22.5 then
        return "&#8593;" -- ↑
    elseif angle >= 22.5 and angle < 67.5 then
        return "&#8599;" -- ↗
    elseif angle >= 67.5 and angle < 112.5 then
        return "&#8594;" -- →
    elseif angle >= 112.5 and angle < 157.5 then
        return "&#8600;" -- ↘
    elseif angle >= -67.5 and angle < -22.5 then
        return "&#8598;" -- ↖
    elseif angle >= -112.5 and angle < -67.5 then
        return "&#8592;" -- ←
    elseif angle >= -157.5 and angle < -112.5 then
        return "&#8601;" -- ↙
    end

    return "&#8595;" -- ↓
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

    if gameState ~= nil and quest.phase ~= nil then
        gameState:SetRemainingPhaseTime(0.0)
        gameState:SetQuestPhase(quest.phase)
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

    AudioManager.Play("Notify", 1.0)
end

local function CompleteCurrentQuest()
    local quest = GetCurrentQuest()
    if quest ~= nil then
        print("Quest completed:", quest.id)
    end

    AudioManager.Play("Complete", 1.0)

    state = QuestState.Completed
    UIManager.SetQuestHud("-", "&#8593;", false)

    StartCoroutine(function()
        Wait(1.0)
        if currentQuestIndex < #quests then
            ShowQuest(currentQuestIndex + 1)
        else
            state = QuestState.AllCompleted
            UIManager.SetQuestHud("완료", "&#8593;", false)
            if gameState ~= nil then
                gameState:SetQuestPhase(ECarGamePhase.None)
            end
            print("All quests completed.")
        end
    end)
end

local function UpdateQuestHud()
    local ownerCar = EnsureCar()
    if ownerCar == nil or activeTarget == nil then
        UIManager.SetQuestHud("-", "&#8593;", false)
        return
    end

    local toTarget = activeTarget.Location - ownerCar.Location
    toTarget.Z = 0.0
    local direction = toTarget:Normalized()

    local forward = ownerCar.Forward
    forward.Z = 0.0
    forward = forward:Normalized()

    local dot = forward:Dot(direction)
    if dot > 1.0 then dot = 1.0 end
    if dot < -1.0 then dot = -1.0 end

    local crossZ = forward.X * direction.Y - forward.Y * direction.X
    local angle = math.atan(crossZ, dot) * 180.0 / 3.14159265
    UIManager.SetQuestHud(GetQuestText(GetCurrentQuest()), GetArrowSymbolFromAngle(angle), true)
end

local function HandleQuestPhaseChanged(phase)
    local quest = GetCurrentQuest()
    if state ~= QuestState.Active or quest == nil then
        return
    end

    if phase == ECarGamePhase.Result
        and gameState ~= nil
        and gameState:GetLastEndedPhase() == quest.phase
        and gameState:GetLastPhaseResult() == EPhaseResult.Success then
        CompleteCurrentQuest()
    end
end

function BeginPlay()
    -- Scene reload 대응 — 모듈 로컬 상태를 매 BeginPlay 마다 초기화한다. Lua VM 은 scene
    -- 전환 사이에도 살아있어 module-top-level 의 `local state = QuestState.NotStarted` 같은
    -- 초기화는 두 번째 BeginPlay 에선 다시 실행되지 않으므로 명시적으로 리셋해야 함.
    state = QuestState.NotStarted
    currentQuestIndex = 0
    activeTarget = nil
    car = nil

    UIManager.Init()
    gameState = GetGameState()
    if gameState ~= nil then
        gameState:BindPhaseChanged(OnPhaseChanged)
        gameState:BindPhaseChanged(HandleQuestPhaseChanged)
    end

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
    UIManager.SetPersonQuestOkCallback(function()
        if currentQuestIndex == 3 and state == QuestState.WaitingAccept then
            StartCurrentQuest()
        end
    end)
    UIManager.SetMeteorQuestOkCallback(function()
        if currentQuestIndex == 4 and state == QuestState.WaitingAccept then
            StartCurrentQuest()
        end
    end)
    UIManager.SetGoalQuestOkCallback(function()
        if currentQuestIndex == 5 and state == QuestState.WaitingAccept then
            StartCurrentQuest()
        end
    end)

    -- Map.Scene 진입은 곧 "게임 시작" — 이전엔 intro start-button 콜백에서 했지만 이제는
    -- intro 가 별도 scene 이라 클릭이 곧 transition 이고, 여기서 (Map 의 BeginPlay) 가
    -- 본격 게임 진입 시점.
    print("Start Game!")
    UIManager.Show("gameOverlay")
    ShowQuest(1)
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    UIManager.Tick(dt)
    UIManager.UpdateHUD()
    UpdateCoroutines(dt)

    -- ESC 토글은 UIManager.OnEscapePressed 가 담당 — World pause 도중에도 동작해야 해서
    -- C++ UGameEngine::Tick 에서 직접 fire 하는 Engine.SetOnEscape 콜백 경로로 옮김.

    if state ~= QuestState.Active then
        return
    end

    UpdateQuestHud()
end
