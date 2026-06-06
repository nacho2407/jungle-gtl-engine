local arms   = nil
local camera = nil
local weaponHudWidget = nil
local debugWidget = nil
local movement = nil
local DamagePostProcess = require("DamagePostProcess")

local SHOOT_ANIM_PATH  = "Content/Data/FPSArm/Test1/Test1_Armature_Armature_Arms_FPS_Anim_OneShot.uasset"
local RELOAD_ANIM_PATH = "Content/Data/FPSArm/Test1/Test1_Armature_Armature_Arms_FPS_Anim_Reload_Fast.uasset"

-- 본 기반 머즐은 Arm_R 이 어깨 관절이라 카메라랑 거의 같은 위치 → 의미 없음.
-- 대부분 FPS 는 카메라에서 forward offset 으로 머즐을 근사함.
local MUZZLE_FWD_OFFSET = 0.4   -- 카메라 앞 0.4m 지점을 머즐로 간주

local MAX_RANGE        = 200.0
local FIRE_INTERVAL    = 0.12
local MAGAZINE_SIZE    = 50
local RELOAD_DURATION  = 1.1

local SPREAD_PER_SHOT       = 0.18
local SPREAD_DECAY_PER_SEC  = 1.15
local MAX_SPREAD            = 1.0
local MAX_SPREAD_ANGLE      = 0.055
local CROSSHAIR_BASE_LEFT   = 16
local CROSSHAIR_BASE_RIGHT  = 80
local CROSSHAIR_BASE_TOP    = 16
local CROSSHAIR_BASE_BOTTOM = 80
local CROSSHAIR_SPREAD_PX   = 24
local KILL_MARKER_DURATION  = 0.22

local MAX_HITS_FOR_FULL_RED = 8
local MAX_HEALTH = 100.0
local DEBUG_Z_ORDER = 220
local DEBUG_SPEED_STEP = 0.25
local DEBUG_DAMAGE_STEP = 5.0
local DEBUG_HEALTH_STEP = 5.0

local fireCooldown = 0.0
local reloadTimer  = 0.0
local currentAmmo  = MAGAZINE_SIZE
local isReloading  = false
local hitCounts    = {}
local weaponSpread = 0.0
local killMarkerTimer = 0.0
local currentHealth = MAX_HEALTH
local isInvincible = false
local debugSpeedMultiplier = 1.0
local debugDamageAmount = 10.0
local debugActiveTab = "player"
local debugWindowX = 40.0
local debugWindowY = 48.0
local debugDragging = false
local debugMouseOver = false
local debugDragOffsetX = 0.0
local debugDragOffsetY = 0.0
local baseMaxWalkSpeed = nil
local baseSprintSpeedMultiplier = nil
local baseWallRunMaxSpeed = nil

local function px(value)
    return string.format("%.2fpx", value)
end

local function format_number(value, decimals)
    return string.format("%." .. tostring(decimals) .. "f", value)
end

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function get_health_ratio()
    if MAX_HEALTH <= 0.0 then return 0.0 end
    return clamp(currentHealth / MAX_HEALTH, 0.0, 1.0)
end

local function apply_player_damage(amount)
    amount = amount or 0.0
    if amount <= 0.0 then return end
    if isInvincible then return end

    currentHealth = clamp(currentHealth - amount, 0.0, MAX_HEALTH)
    local ratio = get_health_ratio()
    DamagePostProcess.TriggerHit(ratio)
    DamagePostProcess.SetHealthRatio(ratio)
end

local function set_player_health(value)
    currentHealth = clamp(value, 0.0, MAX_HEALTH)
    DamagePostProcess.SetHealthRatio(get_health_ratio())
end

local function cache_movement_defaults()
    if obj == nil or type(obj.GetCharacterMovement) ~= "function" then
        movement = nil
        return
    end

    movement = obj:GetCharacterMovement()
    if movement == nil then return end

    if baseMaxWalkSpeed == nil then
        baseMaxWalkSpeed = movement:GetMaxWalkSpeed()
        baseSprintSpeedMultiplier = movement:GetSprintSpeedMultiplier()
        baseWallRunMaxSpeed = movement:GetWallRunMaxSpeed()
    end
end

local function apply_debug_speed_multiplier()
    cache_movement_defaults()
    if movement == nil or baseMaxWalkSpeed == nil then return end

    movement:SetMaxWalkSpeed(baseMaxWalkSpeed * debugSpeedMultiplier)
    movement:SetSprintSpeedMultiplier(baseSprintSpeedMultiplier)
    movement:SetWallRunMaxSpeed(baseWallRunMaxSpeed * debugSpeedMultiplier)
end

local function set_debug_text(id, text)
    if debugWidget == nil then return end
    debugWidget:SetText(id, text)
end

local function set_debug_property(id, property, value)
    if debugWidget == nil then return end
    debugWidget:SetProperty(id, property, value)
end

local function update_debug_values()
    if debugWidget == nil then return end

    set_debug_text("debug-speed-value", format_number(debugSpeedMultiplier, 2) .. "x")
    set_debug_text("debug-invincible-toggle", isInvincible and "ON" or "OFF")
    set_debug_text("debug-damage-value", tostring(math.floor(debugDamageAmount + 0.5)))
    set_debug_text("debug-health-value", tostring(math.floor(currentHealth + 0.5)))
    set_debug_text("debug-health-label", "Health " .. tostring(math.floor(get_health_ratio() * 100.0 + 0.5)) .. "%")
    set_debug_property("debug-invincible-toggle", "color", isInvincible and "#111111" or "#d8d8d8")
    set_debug_property("debug-invincible-toggle", "background-color", isInvincible and "#ffffff" or "#ffffff1F")
end

local function update_debug_tab_visuals()
    if debugWidget == nil then return end

    local tabs = { "player", "world", "ai", "render", "audio" }
    for _, tab in ipairs(tabs) do
        local selected = tab == debugActiveTab
        set_debug_property("debug-tab-" .. tab, "color", selected and "#ffffff" or "#808080")
        set_debug_property("debug-panel-" .. tab, "display", selected and "block" or "none")
    end
end

local function set_debug_tab(tab)
    debugActiveTab = tab
    update_debug_tab_visuals()
end

local function set_debug_window_position(x, y)
    debugWindowX = clamp(x, 0.0, 1600.0)
    debugWindowY = clamp(y, 0.0, 900.0)
    set_debug_property("debug-window", "left", px(debugWindowX))
    set_debug_property("debug-window", "top", px(debugWindowY))
end

local function bind_debug_events()
    if debugWidget == nil then return end

    debugWidget:bind_click("debug-close", function()
        if debugWidget ~= nil and debugWidget:IsInViewport() then
            debugWidget:RemoveFromParent()
            debugWidget:SetWantsMouse(false)
        end
    end)

    debugWidget:bind_click("debug-tab-player", function() set_debug_tab("player") end)
    debugWidget:bind_click("debug-tab-world", function() set_debug_tab("world") end)
    debugWidget:bind_click("debug-tab-ai", function() set_debug_tab("ai") end)
    debugWidget:bind_click("debug-tab-render", function() set_debug_tab("render") end)
    debugWidget:bind_click("debug-tab-audio", function() set_debug_tab("audio") end)

    debugWidget:bind_click("debug-speed-minus", function()
        debugSpeedMultiplier = clamp(debugSpeedMultiplier - DEBUG_SPEED_STEP, 0.25, 8.0)
        apply_debug_speed_multiplier()
        update_debug_values()
    end)
    debugWidget:bind_click("debug-speed-plus", function()
        debugSpeedMultiplier = clamp(debugSpeedMultiplier + DEBUG_SPEED_STEP, 0.25, 8.0)
        apply_debug_speed_multiplier()
        update_debug_values()
    end)
    debugWidget:bind_click("debug-invincible-toggle", function()
        isInvincible = not isInvincible
        update_debug_values()
    end)
    debugWidget:bind_click("debug-damage-minus", function()
        debugDamageAmount = clamp(debugDamageAmount - DEBUG_DAMAGE_STEP, 0.0, 100.0)
        update_debug_values()
    end)
    debugWidget:bind_click("debug-damage-plus", function()
        debugDamageAmount = clamp(debugDamageAmount + DEBUG_DAMAGE_STEP, 0.0, 100.0)
        update_debug_values()
    end)
    debugWidget:bind_click("debug-apply-damage", function()
        apply_player_damage(debugDamageAmount)
        update_debug_values()
    end)
    debugWidget:bind_click("debug-health-minus", function()
        set_player_health(currentHealth - DEBUG_HEALTH_STEP)
        update_debug_values()
    end)
    debugWidget:bind_click("debug-health-plus", function()
        set_player_health(currentHealth + DEBUG_HEALTH_STEP)
        update_debug_values()
    end)

    debugWidget:bind_event("debug-window", "mouseover", function()
        debugMouseOver = true
    end)
    debugWidget:bind_event("debug-window", "mouseout", function()
        if not debugDragging then
            debugMouseOver = false
        end
    end)

    debugWidget:bind_event("debug-titlebar", "mousedown", function(event)
        debugDragging = true
        debugMouseOver = true
        debugDragOffsetX = event.mouse_x - debugWindowX
        debugDragOffsetY = event.mouse_y - debugWindowY
    end)
    debugWidget:bind_event("debug-titlebar", "mouseup", function()
        debugDragging = false
    end)
    debugWidget:bind_event("debug-window", "mouseup", function()
        debugDragging = false
    end)
    debugWidget:bind_event("debug-window", "mousemove", function(event)
        if not debugDragging then return end
        set_debug_window_position(event.mouse_x - debugDragOffsetX, event.mouse_y - debugDragOffsetY)
    end)
end

local function is_debug_input_capturing()
    if debugWidget == nil or not debugWidget:IsInViewport() then return false end
    return debugDragging or debugMouseOver
end

local function ensure_debug_widget()
    if debugWidget ~= nil then return end

    debugWidget = UI.CreateWidget("Content/UI/DebugUI/InGameDebug.rml")
    if debugWidget == nil then return end

    debugWidget:SetWantsMouse(false)
    bind_debug_events()
    update_debug_values()
    update_debug_tab_visuals()
    set_debug_window_position(debugWindowX, debugWindowY)
end

local function toggle_debug_widget()
    ensure_debug_widget()
    if debugWidget == nil then return end

    if debugWidget:IsInViewport() then
        debugWidget:RemoveFromParent()
        debugWidget:SetWantsMouse(false)
        debugDragging = false
        debugMouseOver = false
    else
        debugWidget:AddToViewportZ(DEBUG_Z_ORDER)
        debugWidget:SetWantsMouse(true)
        update_debug_values()
        update_debug_tab_visuals()
        set_debug_window_position(debugWindowX, debugWindowY)
    end
end

local function update_weapon_hud()
    if weaponHudWidget == nil then return end

    weaponHudWidget:SetText("weapon-hud-current-ammo", tostring(currentAmmo))
    weaponHudWidget:SetText("weapon-hud-magazine-ammo", tostring(MAGAZINE_SIZE))
end

local function is_target_actor(actor)
    return actor ~= nil and actor:GetSkeletalMesh() ~= nil
end

local function set_crosshair_color(isRed)
    if weaponHudWidget == nil then return end

    local whiteOpacity = isRed and "0.0" or "0.9"
    local redOpacity   = isRed and "0.9" or "0.0"

    weaponHudWidget:SetProperty("crosshair-left-white", "opacity", whiteOpacity)
    weaponHudWidget:SetProperty("crosshair-right-white", "opacity", whiteOpacity)
    weaponHudWidget:SetProperty("crosshair-top-white", "opacity", whiteOpacity)
    weaponHudWidget:SetProperty("crosshair-bottom-white", "opacity", whiteOpacity)
    weaponHudWidget:SetProperty("crosshair-dot-white", "opacity", whiteOpacity)

    weaponHudWidget:SetProperty("crosshair-left-red", "opacity", redOpacity)
    weaponHudWidget:SetProperty("crosshair-right-red", "opacity", redOpacity)
    weaponHudWidget:SetProperty("crosshair-top-red", "opacity", redOpacity)
    weaponHudWidget:SetProperty("crosshair-bottom-red", "opacity", redOpacity)
    weaponHudWidget:SetProperty("crosshair-dot-red", "opacity", redOpacity)
end

local function set_crosshair_part_position(idWhite, idRed, prop, value)
    if weaponHudWidget == nil then return end

    local valuePx = px(value)
    weaponHudWidget:SetProperty(idWhite, prop, valuePx)
    weaponHudWidget:SetProperty(idRed, prop, valuePx)
end

local function update_crosshair()
    if weaponHudWidget == nil then return end

    local spreadOffset = weaponSpread * CROSSHAIR_SPREAD_PX
    set_crosshair_part_position("crosshair-left-white", "crosshair-left-red", "left", CROSSHAIR_BASE_LEFT - spreadOffset)
    set_crosshair_part_position("crosshair-right-white", "crosshair-right-red", "left", CROSSHAIR_BASE_RIGHT + spreadOffset)
    set_crosshair_part_position("crosshair-top-white", "crosshair-top-red", "top", CROSSHAIR_BASE_TOP - spreadOffset)
    set_crosshair_part_position("crosshair-bottom-white", "crosshair-bottom-red", "top", CROSSHAIR_BASE_BOTTOM + spreadOffset)

    local isTargeted = false
    if camera ~= nil then
        local camPos = camera:GetWorldLocation()
        local camFwd = camera.Forward
        local hit = World.RaycastSkeletalMesh(camPos, camFwd, MAX_RANGE, obj)
        isTargeted = hit ~= nil and is_target_actor(hit.HitActor)
    end
    set_crosshair_color(isTargeted)

    local killOpacity = 0.0
    if killMarkerTimer > 0.0 then
        killOpacity = killMarkerTimer / KILL_MARKER_DURATION
        if killOpacity > 1.0 then killOpacity = 1.0 end
        killOpacity = killOpacity * 0.9
    end
    weaponHudWidget:SetProperty("crosshair-kill-marker", "opacity", string.format("%.2f", killOpacity))
end

local function trigger_kill_marker()
    killMarkerTimer = KILL_MARKER_DURATION
    update_crosshair()
end

local function finish_reload()
    currentAmmo = MAGAZINE_SIZE
    reloadTimer = 0.0
    isReloading = false
    update_weapon_hud()
end

local function start_reload()
    if arms == nil or isReloading or currentAmmo >= MAGAZINE_SIZE then return end

    isReloading = true
    reloadTimer = RELOAD_DURATION
    arms:PlayAnimationByPath(RELOAD_ANIM_PATH, false)
end

local function lerp_red(t)
    if t > 1.0 then t = 1.0 end
    -- 옅은 빨강 (255,180,180) → 진한 빨강 (180,0,0)
    local r = math.floor(255 - (255 - 180) * t)
    local g = math.floor(180 * (1 - t))
    local b = math.floor(180 * (1 - t))
    return r, g, b
end

local function register_hit(hitActor, hitLoc)
    local id = hitActor.UUID
    local n  = (hitCounts[id] or 0) + 1
    hitCounts[id] = n
    local r, g, b = lerp_red(n / MAX_HITS_FOR_FULL_RED)
    Debug.DrawSphere(hitLoc, 0.08, r, g, b, 9999.0, 10)

    if n == MAX_HITS_FOR_FULL_RED then
        trigger_kill_marker()
    end
end

local function get_fire_direction(camFwd)
    if Input.GetKey(Key.MouseRight) or weaponSpread <= 0.0 then
        return camFwd
    end

    local camRight = camera.Right
    local camUp = camera.Up
    local angle = math.random() * math.pi * 2.0
    local radius = math.sqrt(math.random()) * weaponSpread * MAX_SPREAD_ANGLE
    local spreadDir = camFwd + camRight * (math.cos(angle) * radius) + camUp * (math.sin(angle) * radius)
    spreadDir:Normalize()
    return spreadDir
end

local function try_shoot()
    if camera == nil or arms == nil then return end
    if isReloading then return end
    if currentAmmo <= 0 then
        start_reload()
        return
    end

    currentAmmo = currentAmmo - 1
    update_weapon_hud()

    arms:PlayAnimationByPath(SHOOT_ANIM_PATH, false)

    local camPos = camera:GetWorldLocation()
    local camFwd = camera.Forward
    local fireDir = get_fire_direction(camFwd)

    -- 카메라 + forward offset 을 머즐로 간주 (시안색 시각화).
    local muzzleWorld = Vector.new(
        camPos.X + camFwd.X * MUZZLE_FWD_OFFSET,
        camPos.Y + camFwd.Y * MUZZLE_FWD_OFFSET,
        camPos.Z + camFwd.Z * MUZZLE_FWD_OFFSET)
    Debug.DrawSphere(muzzleWorld, 0.04, 0, 200, 255, 1.0, 8)

    -- 카메라 → 머즐 사이에 벽이 있으면 (= 총구가 벽 안으로 박힘) 시작점 클램프.
    local rayStart = muzzleWorld
    local obstrHit = World.RaycastWorldStatic(camPos, fireDir, MUZZLE_FWD_OFFSET, obj)
    if obstrHit ~= nil then
        rayStart = obstrHit.WorldHitLocation
        Debug.DrawSphere(rayStart, 0.05, 255, 230, 0, 1.0, 8)
    end

    -- 벽 우선: 카메라 forward 방향으로 WorldStatic 트레이스 → 닿는 곳까지만 SkeletalMesh 트레이스.
    local effRange = MAX_RANGE
    local wallHit  = World.RaycastWorldStatic(rayStart, fireDir, MAX_RANGE, obj)
    if wallHit ~= nil then
        effRange = wallHit.Distance
    end

    local hit = World.RaycastSkeletalMesh(rayStart, fireDir, effRange, obj)

    local endPos
    if hit ~= nil then
        endPos = hit.WorldHitLocation
    elseif wallHit ~= nil then
        endPos = wallHit.WorldHitLocation
    else
        endPos = Vector.new(
            rayStart.X + fireDir.X * MAX_RANGE,
            rayStart.Y + fireDir.Y * MAX_RANGE,
            rayStart.Z + fireDir.Z * MAX_RANGE)
    end
    Debug.DrawLine(rayStart, endPos, 0, 255, 0, 1.5)

    if hit ~= nil and is_target_actor(hit.HitActor) then
        register_hit(hit.HitActor, hit.WorldHitLocation)
    end

    if not Input.GetKey(Key.MouseRight) then
        weaponSpread = weaponSpread + SPREAD_PER_SHOT
        if weaponSpread > MAX_SPREAD then weaponSpread = MAX_SPREAD end
        update_crosshair()
    end
end

function BeginPlay()
    arms   = obj:GetSkeletalMesh()
    camera = obj:GetCamera()
    movement = nil
    baseMaxWalkSpeed = nil
    baseSprintSpeedMultiplier = nil
    baseWallRunMaxSpeed = nil
    cache_movement_defaults()
    currentAmmo = MAGAZINE_SIZE
    currentHealth = MAX_HEALTH
    isInvincible = false
    debugSpeedMultiplier = 1.0
    debugDamageAmount = 10.0
    reloadTimer = 0.0
    isReloading = false
    fireCooldown = 0.0
    weaponSpread = 0.0
    killMarkerTimer = 0.0
    DamagePostProcess.Initialize()
    DamagePostProcess.Reset()
    DamagePostProcess.SetHealthRatio(get_health_ratio())

    _G.ApplyPlayerDamage = apply_player_damage
    _G.GetPlayerHealthRatio = get_health_ratio

    weaponHudWidget = UI.CreateWidget("Content/UI/HUD/WeaponHUD.rml")
    if weaponHudWidget ~= nil then
        weaponHudWidget:AddToViewportZ(80)
        update_weapon_hud()
        update_crosshair()
    end
end

function EndPlay()
    if weaponHudWidget ~= nil and weaponHudWidget:IsInViewport() then
        weaponHudWidget:RemoveFromParent()
    end
    weaponHudWidget = nil
    DamagePostProcess.Shutdown()
    if _G.ApplyPlayerDamage == apply_player_damage then
        _G.ApplyPlayerDamage = nil
    end
    if _G.GetPlayerHealthRatio == get_health_ratio then
        _G.GetPlayerHealthRatio = nil
    end
    if debugWidget ~= nil and debugWidget:IsInViewport() then
        debugWidget:RemoveFromParent()
    end
    debugWidget = nil
    if movement ~= nil and baseMaxWalkSpeed ~= nil then
        movement:SetMaxWalkSpeed(baseMaxWalkSpeed)
        movement:SetSprintSpeedMultiplier(baseSprintSpeedMultiplier)
        movement:SetWallRunMaxSpeed(baseWallRunMaxSpeed)
    end
    movement = nil
end

function Tick(dt)
    if arms == nil then return end

    if Input.GetKeyDown(Key.F10) then
        toggle_debug_widget()
    end

    if fireCooldown > 0 then
        fireCooldown = fireCooldown - dt
    end

    if weaponSpread > 0.0 then
        weaponSpread = weaponSpread - SPREAD_DECAY_PER_SEC * dt
        if weaponSpread < 0.0 then weaponSpread = 0.0 end
    end

    if Input.GetKey(Key.MouseRight) then
        weaponSpread = 0.0
    end

    if killMarkerTimer > 0.0 then
        killMarkerTimer = killMarkerTimer - dt
        if killMarkerTimer < 0.0 then killMarkerTimer = 0.0 end
    end

    if isReloading then
        reloadTimer = reloadTimer - dt
        if reloadTimer <= 0 then
            finish_reload()
        end
    end

    DamagePostProcess.Tick(dt)
    apply_debug_speed_multiplier()
    if debugWidget ~= nil and debugWidget:IsInViewport() then
        update_debug_values()
    end
    update_crosshair()

    if is_debug_input_capturing() then
        return
    end

    if Input.GetKeyDown(Key.R) then
        start_reload()
    end

    if Input.GetKey(Key.MouseLeft) and fireCooldown <= 0 then
        try_shoot()
        fireCooldown = FIRE_INTERVAL
    end

end
