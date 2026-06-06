local arms   = nil
local camera = nil
local movement = nil
local DamagePostProcess = require("DamagePostProcess")
local InGamePause = require("InGamePause")
local WeaponHud = require("HUD/WeaponHud")
local InGameDebug = require("DebugUI/InGameDebug")
local ItemInspectSystem = require("Items/ItemInspectSystem")
local GameAudio = require("Game.GameAudio")

-- 본 기반 머즐은 Arm_R 이 어깨 관절이라 카메라랑 거의 같은 위치 → 의미 없음.
-- 대부분 FPS 는 카메라에서 forward offset 으로 머즐을 근사함.
local MUZZLE_FWD_OFFSET = 0.4   -- 카메라 앞 0.4m 지점을 머즐로 간주

local MAX_RANGE        = 200.0
local FIRE_INTERVAL    = 0.12
local MAGAZINE_SIZE    = 50
local RELOAD_DURATION  = 1.1

local SPREAD_PER_SHOT       = 0.28
local SPREAD_DECAY_PER_SEC  = 1.15
local MAX_SPREAD            = 1.0
local MAX_SPREAD_ANGLE      = 0.075

local MAX_HITS_FOR_FULL_RED = 8
local MAX_HEALTH = 100.0

local fireCooldown = 0.0
local reloadTimer  = 0.0
local currentAmmo  = MAGAZINE_SIZE
local isReloading  = false
local reloadAudioTrack = 1
local reloadAudioSteps = nil
local reloadAudioNextIndex = 1
local hitCounts    = {}
local weaponSpread = 0.0
local currentHealth = MAX_HEALTH
local baseMaxWalkSpeed = nil
local baseSprintSpeedMultiplier = nil
local baseWallRunMaxSpeed = nil

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
    if InGameDebug.IsInvincible() then return end

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

local function apply_debug_speed_multiplier(multiplier)
    cache_movement_defaults()
    if movement == nil or baseMaxWalkSpeed == nil then return end

    multiplier = multiplier or 1.0
    movement:SetMaxWalkSpeed(baseMaxWalkSpeed * multiplier)
    movement:SetSprintSpeedMultiplier(baseSprintSpeedMultiplier)
    movement:SetWallRunMaxSpeed(baseWallRunMaxSpeed * multiplier)
end

local function get_anim_requests()
    if obj == nil then return nil end

    _G.FPSArmAnimRequests = _G.FPSArmAnimRequests or {}

    local ownerId = obj.UUID
    local requests = _G.FPSArmAnimRequests[ownerId]
    if requests == nil then
        requests = {}
        _G.FPSArmAnimRequests[ownerId] = requests
    end

    return requests
end

local function request_arm_animation(name)
    local requests = get_anim_requests()
    if requests == nil then return end

    requests[name] = true
end

local function update_weapon_hud()
    WeaponHud.SetAmmo(currentAmmo, MAGAZINE_SIZE)
end

local RELOAD_AUDIO_NORMAL = {
    { time = 0.000, event = "weapon.reload.mag_pull." },
    { time = 0.300, event = "weapon.reload.mag_grab." },
    { time = 0.583, event = "weapon.reload.mag_insert." },
    { time = 0.733, event = "weapon.reload.hand_rest." },
}

local RELOAD_AUDIO_EMPTY = {
    { time = 0.150, event = "weapon.reload.empty_mag_pull." },
    { time = 0.350, event = "weapon.reload.empty_mag_grab." },
    { time = 0.633, event = "weapon.reload.empty_mag_insert." },
    { time = 0.850, event = "weapon.reload.bolt_back." },
    { time = 0.917, event = "weapon.reload.bolt_forward." },
    { time = 1.067, event = "weapon.reload.empty_hand_rest." },
}

local function reset_reload_audio()
    reloadAudioTrack = 1
    reloadAudioSteps = nil
    reloadAudioNextIndex = 1
end

local function play_reload_audio_until(elapsed)
    if reloadAudioSteps == nil then return end

    while reloadAudioNextIndex <= #reloadAudioSteps do
        local step = reloadAudioSteps[reloadAudioNextIndex]
        if elapsed < step.time then return end

        GameAudio.PlayEvent(step.event .. tostring(reloadAudioTrack))
        reloadAudioNextIndex = reloadAudioNextIndex + 1
    end
end

local function start_reload_audio(emptyReload)
    reloadAudioTrack = math.random(1, 2)
    reloadAudioSteps = emptyReload and RELOAD_AUDIO_EMPTY or RELOAD_AUDIO_NORMAL
    reloadAudioNextIndex = 1
    play_reload_audio_until(0.0)
end

local function is_target_actor(actor)
    return actor ~= nil and actor:GetSkeletalMesh() ~= nil
end

local function finish_reload()
    currentAmmo = MAGAZINE_SIZE
    reloadTimer = 0.0
    isReloading = false
    reset_reload_audio()
    update_weapon_hud()
end

local function start_reload()
    if arms == nil or isReloading or currentAmmo >= MAGAZINE_SIZE then return end

    GameAudio.StopWeaponFireLoop()
    local emptyReload = currentAmmo <= 0
    isReloading = true
    reloadTimer = RELOAD_DURATION
    start_reload_audio(emptyReload)
    request_arm_animation("reload")
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
        WeaponHud.TriggerKillMarker()
    elseif n < MAX_HITS_FOR_FULL_RED then
        WeaponHud.TriggerHitMarker()
    else
        return
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
    if isReloading then
        GameAudio.StopWeaponFireLoop()
        return
    end
    if currentAmmo <= 0 then
        GameAudio.StopWeaponFireLoop()
        start_reload()
        return
    end

    currentAmmo = currentAmmo - 1
    update_weapon_hud()

    request_arm_animation("shoot")
    GameAudio.NotifyShotFired(Input.GetKey(Key.MouseRight))

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
        WeaponHud.SetSpread(weaponSpread)
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
    GameAudio.Initialize()
    GameAudio.PlayEnvironmentWind(0.35)
    currentAmmo = MAGAZINE_SIZE
    currentHealth = MAX_HEALTH
    reloadTimer = 0.0
    isReloading = false
    reset_reload_audio()
    fireCooldown = 0.0
    local requests = get_anim_requests()
    if requests ~= nil then
        requests.shoot = false
        requests.reload = false
    end
    weaponSpread = 0.0
    DamagePostProcess.Initialize()
    DamagePostProcess.Reset()
    DamagePostProcess.SetHealthRatio(get_health_ratio())
    InGamePause.Initialize()
    InGameDebug.Initialize({
        ZOrder = 220,
        GetHealth = function() return currentHealth end,
        SetHealth = set_player_health,
        GetHealthRatio = get_health_ratio,
        ApplyDamage = apply_player_damage,
        ApplySpeedMultiplier = apply_debug_speed_multiplier,
    })

    _G.ApplyPlayerDamage = apply_player_damage
    _G.GetPlayerHealthRatio = get_health_ratio
    Engine.SetOnEscape(function()
        InGamePause.Toggle()
    end)

    WeaponHud.Initialize({
        camera = camera,
        owner = obj,
        maxRange = MAX_RANGE,
        currentAmmo = currentAmmo,
        magazineSize = MAGAZINE_SIZE,
        weaponSpread = weaponSpread,
        zOrder = 80,
    })
    ItemInspectSystem.Initialize()
end

function EndPlay()
    if obj ~= nil and _G.FPSArmAnimRequests ~= nil then
        _G.FPSArmAnimRequests[obj.UUID] = nil
    end

    WeaponHud.Shutdown()
    ItemInspectSystem.Shutdown()
    DamagePostProcess.Shutdown()
    InGamePause.Shutdown()
    InGameDebug.Shutdown()
    Engine.SetOnEscape(function() end)
    if _G.ApplyPlayerDamage == apply_player_damage then
        _G.ApplyPlayerDamage = nil
    end
    if _G.GetPlayerHealthRatio == get_health_ratio then
        _G.GetPlayerHealthRatio = nil
    end
    if movement ~= nil and baseMaxWalkSpeed ~= nil then
        movement:SetMaxWalkSpeed(baseMaxWalkSpeed)
        movement:SetSprintSpeedMultiplier(baseSprintSpeedMultiplier)
        movement:SetWallRunMaxSpeed(baseWallRunMaxSpeed)
    end
    GameAudio.Shutdown()
    movement = nil
end

function Tick(dt)
    if arms == nil then return end

    if Input.GetKeyDown(Key.F10) then
        InGameDebug.Toggle()
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
    GameAudio.UpdateWeaponFireState(
        Input.GetKey(Key.MouseRight),
        Input.GetKey(Key.MouseLeft) and not isReloading and currentAmmo > 0)

    if isReloading then
        reloadTimer = reloadTimer - dt
        play_reload_audio_until(RELOAD_DURATION - reloadTimer)
        if reloadTimer <= 0 then
            finish_reload()
        end
    end

    DamagePostProcess.Tick(dt)
    InGamePause.Tick()
    WeaponHud.Tick(dt, weaponSpread)
    InGameDebug.Tick()

    if InGamePause.IsOpen() then
        GameAudio.StopWeaponFireLoop()
        GameAudio.UpdateSlideState(false)
        GameAudio.UpdateWallRunState(false)
        GameAudio.StopFallingAudio()
        return
    end

    ItemInspectSystem.Tick(camera, obj)
    if ItemInspectSystem.IsOpen() then
        GameAudio.StopWeaponFireLoop()
        GameAudio.UpdateSlideState(false)
        GameAudio.UpdateWallRunState(false)
        GameAudio.StopFallingAudio()
        return
    end

    GameAudio.UpdateMovement(movement, dt)

    if Input.GetKeyDown(Key.R) then
        start_reload()
    end

    if Input.GetKey(Key.MouseLeft) and fireCooldown <= 0 then
        try_shoot()
        fireCooldown = FIRE_INTERVAL
        GameAudio.UpdateWeaponFireState(
            Input.GetKey(Key.MouseRight),
            Input.GetKey(Key.MouseLeft) and not isReloading and currentAmmo > 0)
    end

end
