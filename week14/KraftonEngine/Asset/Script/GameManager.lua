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
local scoreSubmitted = false
-- OnPhaseChanged 추적용 — EscapePolice/DodgeMeteor 시작/종료 시 카메라 자동 전환에 사용.
local prevPhase = ECarGamePhase.None
local THIRD_PERSON_PHASE_BLEND = 0.6
local policeCinematicOverlayWasVisible = false

local function IsThirdPersonPhase(phase)
    return phase == ECarGamePhase.EscapePolice or phase == ECarGamePhase.DodgeMeteor
end

local function SwitchPlayerCamera(toThirdPerson)
    if ObjRegistry.car == nil then return end
    local cam = nil
    if toThirdPerson then
        cam = ObjRegistry.car:GetThirdPersonCamera()
    else
        cam = ObjRegistry.car:GetFirstPersonCamera()
    end
    if cam ~= nil then
        CameraManager.SetActiveCameraWithBlend(cam, THIRD_PERSON_PHASE_BLEND)
    end
end
-- 다음 퀘스트 진행 coroutine 핸들 — 씬 전환 / EndPlay 시 정리해야 stale 상태로
-- 새 월드의 lua tick 에 끼어들지 않는다.
local nextQuestRoutine = nil

-- 모듈에서 spawn 한 fire-and-forget coroutine 들의 handle 모음. 명시 변수로 잡지 않은
-- 짧은 sequencer / sound delay / animation 같은 코루틴이 EndPlay (씬 종료) 또는 명시
-- 정리 시점에 한꺼번에 stop 될 수 있도록 list 로 추적. dead handle 도 포함되지만
-- StopCoroutine 이 idempotent 라 안전.
local pendingRoutines = {}

local function StartTrackedCoroutine(func)
    local handle = StartCoroutine(func)
    table.insert(pendingRoutines, handle)
    return handle
end

local function StopAllPendingRoutines()
    for _, handle in ipairs(pendingRoutines) do
        StopCoroutine(handle)
    end
    pendingRoutines = {}
end

local function CameraFadeTransition(duration, middle)
    duration = duration or 0.5

    CameraManager.FadeOut(duration)

    StartTrackedCoroutine(function()
        Wait(duration)

        if middle then
            middle()
        end

        CameraManager.FadeIn(duration)
    end)
end

-- 페이즈별 전용 BGM — EscapePolice / DodgeMeteor 진입 시 BGM swap, 이탈 시 기본 BGM 복원.
-- AudioManager.PlayBGM 은 호출 시 자동으로 기존 BGM stop 후 새로 재생하므로 단순 swap.
local PHASE_BGM = {
    [ECarGamePhase.EscapePolice] = "Phase_EscapePolice",
    [ECarGamePhase.DodgeMeteor]  = "Phase_Meteor",
}
local DEFAULT_BGM_KEY    = "CityBgm"
local DEFAULT_BGM_VOLUME = 0.1
local PHASE_BGM_VOLUME   = 0.35

local function HandlePhaseBGM(phase)
    local prevKey = PHASE_BGM[prevPhase]
    local nextKey = PHASE_BGM[phase]
    if nextKey == prevKey then
        return  -- 같은 BGM 유지 (예: Result -> Finished 둘 다 nil)
    end

    -- OnPhaseChanged 는 TickManager 콜스택 안에서 발동되는 callback 이라, 여기서
    -- 직접 PlayBGM 을 부르면 fmod 의 channel state 변경이 같은 Tick 의 다른 audio
    -- 호출과 경합하면서 crash 가 난다 (EscapePolice 정상 종료 reproducer 확인됨).
    -- 한 frame 미뤄 callback 이 반환 → Tick 이 끝난 후의 안전한 시점에 swap.
    StartTrackedCoroutine(function()
        Wait(0)
        if nextKey ~= nil then
            AudioManager.PlayBGM(nextKey, PHASE_BGM_VOLUME)
        else
            AudioManager.PlayBGM(DEFAULT_BGM_KEY, DEFAULT_BGM_VOLUME)
        end
    end)
end

-- DodgeMeteor 페이즈 동안만 흐르는 단일 ambient loop. 메테오 인스턴스마다 따로 두면
-- 동시 N개 채널이 겹쳐 듣기 거슬려서 페이즈 단위 1개로 통합.
local METEOR_FALL_LOOP_NAME = "MeteorFallAmbient"
local METEOR_FALL_VOLUME    = 0.45

local function HandleMeteorAmbientLoop(phase)
    local was = (prevPhase == ECarGamePhase.DodgeMeteor)
    local now = (phase == ECarGamePhase.DodgeMeteor)
    if now and not was then
        AudioManager.PlayLoop("MeteorFall", METEOR_FALL_LOOP_NAME, METEOR_FALL_VOLUME, 1.0)
    elseif was and not now then
        AudioManager.StopLoop(METEOR_FALL_LOOP_NAME)
    end
end

local function ShowMissionFeedback(phase)
    -- 게임플레이 페이즈 진입 시 mission card.
    if phase == ECarGamePhase.CarWash
        or phase == ECarGamePhase.CarGas
        or phase == ECarGamePhase.EscapePolice
        or phase == ECarGamePhase.DodgeMeteor
        or phase == ECarGamePhase.Goal then
        UIManager.ShowMissionStart(phase)
        return
    end

    -- Result 진입 시 직전 페이즈 결과 + 카메라 피드백.
    if phase == ECarGamePhase.Result and gameState ~= nil then
        local lastEnded = gameState:GetLastEndedPhase()
        local lastResult = gameState:GetLastPhaseResult()

        UIManager.ShowMissionResult(lastEnded, lastResult)

        if lastResult == EPhaseResult.Success then
        elseif lastResult == EPhaseResult.Failed then
            CameraManager.SetVignette(0.7, 0.6, 0.4)
            StartTrackedCoroutine(function()
                Wait(1.0)
                CameraManager.ClearVignette()
            end)
        end
    end
end

local function EaseOutCubic(t)
    local inv = 1.0 - t
    return 1.0 - inv * inv * inv
end

local function EaseInOutSine(t)
    return 0.5 - 0.5 * math.cos(t * 3.14159265)
end

local function PlayPoliceFlyBy(policeCar, index)
    if policeCar == nil or not policeCar:IsValid() then
        return
    end

    local duration = 1.15
    local elapsed = 0.0
    local sideSign = (index % 2 == 0) and -1.0 or 1.0
    local peakSpeed = 90.0 + index * 12.0
    local sideKick = 18.0 + index * 4.0

    while elapsed < duration do
        local dt = WaitFrame()
        elapsed = elapsed + dt

        local t = elapsed / duration
        if t > 1.0 then t = 1.0 end

        local speed = peakSpeed * EaseOutCubic(t)
        local lateral = sideSign * sideKick * math.sin(t * 3.14159265)
        local yawSpin = sideSign * (1.8 - EaseInOutSine(t) * 1.4)

        ObjRegistry.SetPoliceControl(policeCar, {
            aiEnabled = false,
            linearVelocity = policeCar.Forward * speed + policeCar.Right * lateral,
            angularVelocity = Vector.new(0, 0, yawSpin)
        })
    end

    ObjRegistry.SetPoliceControl(policeCar, {
        aiEnabled = false,
        linearVelocity = policeCar.Forward * peakSpeed,
        angularVelocity = Vector.new(0, 0, 0)
    })
end

local function BeginPoliceCinematic()
    policeCinematicOverlayWasVisible = UIManager.IsVisible("gameOverlay")
    UIManager.Hide("gameOverlay")
    UIManager.Hide("gasWidget")
    UIManager.Hide("meteorHp")

    ObjRegistry.SetPlayerCinematicLocked(true)

    if ObjRegistry.car ~= nil and ObjRegistry.car:IsValid() then
        local movement = ObjRegistry.car:GetCarMovement()
        if movement ~= nil then
            movement:StopImmediately()
            movement:SetThrottleInput(0)
            movement:SetSteeringInput(0)
        end

        ObjRegistry.car:SetVisible(false)
        ObjRegistry.car:SetCollisionEnabled(false)
    end
end

local function EndPoliceCinematic()
    ObjRegistry.SetPlayerCinematicLocked(false)

    if ObjRegistry.car ~= nil and ObjRegistry.car:IsValid() then
        local movement = ObjRegistry.car:GetCarMovement()
        if movement ~= nil then
            movement:StopImmediately()
            movement:SetThrottleInput(0)
            movement:SetSteeringInput(0)
        end

        ObjRegistry.car:SetVisible(true)
        ObjRegistry.car:SetCollisionEnabled(true)
    end

    if policeCinematicOverlayWasVisible then
        UIManager.Show("gameOverlay")
    end
    policeCinematicOverlayWasVisible = false
end

local function OnPhaseChanged(phase)
    print("Phase changed: " .. tostring(phase))

    -- EscapePolice / DodgeMeteor 시작 시 자동으로 3인칭 전환. 해당 페이즈가 끝나
    -- 다른 페이즈로 넘어가면 1인칭 복귀. 이전 페이즈 추적은 prevPhase.
    local wasEscapePolice = prevPhase == ECarGamePhase.EscapePolice
    local enterThirdPerson = IsThirdPersonPhase(phase) and not IsThirdPersonPhase(prevPhase)
    local exitThirdPerson  = IsThirdPersonPhase(prevPhase) and not IsThirdPersonPhase(phase)
    if enterThirdPerson then
        SwitchPlayerCamera(true)
    elseif exitThirdPerson then
        SwitchPlayerCamera(false)
    end

    -- DodgeMeteor 진입/이탈 시 ambient loop 토글 (prevPhase 갱신 전에 결정).
    HandleMeteorAmbientLoop(phase)

    -- EscapePolice / DodgeMeteor 페이즈 BGM 토글 — 그 외 phase 면 기본 BGM 복원.
    -- HandlePhaseBGM 내부에서 coroutine 으로 한 frame 미뤄 PlayBGM 호출 (fmod race 회피).
    HandlePhaseBGM(phase)

    if wasEscapePolice and phase ~= ECarGamePhase.EscapePolice then
        ObjRegistry.ClearAllPoliceControls()
        EndPoliceCinematic()
    end

    prevPhase = phase

    -- Mission card / Result 피드백 — 페이즈별 차별 표시.
    ShowMissionFeedback(phase)

    if phase == ECarGamePhase.None then
        return
    end

    if phase == ECarGamePhase.CarWash then
        CameraFadeTransition(0.5, function()
            if ObjRegistry.manObj ~= nil then
                ObjRegistry.manObj.Location = Vector.new(150, -23, 4)
            end

            if ObjRegistry.car ~= nil then
                ObjRegistry.car.Location = Vector.new(150, -30, 4)
                ObjRegistry.car.Rotation = Vector.new(0, 0, 0)
            end

            CameraManager.PossessCamera(ObjRegistry.manCamera)
        end)
        print("Run Lua logic for CarWash")
    elseif phase == ECarGamePhase.CarGas then
        CameraFadeTransition(0.5, function()
            if ObjRegistry.manObj ~= nil then
                ObjRegistry.manObj.Location = Vector.new(-100, -234, 3)
            end

            if ObjRegistry.car ~= nil then
                ObjRegistry.car.Location = Vector.new(-100, -230, 3)
            end

            CameraManager.PossessCamera(ObjRegistry.manCamera)
        end)
        print("Run Lua logic for CarGas")
    elseif phase == ECarGamePhase.EscapePolice then
        StartTrackedCoroutine(function()
            BeginPoliceCinematic()

            ObjRegistry.SetAllPoliceControls(function(policeCar, index)
                return {
                    aiEnabled = false,
                    linearVelocity = Vector.new(0, 0, 0),
                    angularVelocity = Vector.new(0, 0, 0),
                    stop = true
                }
            end)

            CameraFadeTransition(0.5, function()
                if ObjRegistry.policeCineCameraObj ~= nil then
                    CameraManager.ToggleOwnerCamera(ObjRegistry.policeCineCameraObj)
                end
            end)

            Wait(2.0)

            for i, policeCar in ipairs(ObjRegistry.policeCars) do
                StartTrackedCoroutine(function()
                    Wait(0.5 + (i - 1) * 0.3)
                    AudioManager.Play("Whoosh", 1.0)
                    CameraManager.StartCameraShakeAsset("Asset/Test.shake", 10.0)
                    PlayPoliceFlyBy(policeCar, i)

                end)
            end

            Wait(3.0)

            ObjRegistry.ClearAllPoliceControls()
            EndPoliceCinematic()

            CameraFadeTransition(0.5, function()
                CameraManager.PossessCamera(ObjRegistry.carCamera)
            end)
        end)
        print("Run Lua logic for EscapePolice")
    elseif phase == ECarGamePhase.DodgeMeteor then
        print("Run Lua logic for DodgeMeteor")
    elseif phase == ECarGamePhase.Goal then
        print("Run Lua logic for Goal")
    elseif phase == ECarGamePhase.Result then
        if CameraManager.GetPossessedCameraOwner().UUID ~= ObjRegistry.car.UUID then
            CameraFadeTransition(0.5, function()
                CameraManager.PossessCamera(ObjRegistry.carCamera)
            end)
        end
    elseif phase == ECarGamePhase.Finished then
        local outcome = EFinishOutcome.None
        local score = 0
        if gameState ~= nil then
            outcome = gameState:GetFinishOutcome()
            score = gameState:GetScore()
        end
        if not scoreSubmitted then
            scoreSubmitted = true
            Scoreboard.SubmitScore(score)
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

    if nextQuestRoutine ~= nil then
        StopCoroutine(nextQuestRoutine)
        nextQuestRoutine = nil
    end

    nextQuestRoutine = StartCoroutine(function()
        Wait(1.0)
        nextQuestRoutine = nil
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
    scoreSubmitted = false
    activeTarget = nil
    car = nil
    -- 이전 BeginPlay 에서 남은 coroutine 핸들이 살아있을 수 있으니 리셋. 옛 핸들은
    -- FireWorldReset 으로 무효화됐으므로 nil 처리만 충분.
    nextQuestRoutine = nil

    UIManager.Init()

    gameState = GetGameState()
    if gameState ~= nil then
        gameState:SetMatchTimerRunning(false)
        gameState:BindPhaseChanged(OnPhaseChanged)
        gameState:BindPhaseChanged(HandleQuestPhaseChanged)
    end

    UIManager.SetStartStoryOkCallback(function()
        if state ~= QuestState.NotStarted then
            return
        end

        print("Start Story accepted.")
        if gameState ~= nil then
            gameState:SetMatchTimerRunning(true)
        end

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
    UIManager.Show("startStory")
end

function EndPlay()
    if nextQuestRoutine ~= nil then
        StopCoroutine(nextQuestRoutine)
        nextQuestRoutine = nil
    end
    -- GameManager 가 spawn 한 fire-and-forget coroutine 정리. StopAllCoroutines 가 어차피
    -- 일괄 stop 하지만 명시 정리로 의도 명확화 + 다른 lua 모듈의 long-running coroutine
    -- (e.g. WalkingPerson 회전) 과 정리 책임을 분리.
    StopAllPendingRoutines()

    ObjRegistry.ClearAllPoliceControls()
    EndPoliceCinematic()
    UIManager.Shutdown()

    StopAllCoroutines()
end

function OnOverlap(OtherActor)
end

-- 디버그 치트 — F5~F8: 현재 phase 가 매핑된 phase 와 일치할 때만 SuccessPhase 호출.
-- 페이즈 진행 중에 즉시 클리어해 다음 흐름 확인 / QA 용. 빌드 분리는 안 했으니 출시 전 정리.
local CHEAT_KEY_TO_PHASE = {
    [Key.F5] = ECarGamePhase.CarWash,
    [Key.F6] = ECarGamePhase.CarGas,
    [Key.F7] = ECarGamePhase.EscapePolice,
    [Key.F8] = ECarGamePhase.DodgeMeteor,
}

local function HandleCheatHotkeys()
    if gameState == nil then return end
    local current = gameState:GetPhase()
    for cheatKey, expectedPhase in pairs(CHEAT_KEY_TO_PHASE) do
        if Input.GetKeyDown(cheatKey) and current == expectedPhase then
            local gm = GetGameMode()
            if gm ~= nil then
                gm:SuccessPhase()
            end
            return  -- 한 프레임에 하나만 발동
        end
    end
end

function Tick(dt)
    UIManager.Tick(dt)
    UIManager.UpdateHUD()

    -- ESC 토글은 UIManager.OnEscapePressed 가 담당 — World pause 도중에도 동작해야 해서
    -- C++ UGameEngine::Tick 에서 직접 fire 하는 Engine.SetOnEscape 콜백 경로로 옮김.

    HandleCheatHotkeys()

    if state ~= QuestState.Active then
        return
    end

    UpdateQuestHud()
end
