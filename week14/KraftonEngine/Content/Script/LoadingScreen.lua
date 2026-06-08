local LoadingScreen = {}

local WIDGET_PATH = "Content/UI/Common/LoadingScreen.rml"
local DEFAULT_DURATION = 5.0
local TIP_INTERVAL = 5.0

local widget = nil
local active = false
local elapsed = 0.0
local duration = DEFAULT_DURATION
local tipTimer = 0.0
local currentTipIndex = 0
local statusDotTimer = 0.0
local statusDotCount = 0
local keepVisibleOnComplete = false

local tips = {
    {
        title = "[ VANTUS ]",
        body = "VANTUS Corporation의 공식 업종은 '행성 자원 개발 및 식민지 인프라 구축'이다. 창립 이래 단 한 건의 소송도 패소한 적이 없다. 기록에 남은 소송 자체가 없기 때문이다.",
    },
    {
        title = "[ VANTUS ]",
        body = "VANTUS는 매년 은하 경제 기여도 상위 3위 안에 든다. 어떤 행성이 그 숫자를 만들었는지는 보고서에 기재되지 않는다.",
    },
    {
        title = "[ VANTUS ]",
        body = "VANTUS 내부에는 '임무 등급'이 존재한다. 대부분의 요원은 C등급 이상의 임무가 무엇을 의미하는지 알지 못한다. 알게 되는 시점은 보통 너무 늦다.",
    },
    {
        title = "[ LANCER ]",
        body = "LANCER 플랫폼은 공식적으로 존재하지 않는다. VANTUS 내부 문서에서도 '궤도 자원 회수 보조 시스템'으로만 기재되어 있다. 실제로 자원을 회수한 적은 없다.",
    },
    {
        title = "[ LANCER ]",
        body = "LANCER의 첫 실전 사용 기록은 삭제되어 있다. 해당 행성의 이름도, 그 행성에 살았던 사람들의 수도 남아 있지 않다.",
    },
    {
        title = "[ 이온(Eion) ]",
        body = "이온은 한때 은하 항로 지도에 녹색으로 표시된 행성이었다. 지금은 그 색이 바뀌었다. 지도를 수정한 것은 VANTUS다.",
    },
    {
        title = "[ 이온(Eion) ]",
        body = "이온의 지하 깊숙이에는 고밀도 에너지 광물층이 분포한다. 이온 원주민들은 그것을 땅의 숨결이라 불렀다. VANTUS는 채굴 코드 E-7로 분류했다.",
    },
    {
        title = "[ 이온(Eion) ]",
        body = "이온에 VANTUS가 진입한 것은 3년 전이다. 당시 협약서에는 '지속 가능한 공동 개발'이라는 문구가 포함되어 있었다.",
    },
    {
        title = "[ ASHBORNE ]",
        body = "Ashborne는 테러 조직으로 공식 분류되어 있다. 분류한 것은 VANTUS다. 해당 분류에 이의를 제기할 수 있는 독립 기관은 이온에 존재하지 않는다.",
    },
    {
        title = "[ ASHBORNE ]",
        body = "Ashborne의 무기 대부분은 폐기된 VANTUS 장비를 개조한 것이다. 그들이 싸우는 총은 그들을 착취한 회사가 만들었다.",
    },
    {
        title = "[ 제임스 매커슨 ]",
        body = "제임스 매커슨은 VANTUS 재직 당시 우수 엔지니어 표창을 두 차례 수상했다. 이후 그의 기록은 사내 시스템에서 전면 삭제되었다. 삭제 사유는 공란으로 남아 있다.",
    },
    {
        title = "[ 케인 / BLADE 프로그램 ]",
        body = "VANTUS의 특수 전술 요원 육성 프로그램은 공식 명칭이 없다. 참여자 대부분은 선택의 여지가 있었는지 기억하지 못한다.",
    },
    {
        title = "[ 케인 / BLADE 프로그램 ]",
        body = "BLADE 등급 요원의 전술 슈트는 벽면 주행, 활강, 다단 도약을 지원한다. 설계 목표는 단 하나였다. 어떤 지형에서도 표적에 도달할 것.",
    },
    {
        title = "[ 세계관 / 기타 ]",
        body = "26세기 기준, 인류가 거주 가능 판정을 받은 행성은 총 4,000여 개다. 그 중 VANTUS가 '개발 완료' 처리한 행성이 몇 개인지는 공개된 적이 없다.",
    },
    {
        title = "[ 세계관 / 기타 ]",
        body = "궤도에서 내려다보면 이온은 아직 아름답다. 채굴 흉터는 지표면에서만 보인다.",
    },
}

local function set_tip(index)
    if widget == nil or #tips <= 0 then
        return
    end

    currentTipIndex = index
    if currentTipIndex < 1 or currentTipIndex > #tips then
        currentTipIndex = 1
    end

    local tip = tips[currentTipIndex]
    widget:SetText("loading-title", tip.title or "")
    widget:SetText("loading-tip", tip.body or "")
end

local function choose_initial_tip_index()
    if #tips <= 0 then
        return 1
    end

    local seed = 1
    if Engine ~= nil and Engine.GetViewportSize ~= nil then
        local size = Engine.GetViewportSize()
        seed = math.floor((size.Width or 1280) + (size.Height or 720) * 3)
    end
    return (seed % #tips) + 1
end

local function update_status_text()
    if widget == nil then
        return
    end

    local dots = ""
    for _ = 1, statusDotCount do
        dots = dots .. "."
    end
    widget:SetText("loading-status", "LOADING" .. dots)
end

function LoadingScreen.Show(config)
    config = config or {}
    LoadingScreen.Hide()

    duration = config.duration or DEFAULT_DURATION
    elapsed = 0.0
    tipTimer = 0.0
    statusDotTimer = 0.0
    statusDotCount = 0
    keepVisibleOnComplete = config.keepVisibleOnComplete == true
    active = true

    widget = UI.CreateWidget(WIDGET_PATH)
    if widget ~= nil then
        widget:SetWantsMouse(false)
        widget:AddToViewportZ(config.zOrder or 240)
        set_tip(config.tipIndex or choose_initial_tip_index())
        update_status_text()
    end
end

function LoadingScreen.Hide()
    if widget ~= nil and widget:IsInViewport() then
        widget:RemoveFromParent()
    end
    widget = nil
    active = false
    elapsed = 0.0
    keepVisibleOnComplete = false
end

function LoadingScreen.IsActive()
    return active
end

function LoadingScreen.Tick(dt)
    if not active then
        return false
    end

    dt = dt or 0.0
    elapsed = elapsed + dt
    tipTimer = tipTimer + dt
    statusDotTimer = statusDotTimer + dt

    if statusDotTimer >= 0.32 then
        statusDotTimer = 0.0
        statusDotCount = (statusDotCount + 1) % 4
        update_status_text()
    end

    if tipTimer >= TIP_INTERVAL and #tips > 1 then
        tipTimer = 0.0
        set_tip((currentTipIndex % #tips) + 1)
    end

    if elapsed >= duration then
        if not keepVisibleOnComplete then
            LoadingScreen.Hide()
        end
        return true
    end

    return false
end

return LoadingScreen
