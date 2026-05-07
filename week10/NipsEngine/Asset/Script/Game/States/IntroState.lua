local IntroState = {}
IntroState.__index = IntroState

local FIRST_PAGE_DELAY = 1.0

local function Utf8NextIndex(text, byteIndex)
    local byte = string.byte(text, byteIndex)
    if byte == nil then
        return byteIndex + 1
    end

    if byte < 0x80 then
        return byteIndex + 1
    elseif byte < 0xE0 then
        return byteIndex + 2
    elseif byte < 0xF0 then
        return byteIndex + 3
    end

    return byteIndex + 4
end

local function Utf8CharCount(text)
    local count = 0
    local index = 1

    while index <= #text do
        index = Utf8NextIndex(text, index)
        count = count + 1
    end

    return count
end

local function Utf8SubChars(text, charCount)
    local index = 1
    local count = 0

    while index <= #text and count < charCount do
        index = Utf8NextIndex(text, index)
        count = count + 1
    end

    return string.sub(text, 1, index - 1)
end

function IntroState.new()
    return setmetatable({
        uiHandle = nil,
        returnTo = "Title",
        pageIndex = 1,
        visibleChars = 0.0,
        pageSoundPlayed = false,
        pageCharCount = 0,
        pageFinished = false,
        finished = false,
        firstPageDelayRemaining = 0.0,
        charsPerSecond = 34.0,
        pages = {
            "나는 단지 세상을 보고 싶었을 뿐이다.",
            "평생을 어둠 속에서 살다가 전 재산을 들여 인공눈을 이식했다.",
            "하지만 수술이 끝나고 처음 마주한 세상은 상상과 전혀 달랐다.",
            "정교한 풍경 대신 눈앞에 펼쳐진 건 근사화된 네모난 세상뿐이었다.",
            "주변의 모든 것은 정육면체 덩어리로 보이기 시작했고,",
            "그들은 기괴한 소리를 내며 나를 향해 달려들고 있다.",
            "이게 인공눈의 오작동인지, 아니면 세상의 진짜 모습인지 생각할 겨를도 없다.",
            "살고 싶다면, 몰려드는 네모들을 베어내고 이 기괴한 공간을 탈출해야 한다."
        }
    }, IntroState)
end

function IntroState:Enter(context, payload)
    payload = payload or {}

    self.returnTo = payload.returnTo or "Title"
    self.pageIndex = 1
    self.visibleChars = 0.0
    self.pageSoundPlayed = false
    self.pageFinished = false
    self.finished = false
    self.firstPageDelayRemaining = FIRST_PAGE_DELAY

    context.managers.UI:Show("Intro")
    context.managers.Sound:StopBGM(0.5)
    Engine.API.Input.SetInputModeGameAndUI()
    Engine.API.Debug.Log("Intro State")

    self:RefreshPage(context)

    self.uiHandle = context.eventBus:Subscribe("UI.Action", self, function(event)
        if event.name == "SkipIntro" then
            self:Finish(context)
        end
    end)
end

function IntroState:Exit(context)
    context.eventBus:Unsubscribe(self.uiHandle)
    self.uiHandle = nil
end

function IntroState:Tick(context, dt)
    if self.finished then
        return
    end

    if self.firstPageDelayRemaining > 0.0 then
        local realDt = Engine.API.World.GetUnscaledDeltaTime()
        if realDt <= 0.0 then
            realDt = dt or 0.0
        end
        self.firstPageDelayRemaining = math.max(0.0, self.firstPageDelayRemaining - realDt)
        return
    end

    if Engine.API.Input.IsKeyPressed("Space") then
        if self.pageFinished then
            self:Advance(context)
        else
            self.visibleChars = self.pageCharCount
            self.pageFinished = true
            self:RefreshPage(context)
        end

        return
    end

    if self.pageFinished then
        return
    end

    local realDt = Engine.API.World.GetUnscaledDeltaTime()
    self.visibleChars = self.visibleChars + realDt * self.charsPerSecond

    if self.visibleChars >= self.pageCharCount then
        self.visibleChars = self.pageCharCount
        self.pageFinished = true
    end

    self:RefreshPage(context)
end

function IntroState:Advance(context)
    if self.pageIndex >= #self.pages then
        self:Finish(context)
        return
    end

    self.pageIndex = self.pageIndex + 1
    self.visibleChars = 0.0
    self.pageSoundPlayed = false
    self.pageFinished = false
    self:RefreshPage(context)
end

function IntroState:Finish(context)
    if self.finished then
        return
    end

    self.finished = true
    if self.returnTo == "Title" and context.root:OpenMainScene() then
        return
    end
    context.stateMachine:Change(self.returnTo or "Title")
end

function IntroState:RefreshPage(context)
    local text = self.pages[self.pageIndex] or ""
    self.pageCharCount = Utf8CharCount(text)

    local visibleCount = math.floor(self.visibleChars)
    if visibleCount > self.pageCharCount then
        visibleCount = self.pageCharCount
    end

    if visibleCount > 0 and not self.pageSoundPlayed then
        context.managers.Sound:PlayIntroText()
        self.pageSoundPlayed = true
    end

    context.managers.UI:SetIntroText(Utf8SubChars(text, visibleCount))
    context.managers.UI:SetIntroProgress(self.pageIndex, #self.pages)
    context.managers.UI:SetIntroContinueVisible(self.pageFinished)
end

return IntroState
