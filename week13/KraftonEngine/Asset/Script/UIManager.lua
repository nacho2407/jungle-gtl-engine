local UIManager = {}
local ObjRegistry = require("ObjRegistry")
local widgets = {}

local SPEEDOMETER_DEFAULT_MAX_SPEED = 100.0
local SPEEDOMETER_MIN_ANGLE = 135.0
local SPEEDOMETER_MAX_ANGLE = 315.0
local speedometerCachedMaxSpeed = nil

local onStartGame = nil
local fade = {
    active = false,
    elapsed = 0,
    duration = 0,
    from = 0,
    to = 0,
    onComplete = nil,
    hideWhenDone = false
}
local scoreCounter = {
    initialized = false,
    display = 0,
    target = 0
}
local scoreFeedback = {
    lastShownEventId = 0,
    duration = 1.35,
    slots = {
        { active = false, elapsed = 0, text = "" },
        { active = false, elapsed = 0, text = "" },
        { active = false, elapsed = 0, text = "" }
    }
}
local gameOverScoreRoutine = nil
local criticalWarningRoutine = nil
local onStartStoryOk = nil
local onCarWashQuestOk = nil
local onGasQuestOk = nil
local onPersonQuestOk = nil
local onMeteorQuestOk = nil
local onGoalQuestOk = nil
local introLayout = {
    width = 0,
    height = 0
}

local function Clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function SetFadeOpacity(opacity)
    local fadeWidget = widgets["fade"]
    if fadeWidget == nil then
        return false
    end

    if not fadeWidget:IsInViewport() then
        fadeWidget:AddToViewportZ(10000)
    end

    opacity = Clamp(opacity, 0, 1)
    return fadeWidget:set_property("fade-screen", "opacity", tostring(opacity))
end

local function GetGasColor(ratio)
    if ratio <= 0.2 then
        return "#e65353"
    elseif ratio <= 0.5 then
        return "#f09a3e"
    end
    return "#f0c85a"
end

local function FormatScore(score)
    return string.format("%d", math.floor(score + 0.5))
end

local function SetHudScoreText(score)
    local widget = widgets["gameOverlay"]
    if widget == nil or not widget:IsInViewport() then
        return
    end

    widget:set_text("score-value", FormatScore(score))
end

local function FormatSpeedometerLabel(speed)
    return tostring(math.floor(speed + 0.5))
end

local function UpdateSpeedometerLabels(widget, maxSpeed)
    if speedometerCachedMaxSpeed ~= nil and math.abs(speedometerCachedMaxSpeed - maxSpeed) < 0.01 then
        return
    end

    speedometerCachedMaxSpeed = maxSpeed
    widget:set_text("speedometer-number-0", "0")
    widget:set_text("speedometer-number-20", FormatSpeedometerLabel(maxSpeed * 0.2))
    widget:set_text("speedometer-number-40", FormatSpeedometerLabel(maxSpeed * 0.4))
    widget:set_text("speedometer-number-60", FormatSpeedometerLabel(maxSpeed * 0.6))
    widget:set_text("speedometer-number-80", FormatSpeedometerLabel(maxSpeed * 0.8))
    widget:set_text("speedometer-number-100", FormatSpeedometerLabel(maxSpeed))
end

local function UpdateSpeedometerWidget(widget)
    local rawSpeed = 0.0
    local maxSpeed = SPEEDOMETER_DEFAULT_MAX_SPEED
    local car = ObjRegistry.car
    if car ~= nil then
        local movement = car:GetCarMovement()
        if movement ~= nil then
            rawSpeed = math.abs(movement:GetForwardSpeed())
            maxSpeed = math.abs(movement:GetMaxSpeed())
        end
    end

    if maxSpeed <= 0.01 then
        maxSpeed = SPEEDOMETER_DEFAULT_MAX_SPEED
    end

    UpdateSpeedometerLabels(widget, maxSpeed)

    local speed = Clamp(rawSpeed, 0.0, maxSpeed)
    local speedRatio = speed / maxSpeed
    local angle = SPEEDOMETER_MIN_ANGLE + (SPEEDOMETER_MAX_ANGLE - SPEEDOMETER_MIN_ANGLE) * speedRatio

    widget:set_text("speedometer-value", string.format("%03d", math.floor(speed + 0.5)))
    widget:set_property("speedometer-needle", "transform", string.format("rotate(%.1fdeg)", angle))
end

local function SetDpProperty(widget, elementId, propertyName, value)
    widget:set_property(elementId, propertyName, string.format("%.0fdp", value))
end

local function ApplyIntroLayout()
    local widget = widgets["intro"]
    if widget == nil or not widget:IsInViewport() then
        return
    end

    if Engine.GetViewportSize == nil then
        return
    end

    local viewport = Engine.GetViewportSize()
    if viewport == nil then
        return
    end

    local width = viewport.Width or 0
    local height = viewport.Height or 0
    if width <= 0 or height <= 0 then
        return
    end

    if introLayout.width == width and introLayout.height == height then
        return
    end

    introLayout.width = width
    introLayout.height = height

    local minSize = width
    if height < minSize then minSize = height end

    local titleSize = Clamp(minSize * 0.44, 260, 520)
    local titleTop = Clamp(height * 0.09 + 50, 80, height * 0.24)
    local titleLeft = width * 0.5 - titleSize * 0.5
    local titleBottom = titleTop + titleSize

    local subtitleTop = titleBottom + Clamp(height * 0.012, 8, 18)
    local startTop = subtitleTop + Clamp(height * 0.045, 36, 54)
    local buttonGap = Clamp(height * 0.022, 10, 24)
    local scoreboardTop = startTop + 58 + buttonGap
    local exitTop = scoreboardTop + 58 + buttonGap
    local footerTop = exitTop + 58 + Clamp(height * 0.018, 8, 20)

    local maxFooterTop = height * 0.9
    if footerTop > maxFooterTop then
        local overflow = footerTop - maxFooterTop
        subtitleTop = subtitleTop - overflow
        startTop = startTop - overflow
        scoreboardTop = scoreboardTop - overflow
        exitTop = exitTop - overflow
        footerTop = maxFooterTop
    end

    SetDpProperty(widget, "intro-title-pop", "left", titleLeft)
    SetDpProperty(widget, "intro-title-pop", "top", titleTop)
    SetDpProperty(widget, "intro-title-pop", "width", titleSize)
    SetDpProperty(widget, "intro-title-pop", "height", titleSize)
    widget:set_property("intro-title-pop", "margin-left", "0dp")
    widget:set_property("intro-title-pop", "margin-top", "0dp")

    SetDpProperty(widget, "intro-title", "width", titleSize)
    SetDpProperty(widget, "intro-title", "height", titleSize)

    SetDpProperty(widget, "intro-subtitle", "top", subtitleTop)
    SetDpProperty(widget, "start-button", "top", startTop)
    SetDpProperty(widget, "scoreboard-button", "top", scoreboardTop)
    SetDpProperty(widget, "exit-button", "top", exitTop)
    SetDpProperty(widget, "intro-footer", "top", footerTop)
end

local function SetGameOverScoreText(score)
    local widget = widgets["gameOver"]
    if widget == nil or not widget:IsInViewport() then
        return
    end

    widget:set_text("final-score-value", FormatScore(score))
end

local function UpdateScoreboardWidget()
    local widget = widgets["scoreboard"]
    if widget == nil or not widget:IsInViewport() then
        return
    end

    Scoreboard.Load()

    local count = Scoreboard.GetEntryCount()
    for i = 0, 9 do
        local entry = nil
        if i < count then
            entry = Scoreboard.GetEntry(i)
        end

        if entry ~= nil then
            widget:set_text("scoreboard-rank-" .. tostring(i), tostring(i + 1))
            widget:set_text("scoreboard-score-" .. tostring(i), FormatScore(entry.Score))
        else
            widget:set_text("scoreboard-rank-" .. tostring(i), "-")
            widget:set_text("scoreboard-score-" .. tostring(i), "-")
        end
    end

    if count > 0 then
        widget:set_text("scoreboard-empty", "")
    else
        widget:set_text("scoreboard-empty", "NO SCORES YET")
    end
end

-- 게임 측 reason 식별자 → 플레이어가 보는 텍스트 매핑. 미등록 reason 은 원문을 그대로 노출.
local SCORE_REASON_LABELS = {
    HitPerson    = "보행자 적중",
    CarWashClear = "세차 완료",
    CarGasClear  = "주유 완료",
    EscapeClear  = "경찰 회피",
    MeteorClear  = "운석 회피",
    GoalClear    = "도착",
    Final        = "종합 보너스",
}

local function FormatScoreFeedback(amount, reason)
    local prefix = "+"
    if amount < 0 then
        prefix = ""
    end

    if reason == nil or reason == "" then
        return prefix .. tostring(amount)
    end

    local label = SCORE_REASON_LABELS[reason] or reason
    return prefix .. tostring(amount) .. " " .. label
end

local function ApplyScoreFeedbackSlot(index)
    local widget = widgets["gameOverlay"]
    if widget == nil or not widget:IsInViewport() then
        return
    end

    local slot = scoreFeedback.slots[index]
    local elementId = "score-feedback-" .. tostring(index - 1)

    if not slot.active then
        widget:set_text(elementId, "")
        widget:set_property(elementId, "opacity", "0")
        return
    end

    local fadeIn = 0.12
    local fadeOut = 0.38
    local opacity = 1.0
    if slot.elapsed < fadeIn then
        opacity = slot.elapsed / fadeIn
    elseif slot.elapsed > scoreFeedback.duration - fadeOut then
        opacity = (scoreFeedback.duration - slot.elapsed) / fadeOut
    end

    opacity = Clamp(opacity, 0, 1)
    widget:set_text(elementId, slot.text)
    widget:set_property(elementId, "opacity", tostring(opacity))
end

local function PushScoreFeedback(amount, reason)
    for i = #scoreFeedback.slots, 2, -1 do
        scoreFeedback.slots[i].active = scoreFeedback.slots[i - 1].active
        scoreFeedback.slots[i].elapsed = scoreFeedback.slots[i - 1].elapsed
        scoreFeedback.slots[i].text = scoreFeedback.slots[i - 1].text
        ApplyScoreFeedbackSlot(i)
    end

    scoreFeedback.slots[1].active = true
    scoreFeedback.slots[1].elapsed = 0
    scoreFeedback.slots[1].text = FormatScoreFeedback(amount, reason)
    ApplyScoreFeedbackSlot(1)
end

local function UpdateScoreFeedback(dt)
    dt = dt or 0

    for i = 1, #scoreFeedback.slots do
        local slot = scoreFeedback.slots[i]
        if slot.active then
            slot.elapsed = slot.elapsed + dt
            if slot.elapsed >= scoreFeedback.duration then
                slot.active = false
                slot.elapsed = 0
                slot.text = ""
            end
            ApplyScoreFeedbackSlot(i)
        end
    end
end

local function PollScoreEvents(gs)
    local lastEventId = gs:GetLastScoreEventId()
    if lastEventId < scoreFeedback.lastShownEventId then
        scoreFeedback.lastShownEventId = 0
    end

    local count = gs:GetScoreEventCount()
    for i = 0, count - 1 do
        local event = gs:GetScoreEvent(i)
        if event ~= nil and event.SequenceId > scoreFeedback.lastShownEventId then
            PushScoreFeedback(event.Amount, event.Reason)
            scoreFeedback.lastShownEventId = event.SequenceId
        end
    end
end

local function UpdateScoreCounter(dt)
    dt = dt or 0

    local widget = widgets["gameOverlay"]
    if widget == nil or not widget:IsInViewport() or not scoreCounter.initialized then
        return
    end

    if scoreCounter.display == scoreCounter.target then
        return
    end

    if scoreCounter.display > scoreCounter.target then
        scoreCounter.display = scoreCounter.target
        SetHudScoreText(scoreCounter.display)
        return
    end

    local remaining = scoreCounter.target - scoreCounter.display
    local minSpeed = 240.0
    local speed = math.max(minSpeed, remaining * 8.0)
    scoreCounter.display = scoreCounter.display + speed * dt

    if scoreCounter.display >= scoreCounter.target then
        scoreCounter.display = scoreCounter.target
    end

    SetHudScoreText(scoreCounter.display)
end

local function StopGameOverScoreRoutine()
    if gameOverScoreRoutine ~= nil then
        StopCoroutine(gameOverScoreRoutine)
        gameOverScoreRoutine = nil
    end
end

local function StopCriticalWarningRoutine()
    if criticalWarningRoutine ~= nil then
        StopCoroutine(criticalWarningRoutine)
        criticalWarningRoutine = nil
    end
end

local function StartGameOverScoreRoutine(events, finalScore, onComplete)
    StopGameOverScoreRoutine()

    gameOverScoreRoutine = StartCoroutine(function()
        local widget = widgets["gameOver"]
        if widget == nil or not widget:IsInViewport() then
            gameOverScoreRoutine = nil
            if onComplete ~= nil then onComplete() end
            return
        end

        local display = 0
        local duration = 0.75
        SetGameOverScoreText(display)
        widget:set_text("final-score-detail", "")

        for i = 1, #events do
            if widget == nil or not widget:IsInViewport() then
                gameOverScoreRoutine = nil
                return
            end

            AudioManager.Play("ScoreUp", 1.0)

            local event = events[i]
            local startScore = display
            local targetScore = display + event.amount
            local elapsed = 0

            widget:set_text("final-score-detail", FormatScoreFeedback(event.amount, event.reason))

            while elapsed < duration do
                local dt = WaitFrame()
                if widget == nil or not widget:IsInViewport() then
                    gameOverScoreRoutine = nil
                    return
                end

                elapsed = elapsed + (dt or 0)
                local t = Clamp(elapsed / duration, 0, 1)
                local eased = 1.0 - ((1.0 - t) * (1.0 - t))
                SetGameOverScoreText(startScore + (targetScore - startScore) * eased)
            end

            display = targetScore
            SetGameOverScoreText(display)

            Wait(0.5)
        end

        SetGameOverScoreText(finalScore)
        widget:set_text("final-score-detail", "")
        gameOverScoreRoutine = nil

        if onComplete ~= nil then
            onComplete()
        end
    end)
end

function UIManager.SetStartGameCallback(callback)
    onStartGame = callback
end

function UIManager.SetStartStoryOkCallback(callback)
    onStartStoryOk = callback
end

function UIManager.SetCarWashQuestOkCallback(callback)
    onCarWashQuestOk = callback
end

function UIManager.SetGasQuestOkCallback(callback)
    onGasQuestOk = callback
end

function UIManager.SetPersonQuestOkCallback(callback)
    onPersonQuestOk = callback
end

function UIManager.SetMeteorQuestOkCallback(callback)
    onMeteorQuestOk = callback
end

function UIManager.SetGoalQuestOkCallback(callback)
    onGoalQuestOk = callback
end

function UIManager.Init()
    -- Scene reload 대응 — UI 위젯은 UUIManager 싱글턴이 보유 (월드와 별개) 라 첫 init 후
    -- 유지된다. 두 번째 Init 호출 시 위젯 재생성 없이 표시만 인트로 화면으로 정리.
    if widgets.intro ~= nil then
        -- Scene 전환 시 재호출. 게임플레이 위젯은 모두 정리하고 fade 도 cancel 한다.
        -- IntroWidget 은 "보일지 말지" 를 호출자 (Intro.Scene 의 IntroManager.lua /
        -- Map.Scene 의 GameManager.lua) 가 결정하도록 여기선 명시적으로 hide.
        UIManager.Hide("intro")
        UIManager.Hide("gameOverlay")
        UIManager.Hide("gameOver")
        UIManager.Hide("criticalWarning")
        UIManager.Hide("scoreboard")
        UIManager.Hide("gasWidget")
        UIManager.Hide("meteorHp")
        UIManager.Hide("startStory")
        UIManager.Hide("carWashQuest")
        UIManager.Hide("gasQuest")
        UIManager.Hide("personQuest")
        UIManager.Hide("meteorQuest")
        UIManager.Hide("goalQuest")
        UIManager.Hide("contributor")
        UIManager.Hide("whiteBox")
        UIManager.Hide("pauseMenu")
        -- 진행 중이던 fade 취소 — 콜백이 죽은 액터를 참조하지 않도록.
        fade.active = false
        fade.onComplete = nil
        SetFadeOpacity(0)
        UIManager.Hide("fade")
        return
    end

    local introWidget = UI.CreateWidget("Asset/UI/IntroWidget.rml")
    introWidget:SetWantsMouse(true)
    introWidget:bind_click("start-button", function()
        -- 인트로 → 게임 화면 전환. fade 가 끝나면 Map.Scene 으로 transition.
        -- TransitionToScene 은 UGameEngine::Tick 끝에 deferred 처리되므로 fade callback
        -- 안에서 호출해도 안전.
        AudioManager.Play("Click", 1.0)
        UIManager.FadeOut(0.5, function()
            UIManager.Hide("intro")
        Engine.TransitionToScene("Map")
        end)
    end)
    introWidget:bind_click("scoreboard-button", function()
        AudioManager.Play("Click", 1.0)
        UIManager.Show("scoreboard")
        UpdateScoreboardWidget()
    end)
    introWidget:bind_click("exit-button", function()
        AudioManager.Play("Click", 1.0)
        Engine.Exit()
    end)

    local contributorWidget = UI.CreateWidget("Asset/UI/ContributorWidget.rml")
    contributorWidget:SetWantsMouse(true)
    contributorWidget:bind_click("contributor-close-button", function()
        AudioManager.Play("Click", 1.0)
        UIManager.Hide("contributor")
    end)

    local startStoryWidget = UI.CreateWidget("Asset/UI/StartStory.rml")
    startStoryWidget:SetWantsMouse(true)
    startStoryWidget:bind_click("start-story-ok-button", function()
        AudioManager.Play("Click", 1.0)
        UIManager.Hide("startStory")

        if onStartStoryOk ~= nil then
            onStartStoryOk()
        end
    end)

    local scoreboardWidget = UI.CreateWidget("Asset/UI/ScoreboardWidget.rml")
    scoreboardWidget:SetWantsMouse(true)
    scoreboardWidget:bind_click("scoreboard-close-button", function()
        AudioManager.Play("Click", 1.0)
        UIManager.Hide("scoreboard")
    end)

    local criticalWarningWidget = UI.CreateWidget("Asset/UI/CriticalWarningWidget.rml")

    local carWashQuestWidget = UI.CreateWidget("Asset/UI/CarWashQuestWidget.rml")
    carWashQuestWidget:SetWantsMouse(true)
    carWashQuestWidget:bind_click("car-wash-quest-ok-button", function()
        AudioManager.Play("Click", 1.0)
        UIManager.Hide("carWashQuest")

        if onCarWashQuestOk ~= nil then
            onCarWashQuestOk()
        end
    end)

    local gasQuestWidget = UI.CreateWidget("Asset/UI/GasQuestWidget.rml")
    gasQuestWidget:SetWantsMouse(true)
    gasQuestWidget:bind_click("gas-quest-ok-button", function()
        AudioManager.Play("Click", 1.0)
        UIManager.Hide("gasQuest")

        if onGasQuestOk ~= nil then
            onGasQuestOk()
        end
    end)

    local personQuestWidget = UI.CreateWidget("Asset/UI/PersonQuestWidget.rml")
    personQuestWidget:SetWantsMouse(true)
    personQuestWidget:bind_click("person-quest-ok-button", function()
        AudioManager.Play("Click", 1.0)
        UIManager.Hide("personQuest")

        if onPersonQuestOk ~= nil then
            onPersonQuestOk()
        end
    end)

    local meteorQuestWidget = UI.CreateWidget("Asset/UI/MeteorQuestWidget.rml")
    meteorQuestWidget:SetWantsMouse(true)
    meteorQuestWidget:bind_click("meteor-quest-ok-button", function()
        AudioManager.Play("Click", 1.0)
        UIManager.Hide("meteorQuest")

        if onMeteorQuestOk ~= nil then
            onMeteorQuestOk()
        end
    end)

    local goalQuestWidget = UI.CreateWidget("Asset/UI/GoalQuestWidget.rml")
    goalQuestWidget:SetWantsMouse(true)
    goalQuestWidget:bind_click("goal-quest-ok-button", function()
        UIManager.Hide("goalQuest")

        if onGoalQuestOk ~= nil then
            onGoalQuestOk()
        end
    end)

    local pauseMenuWidget = UI.CreateWidget("Asset/UI/PauseMenuWidget.rml")
    pauseMenuWidget:SetWantsMouse(true)
    pauseMenuWidget:bind_click("pause-menu-go-intro-button", function()
        -- Intro.Scene 으로 transition — Map.Scene 의 모든 동적 상태 (차량 / 경찰 / 운석 /
        -- GameMode 타이머 / Lua 모듈 로컬) 가 월드 destroy 와 함께 정리되고, Intro.Scene
        -- 의 IntroManager.lua BeginPlay 가 IntroWidget 을 다시 띄운다.
        AudioManager.Play("Click", 1.0)
        UIManager.Hide("pauseMenu")
        Engine.ResumeGame()  -- Intro 씬은 paused 상태가 아니어야 — fade Tick 등이 동작.
        Engine.TransitionToScene("Intro")
    end)
    pauseMenuWidget:bind_click("pause-menu-exit-button", function()
        AudioManager.Play("Click", 1.0)
        Engine.Exit()
    end)

    local gameOverWidget = UI.CreateWidget("Asset/UI/GameOverWidget.rml")
    gameOverWidget:SetWantsMouse(true)
    gameOverWidget:bind_click("lobby-button", function()
        -- Intro 씬으로 복귀 — Map 재진입 시 일부 상태가 stale 하게 남는 증상이 있어
        -- 로비를 거쳐 새로 시작하도록. Intro 의 start-button 이 Map 으로 다시 transition.
        AudioManager.Play("Click", 1.0)
        UIManager.Hide("gameOver")
        Engine.ResumeGame()
        Engine.TransitionToScene("Intro")
    end)
    gameOverWidget:bind_click("game-over-exit-button", function()
        AudioManager.Play("Click", 1.0)
        Engine.Exit()
    end)

    UIManager.Register("intro", introWidget)
    UIManager.Register("whiteBox", UI.CreateWidget("Asset/UI/PIEWhiteBox.rml"))
    UIManager.Register("contributor", contributorWidget)
    UIManager.Register("scoreboard", scoreboardWidget)
    UIManager.Register("criticalWarning", criticalWarningWidget)
    UIManager.Register("gameOverlay", UI.CreateWidget("Asset/UI/GameOverlayWidget.rml"))
    UIManager.Register("startStory", startStoryWidget)
    UIManager.Register("carWashQuest", carWashQuestWidget)
    UIManager.Register("gasQuest", gasQuestWidget)
    UIManager.Register("personQuest", personQuestWidget)
    UIManager.Register("meteorQuest", meteorQuestWidget)
    UIManager.Register("goalQuest", goalQuestWidget)
    UIManager.Register("gasWidget", UI.CreateWidget("Asset/UI/GasWidget.rml"))
    UIManager.Register("meteorHp", UI.CreateWidget("Asset/UI/MeteorHpWidget.rml"))
    UIManager.Register("gameOver", gameOverWidget)
    UIManager.Register("pauseMenu", pauseMenuWidget)
    UIManager.Register("fade", UI.CreateWidget("Asset/UI/FadeWidget.rml"))

    -- ESC 처리 — World pause 와 무관하게 동작해야 하므로 component-tick 이 아닌 C++ 의
    -- UGameEngine::Tick 이 직접 fire 하는 콜백 경로 (Engine.SetOnEscape) 에 바인딩.
    Engine.SetOnEscape(UIManager.OnEscapePressed)
end

-- GameManager EndPlay 에서 호출. UIManager 가 보유한 coroutine 핸들들을 정리해 다음
-- BeginPlay (또는 씬 전환) 시점에 stale routine 이 남지 않게 한다. 위젯 자체는 UUIManager
-- 싱글턴 소유라 destroy 하지 않고 Hide 만.
function UIManager.Shutdown()
    StopGameOverScoreRoutine()
    StopCriticalWarningRoutine()
end

function UIManager.OnEscapePressed()
    -- 인트로 / 결과 화면 등 다른 fullscreen 모달이 떠 있으면 ESC 무시.
    if UIManager.IsVisible("intro") or UIManager.IsVisible("gameOver") then
        return
    end

    if UIManager.IsVisible("pauseMenu") then
        UIManager.Hide("pauseMenu")
        Engine.ResumeGame()
    else
        UIManager.Show("pauseMenu")
        Engine.PauseGame()
    end
end

function UIManager.Register(key, widget)
    widgets[key] = widget
end

function UIManager.Show(key)
    local widget = widgets[key]
    if widget ~= nil then
        widget:show()
        if key == "intro" then
            introLayout.width = 0
            introLayout.height = 0
            ApplyIntroLayout()
        end
        return true
    end
    return false
end

function UIManager.Hide(key)
    local widget = widgets[key]
    if widget ~= nil then
        if key == "gameOver" then
            StopGameOverScoreRoutine()
        elseif key == "criticalWarning" then
            StopCriticalWarningRoutine()
        end
        widget:hide()
        return true
    end
    return false
end

function UIManager.Toggle(key)
    local widget = widgets[key]
    if widget == nil then
        return false
    end

    if widget:IsInViewport() then
        widget:hide()
    else
        widget:show()
    end
    return true
end

function UIManager.IsVisible(key)
    local widget = widgets[key]
    if widget == nil then
        return false
    end
    return widget:IsInViewport()
end

function UIManager.SetQuestHud(questText, arrowSymbol, visible)
    local widget = widgets["gameOverlay"]
    if widget == nil then
        return false
    end

    local opacity = "1"
    if visible == false then
        opacity = "0"
    end

    widget:set_text("quest-value", questText or "-")
    widget:set_text("quest-arrow", arrowSymbol or "&#8593;")
    widget:set_property("hud-quest-panel", "opacity", opacity)
    return true
end

function UIManager.FadeTo(fromOpacity, toOpacity, duration, onComplete, hideWhenDone)
    fade.active = true
    fade.elapsed = 0
    fade.duration = duration or 1
    fade.from = Clamp(fromOpacity or 0, 0, 1)
    fade.to = Clamp(toOpacity or 0, 0, 1)
    fade.onComplete = onComplete
    fade.hideWhenDone = hideWhenDone == true

    if fade.duration <= 0 then
        SetFadeOpacity(fade.to)
        fade.active = false
        if fade.hideWhenDone then
            UIManager.Hide("fade")
        end
        if fade.onComplete ~= nil then
            fade.onComplete()
        end
        return
    end

    SetFadeOpacity(fade.from)
end

function UIManager.FadeOut(duration, onComplete)
    UIManager.FadeTo(0, 1, duration, onComplete, false)
end

function UIManager.FadeIn(duration, onComplete)
    UIManager.FadeTo(1, 0, duration, onComplete, true)
end

function UIManager.IsFading()
    return fade.active
end

-- styleClass 가 nil 이면 RCSS 기본값 그대로 (옛 호출 호환). nil 이 아니면 title/
-- description 텍스트를 <span class='...'> 으로 감싸 RCSS 의 .mission / .success /
-- .failure 클래스에 정의된 색상을 적용.
-- 주의: SetInnerRML 사용이라 title/description 에 RML 메타문자 (< > &)가 들어오는
-- 경우는 이스케이프되어 있어야 한다. 본 게임의 텍스트는 한글/영문 평문이라 안전.
local function WrapStyleClass(text, styleClass)
    if styleClass == nil or text == nil or text == "" then
        return text or ""
    end
    return "<span class='" .. styleClass .. "'>" .. text .. "</span>"
end

function UIManager.ShowCriticalWarning(title, description, duration, onComplete, styleClass)
    local widget = widgets["criticalWarning"]
    if widget == nil then
        if onComplete ~= nil then
            onComplete()
        end
        return
    end

    StopCriticalWarningRoutine()
    UIManager.Show("criticalWarning")

    widget:set_text("critical-warning-title", WrapStyleClass(title or "WARNING", styleClass))
    widget:set_text("critical-warning-description", WrapStyleClass(description or "", styleClass))

    criticalWarningRoutine = StartCoroutine(function()
        Wait(duration)
        criticalWarningRoutine = nil
        UIManager.Hide("criticalWarning")

        if onComplete ~= nil then
            onComplete()
        end
    end)
end

-- ─── Mission feedback (start / result) ─────────────────────────────────
-- CriticalWarning 위젯을 재사용 — title/subtitle 텍스트와 RCSS 클래스를 페이즈별로 다르게.
-- 색상은 RCSS 의 .mission / .success / .failure 클래스에 정의 (set_property 우회).
local MISSION_INFOS = {
    [ECarGamePhase.CarWash]      = { title = "MISSION 1 - CAR WASH",      subtitle = "더러워진 차를 깨끗이 닦으세요"   },
    [ECarGamePhase.CarGas]       = { title = "MISSION 2 - GAS FILL",      subtitle = "주유기를 사용해 연료를 채우세요" },
    [ECarGamePhase.EscapePolice] = { title = "MISSION 3 - ESCAPE POLICE", subtitle = "경찰을 따돌리고 시간을 버티세요" },
    [ECarGamePhase.DodgeMeteor]  = { title = "MISSION 4 - DODGE METEOR",  subtitle = "운석을 피해 살아남으세요"        },
    [ECarGamePhase.Goal]         = { title = "FINAL  -  REACH THE GOAL", subtitle = "목표 지점에 도달하세요"          },
}
local MISSION_RESULT_DURATION = 1.6

function UIManager.ShowMissionStart(phase, onComplete)
    local info = MISSION_INFOS[phase]
    if info == nil then
        if onComplete ~= nil then onComplete() end
        return
    end
    UIManager.ShowCriticalWarning(info.title, info.subtitle, 1.8, onComplete, "mission")
end

function UIManager.ShowMissionResult(phase, result, onComplete)
    local phaseName = (MISSION_INFOS[phase] and MISSION_INFOS[phase].title) or "MISSION"
    local title, subtitle, styleClass
    if result == EPhaseResult.Success then
        title      = "MISSION CLEAR!"
        subtitle   = phaseName
        styleClass = "success"
    elseif result == EPhaseResult.Failed then
        title      = "MISSION FAILED"
        subtitle   = phaseName
        styleClass = "failure"
    else
        if onComplete ~= nil then onComplete() end
        return
    end
    UIManager.ShowCriticalWarning(title, subtitle, MISSION_RESULT_DURATION, onComplete, styleClass)
end

-- 게임 종료 화면 표시 — outcome 에 따라 타이틀 텍스트 swap.
-- finalScore 는 Score 시스템 도입 시 채워서 호출 (지금은 nil 이면 placeholder 유지).
-- 주의: UUIManager 가 RML Document 를 첫 AddToViewport 시점에 lazy-load 하므로
-- set_text 는 Show 이후에 호출해야 한다. 그 전엔 Document=null 이라 silent no-op.
function UIManager.ShowGameOver(outcome, finalScore, onScoreComplete)
    local widget = widgets["gameOver"]
    if widget == nil then return end

    UIManager.Show("gameOver")

    if outcome == EFinishOutcome.Win then
        widget:set_text("game-over-title", "GOAL IN")
        widget:set_text("game-over-kicker", "당신은 탈출할 수 없습니다.")
    else
        widget:set_text("game-over-title", "GAME OVER")
        widget:set_text("game-over-kicker", "GAME RESULT")
    end

    StopGameOverScoreRoutine()
    local scoreEvents = {}
    local targetFinalScore = finalScore or 0

    local gs = GetGameState()
    local eventTotal = 0
    -- 같은 Reason 의 점수 이벤트 (예: "HitPerson" +100 가 여러 번) 는 정산 화면에서
    -- 한 줄로 합쳐 표시한다. reasonGroups 가 누적, reasonOrder 가 첫 등장 순서를 유지.
    local reasonGroups = {}
    local reasonOrder = {}
    if gs ~= nil then
        local count = gs:GetScoreEventCount()
        for i = 0, count - 1 do
            local event = gs:GetScoreEvent(i)
            if event ~= nil then
                eventTotal = eventTotal + event.Amount
                local key = event.Reason or ""
                if reasonGroups[key] == nil then
                    reasonGroups[key] = { amount = 0, count = 0 }
                    table.insert(reasonOrder, key)
                end
                reasonGroups[key].amount = reasonGroups[key].amount + event.Amount
                reasonGroups[key].count  = reasonGroups[key].count + 1
            end
        end
        if finalScore == nil then
            targetFinalScore = gs:GetScore()
        end
    end

    for _, reason in ipairs(reasonOrder) do
        local group = reasonGroups[reason]
        local displayName = SCORE_REASON_LABELS[reason] or reason
        local label = displayName
        if group.count > 1 then
            label = string.format("%s x %d", displayName, group.count)
        end
        table.insert(scoreEvents, { amount = group.amount, reason = label })
    end

    local remainingScore = targetFinalScore - eventTotal
    if remainingScore ~= 0 then
        table.insert(scoreEvents, {
            amount = remainingScore,
            reason = "Final"
        })
    end

    if #scoreEvents > 0 then
        widget:set_text("final-score-value", FormatScore(0))
        widget:set_text("final-score-detail", "")
        StartGameOverScoreRoutine(scoreEvents, targetFinalScore, onScoreComplete)
    else
        widget:set_text("final-score-value", FormatScore(targetFinalScore))
        widget:set_text("final-score-detail", "")
        if onScoreComplete ~= nil then
            onScoreComplete()
        end
    end
end

function UIManager.Tick(dt)
    ApplyIntroLayout()

    if fade.active then
        fade.elapsed = fade.elapsed + dt
        local alpha = Clamp(fade.elapsed / fade.duration, 0, 1)
        local opacity = fade.from + (fade.to - fade.from) * alpha
        SetFadeOpacity(opacity)

        if alpha >= 1 then
            local onComplete = fade.onComplete
            local hideWhenDone = fade.hideWhenDone

            fade.active = false
            fade.onComplete = nil
            fade.hideWhenDone = false

            if hideWhenDone then
                UIManager.Hide("fade")
            end

            if onComplete ~= nil then
                onComplete()
            end
        end
    end

    UIManager.UpdateGasWidget()
    UIManager.UpdateMeteorHpWidget()
    UpdateScoreCounter(dt)
    UpdateScoreFeedback(dt)
end

function UIManager.GetWidget(key)
    return widgets[key]
end

-- ============================================================
-- HUD (GameOverlayWidget) 갱신
--
-- 매 프레임 폴링: time-value(매치 카운트다운), combo-value(페이즈 카운트다운).
-- 페이즈 전환 / 클리어 시 OnPhaseChanged 콜백에서 UpdateHUD 호출 → 즉시 라벨 갱신.
-- 게임오버레이가 viewport 에 없으면 no-op (intro 화면 동안).
-- ============================================================

local PHASE_OBJECTIVE = {
    [ECarGamePhase.None]         = "퀘스트를 수행하라",
    [ECarGamePhase.CarWash]      = "세차 진행 중",
    [ECarGamePhase.CarGas]       = "주유 중",
    [ECarGamePhase.EscapePolice] = "경찰 따돌리는 중",
    [ECarGamePhase.DodgeMeteor]  = "메테오 피하는 중",
    [ECarGamePhase.Goal]         = "골인 지점으로 이동 중",
    [ECarGamePhase.Finished]     = "매치 종료",
}

local PHASE_NAME = {
    [ECarGamePhase.CarWash]      = "CAR WASH",
    [ECarGamePhase.CarGas]       = "GAS FILL",
    [ECarGamePhase.EscapePolice] = "ESCAPE",
    [ECarGamePhase.DodgeMeteor]  = "METEOR DODGE",
    [ECarGamePhase.Goal]         = "GOAL",
}

local PHASE_HINT = {
    [ECarGamePhase.CarWash] = "SPACE : 물 뿌리기",
}

local function FormatTime(seconds)
    if seconds == nil or seconds < 0 then seconds = 0 end
    local m = math.floor(seconds / 60)
    local s = math.floor(seconds % 60)
    return string.format("%02d:%02d", m, s)
end

local function PopCount(mask)
    local n = 0
    while mask > 0 do
        if mask % 2 == 1 then n = n + 1 end
        mask = math.floor(mask / 2)
    end
    return n
end

local function GetObjectiveText(phase, lastEnded, lastResult)
    if phase == ECarGamePhase.Result then
        local name = PHASE_NAME[lastEnded] or "PHASE"
        if lastResult == EPhaseResult.Success then
            return name .. " CLEARED!"
        elseif lastResult == EPhaseResult.Failed then
            return name .. " FAILED"
        end
    end
    return PHASE_OBJECTIVE[phase] or ""
end

function UIManager.UpdateHUD()
    local gs = GetGameState()
    if gs == nil then return end
    local widget = widgets["gameOverlay"]
    if widget == nil or not widget:IsInViewport() then return end

    local phase = gs:GetPhase()

    -- 매치 전체 카운트다운 (항상 표시)
    widget:set_text("time-value", FormatTime(gs:GetRemainingMatchTime()))

    -- 페이즈 카운트다운 — 활성/Result 일 때만, 그 외 "--"
    if phase == ECarGamePhase.None or phase == ECarGamePhase.Finished then
        widget:set_text("combo-value", "--")
    else
        widget:set_text("combo-value", FormatTime(gs:GetRemainingPhaseTime()))
    end

    -- objective + 점수 — 매 프레임 호출 비용 미미하므로 같이 갱신
    widget:set_text("objective-value", GetObjectiveText(phase, gs:GetLastEndedPhase(), gs:GetLastPhaseResult()))
    widget:set_text("hud-bottom-tip", PHASE_HINT[phase] or "F2 : CAMERA")

    local score = gs:GetScore()
    if not scoreCounter.initialized then
        scoreCounter.initialized = true
        scoreCounter.display = score
        scoreCounter.target = score
        SetHudScoreText(scoreCounter.display)
    elseif score ~= scoreCounter.target then
        scoreCounter.target = score
        if scoreCounter.display > scoreCounter.target then
            scoreCounter.display = scoreCounter.target
            SetHudScoreText(scoreCounter.display)
        end
    end
    UpdateSpeedometerWidget(widget)

    -- HP — RML 에 hp-slot-0/1/2 슬롯이 있고 색만 채워진(빨강)/빈(회색) 으로 토글.
    PollScoreEvents(gs)

    local hp = gs:GetHealth()
    local maxHp = gs:GetMaxHealth()
    for i = 0, maxHp - 1 do
        local color = (i < hp) and "#f04444" or "#4a4a52"
        widget:set_property("hp-slot-" .. tostring(i), "color", color)
    end
end

function UIManager.UpdateGasWidget()
    local widget = widgets["gasWidget"]
    if widget == nil then return end

    local overlayWidget = widgets["gameOverlay"]
    if overlayWidget == nil or not overlayWidget:IsInViewport() then
        if widget:IsInViewport() then
            widget:hide()
        end
        return
    end

    local car = ObjRegistry.car
    if car == nil then return end

    local gas = car:GetCarGas()
    if gas == nil then return end

    if not widget:IsInViewport() then
        widget:show()
    end

    local currentGas = gas:GetGas()
    local maxGas = gas:GetMaxGas()
    local ratio = Clamp(gas:GetGasRatio(), 0, 1)
    local color = GetGasColor(ratio)
    
    widget:set_text("gas-value", string.format("%d / %d", math.floor(currentGas + 0.5), math.floor(maxGas + 0.5)))
    widget:set_property("gas-bar-fill", "width", string.format("%.1f%%", ratio * 100))
    widget:set_property("gas-bar-fill", "background-color", color)
    widget:set_property("gas-value", "color", color)
end

-- 메테오 HP 바 — Phase==DodgeMeteor 일 때만 표시. ObjRegistry.car 의 MeteorHealth 폴링.
function UIManager.UpdateMeteorHpWidget()
    local widget = widgets["meteorHp"]
    if widget == nil then return end

    local gs = GetGameState()
    if gs == nil then
        if widget:IsInViewport() then widget:hide() end
        return
    end

    local phase = gs:GetPhase()
    local bShouldShow = (phase == ECarGamePhase.DodgeMeteor)
    if not bShouldShow then
        if widget:IsInViewport() then widget:hide() end
        return
    end

    local car = ObjRegistry.car
    if car == nil then
        if widget:IsInViewport() then widget:hide() end
        return
    end

    if not widget:IsInViewport() then
        widget:show()
    end

    local hp    = car:GetMeteorHealth()
    local maxHp = car:GetMaxMeteorHealth()
    if maxHp <= 0 then maxHp = 1 end
    local ratio = Clamp(hp / maxHp, 0, 1)

    -- HP 비율에 따라 색 — 높음 빨강 / 낮음 더 진한 빨강(거의 죽음 경고)
    local color
    if ratio <= 0.25 then
        color = "#a01010"
    elseif ratio <= 0.5 then
        color = "#d83030"
    else
        color = "#f04444"
    end

    widget:set_text("meteor-hp-value", string.format("%d / %d", math.floor(hp + 0.5), math.floor(maxHp + 0.5)))
    widget:set_property("meteor-hp-bar-fill", "width", string.format("%.1f%%", ratio * 100))
    widget:set_property("meteor-hp-bar-fill", "background-color", color)
end

return UIManager
