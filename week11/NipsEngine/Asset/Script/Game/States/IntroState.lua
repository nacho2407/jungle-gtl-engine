local IntroState = {}
IntroState.__index = IntroState

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
        charsPerSecond = 34.0,
        pages = {
            "엄마가 칼 가지고 장난치지 말랬지!",
            "마감일이 하루 남았네요 샤갈",
            "네 이제 게임을 시작합니다 야호"
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
