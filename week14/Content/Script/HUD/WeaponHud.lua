local CombatEvents = require("Game.CombatEvents")

local WeaponHud = {}

local WIDGET_PATH = "Content/UI/HUD/WeaponHUD.rml"
local DEFAULT_Z_ORDER = 80

local CROSSHAIR_BASE_LEFT   = 16
local CROSSHAIR_BASE_RIGHT  = 80
local CROSSHAIR_BASE_TOP    = 16
local CROSSHAIR_BASE_BOTTOM = 80
local CROSSHAIR_SPREAD_PX   = 34
local HIT_MARKER_DURATION   = 0.12
local KILL_MARKER_DURATION  = 0.22
local LETTERBOX_HEIGHT_RATIO = 0.135
local LETTERBOX_ANIM_DURATION = 0.35
local SKIP_RING_FRAME_COUNT = 25

local widget = nil
local camera = nil
local owner = nil
local movement = nil
local maxRange = 200.0
local weaponSpread = 0.0
local hitMarkerTimer = 0.0
local killMarkerTimer = 0.0
local currentAmmo = 0
local magazineSize = 0
local bVisible = true
local bLetterboxVisible = false
local letterboxProgress = 0.0
local bDialogueVisible = false
local dialogueOpacity = 0.0
local bSkipPromptVisible = false
local skipProgress = 0.0
local scoreWarningLogged = false

local function px(value)
    return string.format("%.2fpx", value)
end

local function clamp01(value)
    if value < 0.0 then return 0.0 end
    if value > 1.0 then return 1.0 end
    return value
end

local function get_viewport_size()
    if Engine ~= nil and Engine.GetViewportSize ~= nil then
        local size = Engine.GetViewportSize()
        if size ~= nil then
            local width = size.Width or 0.0
            local height = size.Height or 0.0
            if width > 0.0 and height > 0.0 then
                return width, height
            end
        end
    end

    return 1920.0, 1080.0
end

local function update_score()
    if widget == nil then return end
    if ScoreManager == nil or ScoreManager.GetSnapshot == nil then
        if not scoreWarningLogged and Game ~= nil and Game.Log ~= nil then
            Game.Log("[WeaponHud]", "ScoreManager.GetSnapshot is not available")
            scoreWarningLogged = true
        end
        widget:SetText("score-hud-value", "000000")
        return
    end

    local snapshot = ScoreManager.GetSnapshot()
    if snapshot == nil or snapshot.score == nil then
        if not scoreWarningLogged and Game ~= nil and Game.Log ~= nil then
            Game.Log("[WeaponHud]", "Score snapshot has no score value")
            scoreWarningLogged = true
        end
        widget:SetText("score-hud-value", "000000")
        return
    end
    widget:SetText("score-hud-value", string.format("%06d", snapshot.score or 0))
end

local function update_speed()
    if widget == nil then return end

    local speed = 0.0
    if movement ~= nil and movement.GetSpeed ~= nil then
        speed = movement:GetSpeed() or 0.0
    end
    speed = speed * 0.5

    widget:SetText("speed-hud-value", string.format("%.1f m/s", speed))
end

local function apply_visibility()
    if widget == nil then return end

    -- 컷씬 중에는 HUD 위젯 인스턴스와 탄약 상태는 유지하고, 화면 표시만 끕니다.
    local display = bVisible and "block" or "none"
    widget:SetProperty("crosshair-screen", "display", display)
    widget:SetProperty("weapon-hud-root", "display", display)
end

local function apply_dialogue_visibility()
    if widget == nil then return end

    widget:SetProperty("hud-dialogue-box", "display", bDialogueVisible and "block" or "none")
    widget:SetProperty("hud-dialogue-box", "opacity", string.format("%.2f", dialogueOpacity))
end

local function apply_skip_prompt_visibility()
    if widget == nil then return end

    local display = bSkipPromptVisible and "block" or "none"
    widget:SetProperty("hud-cutscene-skip-prompt", "display", display)
    widget:SetProperty("hud-skip-ring", "display", display)
end

local function apply_skip_ring_progress()
    if widget == nil then return end

    local clamped = clamp01(skipProgress or 0.0)
    widget:SetProperty("hud-skip-ring", "opacity", clamped > 0.001 and "1.0" or "0.0")

    local visibleIndex = math.floor(clamped * (SKIP_RING_FRAME_COUNT - 1) + 0.5)
    for index = 0, SKIP_RING_FRAME_COUNT - 1 do
        local id = string.format("hud-skip-ring-frame-%02d", index)
        widget:SetProperty(id, "opacity", index == visibleIndex and "1.0" or "0.0")
    end
end

local function apply_letterbox_visibility()
    if widget == nil then return end

    -- 레터박스는 display를 유지한 상태에서 위/아래 바의 위치만 보간합니다.
    local shouldDisplay = bLetterboxVisible or letterboxProgress > 0.001
    local _, viewportHeight = get_viewport_size()
    local barHeight = viewportHeight * LETTERBOX_HEIGHT_RATIO
    local offset = -barHeight * (1.0 - letterboxProgress)

    widget:SetProperty("hud-letterbox-layer", "display", shouldDisplay and "block" or "none")
    widget:SetProperty("hud-letterbox-top", "top", px(offset))
    widget:SetProperty("hud-letterbox-bottom", "bottom", px(offset))
end

local function update_letterbox(dt)
    local target = bLetterboxVisible and 1.0 or 0.0
    if math.abs(letterboxProgress - target) <= 0.001 then
        letterboxProgress = target
        apply_letterbox_visibility()
        return
    end

    local duration = LETTERBOX_ANIM_DURATION
    local step = duration > 0.0 and ((dt or 0.0) / duration) or 1.0
    if target > letterboxProgress then
        letterboxProgress = clamp01(letterboxProgress + step)
    else
        letterboxProgress = clamp01(letterboxProgress - step)
    end

    apply_letterbox_visibility()
end

local function is_target_actor(actor)
    return CombatEvents.IsDamageable(actor)
end

local function set_crosshair_color(isRed)
    if widget == nil then return end

    local whiteOpacity = isRed and "0.0" or "0.9"
    local redOpacity   = isRed and "0.9" or "0.0"

    widget:SetProperty("crosshair-left-white", "opacity", whiteOpacity)
    widget:SetProperty("crosshair-right-white", "opacity", whiteOpacity)
    widget:SetProperty("crosshair-top-white", "opacity", whiteOpacity)
    widget:SetProperty("crosshair-bottom-white", "opacity", whiteOpacity)
    widget:SetProperty("crosshair-dot-white", "opacity", whiteOpacity)

    widget:SetProperty("crosshair-left-red", "opacity", redOpacity)
    widget:SetProperty("crosshair-right-red", "opacity", redOpacity)
    widget:SetProperty("crosshair-top-red", "opacity", redOpacity)
    widget:SetProperty("crosshair-bottom-red", "opacity", redOpacity)
    widget:SetProperty("crosshair-dot-red", "opacity", redOpacity)
end

local function set_crosshair_part_position(idWhite, idRed, prop, value)
    if widget == nil then return end

    local valuePx = px(value)
    widget:SetProperty(idWhite, prop, valuePx)
    widget:SetProperty(idRed, prop, valuePx)
end

local function update_crosshair()
    if widget == nil then return end
    if not bVisible then return end

    local spreadOffset = weaponSpread * CROSSHAIR_SPREAD_PX
    set_crosshair_part_position("crosshair-left-white", "crosshair-left-red", "left", CROSSHAIR_BASE_LEFT - spreadOffset)
    set_crosshair_part_position("crosshair-right-white", "crosshair-right-red", "left", CROSSHAIR_BASE_RIGHT + spreadOffset)
    set_crosshair_part_position("crosshair-top-white", "crosshair-top-red", "top", CROSSHAIR_BASE_TOP - spreadOffset)
    set_crosshair_part_position("crosshair-bottom-white", "crosshair-bottom-red", "top", CROSSHAIR_BASE_BOTTOM + spreadOffset)

    local isTargeted = false
    if camera ~= nil then
        local camPos = camera:GetWorldLocation()
        local camFwd = camera.Forward
        local hit = World.RaycastPrimitive(camPos, camFwd, maxRange, owner)
        isTargeted = hit ~= nil and is_target_actor(hit.HitActor)
    end
    set_crosshair_color(isTargeted)

    local hitOpacity = 0.0
    if hitMarkerTimer > 0.0 then
        hitOpacity = hitMarkerTimer / HIT_MARKER_DURATION
        if hitOpacity > 1.0 then hitOpacity = 1.0 end
        hitOpacity = hitOpacity * 0.9
    end

    local killOpacity = 0.0
    if killMarkerTimer > 0.0 then
        killOpacity = killMarkerTimer / KILL_MARKER_DURATION
        if killOpacity > 1.0 then killOpacity = 1.0 end
        killOpacity = killOpacity * 0.9
    end
    widget:SetProperty("crosshair-hit-marker", "opacity", string.format("%.2f", hitOpacity))
    widget:SetProperty("crosshair-kill-marker", "opacity", string.format("%.2f", killOpacity))
end

function WeaponHud.Initialize(config)
    config = config or {}
    camera = config.camera
    owner = config.owner
    movement = config.movement
    maxRange = config.maxRange or maxRange
    magazineSize = config.magazineSize or magazineSize
    currentAmmo = config.currentAmmo or currentAmmo
    weaponSpread = config.weaponSpread or 0.0
    hitMarkerTimer = 0.0
    killMarkerTimer = 0.0

    if widget == nil then
        widget = UI.CreateWidget(WIDGET_PATH)
    end

    if widget ~= nil and not widget:IsInViewport() then
        widget:AddToViewportZ(config.zOrder or DEFAULT_Z_ORDER)
    end

    apply_visibility()
    apply_letterbox_visibility()
    apply_dialogue_visibility()
    apply_skip_prompt_visibility()
    apply_skip_ring_progress()
    WeaponHud.SetAmmo(currentAmmo, magazineSize)
    update_score()
    update_speed()
    update_crosshair()
end

function WeaponHud.Shutdown()
    if widget ~= nil and widget:IsInViewport() then
        widget:RemoveFromParent()
    end

    widget = nil
    camera = nil
    owner = nil
    movement = nil
    weaponSpread = 0.0
    hitMarkerTimer = 0.0
    killMarkerTimer = 0.0
    bVisible = true
    bLetterboxVisible = false
    letterboxProgress = 0.0
    bDialogueVisible = false
    dialogueOpacity = 0.0
    bSkipPromptVisible = false
    skipProgress = 0.0
    scoreWarningLogged = false
end

function WeaponHud.SetVisible(visible)
    bVisible = visible ~= false
    apply_visibility()
    if bVisible then
        update_crosshair()
    end
end

-- 크로스헤어(가운데 십자/도트)만 따로 끄고 켠다. 탄약 카운터 등 weapon-hud-root 는 유지.
-- ADS 같이 조준경으로 보는 동안 v_cro 가 시야를 가리는 걸 피하려고 사용.
function WeaponHud.SetCrosshairVisible(visible)
    if widget == nil then return end
    if not bVisible then return end
    widget:SetProperty("crosshair-screen", "display", (visible ~= false) and "block" or "none")
end

function WeaponHud.ShowLetterbox(config)
    bLetterboxVisible = true
    if config ~= nil and config.instant == true then
        letterboxProgress = 1.0
    end
    apply_letterbox_visibility()
end

function WeaponHud.HideLetterbox(config)
    bLetterboxVisible = false
    if config ~= nil and config.instant == true then
        letterboxProgress = 0.0
    end
    apply_letterbox_visibility()
end

function WeaponHud.SetAmmo(inCurrentAmmo, inMagazineSize)
    currentAmmo = inCurrentAmmo or currentAmmo
    magazineSize = inMagazineSize or magazineSize

    if widget == nil then return end
    widget:SetText("weapon-hud-current-ammo", tostring(currentAmmo))
    widget:SetText("weapon-hud-magazine-ammo", tostring(magazineSize))
end

function WeaponHud.SetSpread(spread)
    weaponSpread = spread or 0.0
    update_crosshair()
end

function WeaponHud.ShowDialogue(text, config)
    config = config or {}

    if widget == nil then return end

    local width = config.width or 980.0
    local height = config.height or 48.0
    local fontSize = config.fontSize or 22.0
    local lineHeight = config.lineHeight or height
    local opacity = config.opacity or 0.0
    local viewportWidth, viewportHeight = get_viewport_size()
    local left = config.left or ((viewportWidth - width) * 0.5)
    local bottom = config.bottom or (viewportHeight * 0.135 + 18.0)

    bDialogueVisible = true
    dialogueOpacity = opacity

    widget:SetText("hud-dialogue-line", text or "")
    widget:SetProperty("hud-dialogue-box", "display", "block")
    widget:SetProperty("hud-dialogue-box", "left", px(left))
    widget:SetProperty("hud-dialogue-box", "bottom", px(bottom))
    widget:SetProperty("hud-dialogue-box", "width", px(width))
    widget:SetProperty("hud-dialogue-box", "height", px(height))
    widget:SetProperty("hud-dialogue-box", "opacity", string.format("%.2f", dialogueOpacity))
    widget:SetProperty("hud-dialogue-line", "left", px(config.textLeft or 16.0))
    widget:SetProperty("hud-dialogue-line", "top", "0px")
    widget:SetProperty("hud-dialogue-line", "width", px(width - (config.textLeft or 16.0) * 2.0))
    widget:SetProperty("hud-dialogue-line", "height", px(height))
    widget:SetProperty("hud-dialogue-line", "font-family", config.font or "Pretendard")
    widget:SetProperty("hud-dialogue-line", "font-size", px(fontSize))
    widget:SetProperty("hud-dialogue-line", "font-weight", tostring(config.weight or 700))
    widget:SetProperty("hud-dialogue-line", "line-height", px(lineHeight))
end

function WeaponHud.SetDialogueOpacity(opacity)
    dialogueOpacity = opacity or 0.0
    apply_dialogue_visibility()
end

function WeaponHud.HideDialogue()
    bDialogueVisible = false
    dialogueOpacity = 0.0
    if widget == nil then return end
    widget:SetText("hud-dialogue-line", "")
    apply_dialogue_visibility()
end

function WeaponHud.ShowSkipPrompt(config)
    config = config or {}
    bSkipPromptVisible = true

    if widget ~= nil then
        local keyboardText = config.keyboardText
        local gamepadText = config.gamepadText
        if (keyboardText == nil or gamepadText == nil) and config.keyText ~= nil then
            -- 기존 호출부의 "Ctrl / B" 형식도 키보드 사각형과 패드 원형으로 나누어 표시합니다.
            keyboardText, gamepadText = string.match(config.keyText, "^%s*(.-)%s*/%s*(.-)%s*$")
            keyboardText = keyboardText or config.keyText
        end

        widget:SetText("hud-cutscene-skip-keyboard", keyboardText or "Ctrl")
        widget:SetText("hud-cutscene-skip-separator", "/")
        widget:SetText("hud-cutscene-skip-gamepad", gamepadText or "B")
    end
    if widget ~= nil and config.label ~= nil then
        widget:SetText("hud-cutscene-skip-label", config.label)
    end

    apply_skip_prompt_visibility()
    apply_skip_ring_progress()
end

function WeaponHud.HideSkipPrompt()
    bSkipPromptVisible = false
    skipProgress = 0.0
    apply_skip_prompt_visibility()
    apply_skip_ring_progress()
end

function WeaponHud.SetSkipProgress(progress)
    skipProgress = clamp01(progress or 0.0)
    apply_skip_ring_progress()
end

function WeaponHud.TriggerHitMarker()
    hitMarkerTimer = HIT_MARKER_DURATION
    update_crosshair()
end

function WeaponHud.TriggerKillMarker()
    hitMarkerTimer = 0.0
    killMarkerTimer = KILL_MARKER_DURATION
    update_crosshair()
end

function WeaponHud.Tick(dt, spread)
    dt = dt or 0.0

    if spread ~= nil then
        weaponSpread = spread
    end

    update_letterbox(dt)

    if hitMarkerTimer > 0.0 then
        hitMarkerTimer = hitMarkerTimer - dt
        if hitMarkerTimer < 0.0 then hitMarkerTimer = 0.0 end
    end

    if killMarkerTimer > 0.0 then
        killMarkerTimer = killMarkerTimer - dt
        if killMarkerTimer < 0.0 then killMarkerTimer = 0.0 end
    end

    update_score()
    update_speed()
    update_crosshair()
end

return WeaponHud
