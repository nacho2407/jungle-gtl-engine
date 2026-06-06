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

local widget = nil
local camera = nil
local owner = nil
local maxRange = 200.0
local weaponSpread = 0.0
local hitMarkerTimer = 0.0
local killMarkerTimer = 0.0
local currentAmmo = 0
local magazineSize = 0

local function px(value)
    return string.format("%.2fpx", value)
end

local function is_target_actor(actor)
    return actor ~= nil and actor:GetSkeletalMesh() ~= nil
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

    local spreadOffset = weaponSpread * CROSSHAIR_SPREAD_PX
    set_crosshair_part_position("crosshair-left-white", "crosshair-left-red", "left", CROSSHAIR_BASE_LEFT - spreadOffset)
    set_crosshair_part_position("crosshair-right-white", "crosshair-right-red", "left", CROSSHAIR_BASE_RIGHT + spreadOffset)
    set_crosshair_part_position("crosshair-top-white", "crosshair-top-red", "top", CROSSHAIR_BASE_TOP - spreadOffset)
    set_crosshair_part_position("crosshair-bottom-white", "crosshair-bottom-red", "top", CROSSHAIR_BASE_BOTTOM + spreadOffset)

    local isTargeted = false
    if camera ~= nil then
        local camPos = camera:GetWorldLocation()
        local camFwd = camera.Forward
        local hit = World.RaycastSkeletalMesh(camPos, camFwd, maxRange, owner)
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

    WeaponHud.SetAmmo(currentAmmo, magazineSize)
    update_crosshair()
end

function WeaponHud.Shutdown()
    if widget ~= nil and widget:IsInViewport() then
        widget:RemoveFromParent()
    end

    widget = nil
    camera = nil
    owner = nil
    weaponSpread = 0.0
    hitMarkerTimer = 0.0
    killMarkerTimer = 0.0
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
    if spread ~= nil then
        weaponSpread = spread
    end

    if hitMarkerTimer > 0.0 then
        hitMarkerTimer = hitMarkerTimer - dt
        if hitMarkerTimer < 0.0 then hitMarkerTimer = 0.0 end
    end

    if killMarkerTimer > 0.0 then
        killMarkerTimer = killMarkerTimer - dt
        if killMarkerTimer < 0.0 then killMarkerTimer = 0.0 end
    end

    update_crosshair()
end

return WeaponHud
