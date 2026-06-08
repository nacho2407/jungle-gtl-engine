local arms   = nil
local camera = nil
local movement = nil
local DamagePostProcess = require("DamagePostProcess")
local GameOver = require("GameOver")
local InGamePause = require("InGamePause")
local WeaponHud = require("HUD/WeaponHud")
local CombatEvents = require("Game.CombatEvents")
local InGameDebug = require("DebugUI/InGameDebug")
local ItemInspectSystem = require("Items/ItemInspectSystem")
local GameAudio = require("Game.GameAudio")
local HitSpark = require("Game.HitSpark")
local ImpactAudio = require("Game.ImpactAudio")
local TutorialSystem = require("Tutorial.TutorialSystem")

-- 본 기반 머즐은 Arm_R 이 어깨 관절이라 카메라랑 거의 같은 위치 → 의미 없음.
-- 대부분 FPS 는 카메라에서 forward offset 으로 머즐을 근사함.
local MUZZLE_FWD_OFFSET = 0.4   -- 카메라 앞 0.4m 지점을 머즐로 간주

local MAX_RANGE        = 200.0
local FIRE_INTERVAL    = 0.12
local MAGAZINE_SIZE    = 30
local RELOAD_DURATION  = 1.1

local SPREAD_PER_SHOT       = 0.28
local SPREAD_DECAY_PER_SEC  = 1.15
local MAX_SPREAD            = 1.0
local MAX_SPREAD_ANGLE      = 0.075

-- Hard-coded recoil spray. vertical climb, left bias, then a rightward sweep. Values are camera kick in degrees per shot.
local RECOIL_ADS_MULTIPLIER          = 0.72
local RECOIL_STRENGTH_MULTIPLIER     = 5.0
local RECOIL_RECOVERY_DELAY          = 0.03
local RECOIL_RECOVERY_SPEED_FIRING   = 9.0
local RECOIL_RECOVERY_SPEED_RELEASED = 50.0
local RECOIL_PATTERN_RESET_DELAY     = 0.35
local RECOIL_PATTERN = {
    { pitch = -0.34, yaw = -0.02 },
    { pitch = -0.42, yaw =  0.04 },
    { pitch = -0.50, yaw =  0.09 },
    { pitch = -0.58, yaw =  0.05 },
    { pitch = -0.63, yaw = -0.04 },
    { pitch = -0.67, yaw = -0.14 },
    { pitch = -0.66, yaw = -0.24 },
    { pitch = -0.62, yaw = -0.32 },
    { pitch = -0.56, yaw = -0.38 },
    { pitch = -0.50, yaw = -0.34 },
    { pitch = -0.46, yaw = -0.22 },
    { pitch = -0.43, yaw = -0.06 },
    { pitch = -0.41, yaw =  0.12 },
    { pitch = -0.40, yaw =  0.29 },
    { pitch = -0.38, yaw =  0.44 },
    { pitch = -0.36, yaw =  0.54 },
    { pitch = -0.34, yaw =  0.48 },
    { pitch = -0.32, yaw =  0.36 },
    { pitch = -0.30, yaw =  0.22 },
    { pitch = -0.28, yaw =  0.08 },
    { pitch = -0.26, yaw = -0.02 },
    { pitch = -0.24, yaw = -0.08 },
    { pitch = -0.22, yaw = -0.06 },
    { pitch = -0.20, yaw =  0.03 },
}

local BULLET_DAMAGE = 12.5
local MAX_HEALTH = 100.0
local PLAYER_BULLET_IMPACT_AUDIO_EVENT = "player.bullet_impact"

local fireCooldown = 0.0
local reloadTimer  = 0.0
local currentAmmo  = MAGAZINE_SIZE
local isReloading  = false
local reloadAudioTrack = 1
local reloadAudioSteps = nil
local reloadAudioNextIndex = 1
local isDead       = false
local deathHandled = false
local weaponSpread = 0.0
local recoilPatternIndex = 1
local recoilRemainingPitch = 0.0
local recoilRemainingYaw = 0.0
local recoilRecoverDelay = 0.0
local recoilPatternResetTimer = 0.0
local currentHealth = MAX_HEALTH
local baseMaxWalkSpeed = nil
local baseSprintSpeedMultiplier = nil
local baseWallRunMaxSpeed = nil

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function add_weapon_recoil()
    if camera == nil then return end

    local step = RECOIL_PATTERN[recoilPatternIndex] or RECOIL_PATTERN[#RECOIL_PATTERN]
    local scale = RECOIL_STRENGTH_MULTIPLIER
    if Input.GetKey(Key.MouseRight) then
        scale = scale * RECOIL_ADS_MULTIPLIER
    end

    local deltaRotation = Vector.new(
        0.0,
        step.pitch * scale,
        step.yaw * scale)

    if type(camera.AddLocalRotation) == "function" then
        camera:AddLocalRotation(deltaRotation)
    elseif type(camera.GetRotation) == "function" and type(camera.SetRotation) == "function" then
        local rotation = camera:GetRotation()
        camera:SetRotation(Vector.new(
            rotation.X,
            rotation.Y + deltaRotation.Y,
            rotation.Z + deltaRotation.Z))
    end

    recoilRemainingPitch = recoilRemainingPitch + deltaRotation.Y
    recoilRemainingYaw = recoilRemainingYaw + deltaRotation.Z
    recoilRecoverDelay = RECOIL_RECOVERY_DELAY
    recoilPatternResetTimer = RECOIL_PATTERN_RESET_DELAY
    recoilPatternIndex = recoilPatternIndex + 1
end

local function recover_recoil_axis(value, maxStep)
    if value > maxStep then return value - maxStep, -maxStep end
    if value < -maxStep then return value + maxStep, maxStep end
    return 0.0, -value
end

local function update_weapon_recoil(dt)
    if camera == nil or dt == nil or dt <= 0.0 then return end

    if recoilPatternResetTimer > 0.0 then
        recoilPatternResetTimer = recoilPatternResetTimer - dt
        if recoilPatternResetTimer <= 0.0 then
            recoilPatternIndex = 1
        end
    end

    if recoilRecoverDelay > 0.0 then
        recoilRecoverDelay = recoilRecoverDelay - dt
        return
    end

    if math.abs(recoilRemainingPitch) <= 0.0001 and math.abs(recoilRemainingYaw) <= 0.0001 then
        recoilRemainingPitch = 0.0
        recoilRemainingYaw = 0.0
        return
    end

    local isFiring = Input.GetKey(Key.MouseLeft) and not isReloading and currentAmmo > 0
    local recoverySpeed = isFiring and RECOIL_RECOVERY_SPEED_FIRING or RECOIL_RECOVERY_SPEED_RELEASED
    local maxStep = recoverySpeed * dt
    local pitchDelta
    local yawDelta
    recoilRemainingPitch, pitchDelta = recover_recoil_axis(recoilRemainingPitch, maxStep)
    recoilRemainingYaw, yawDelta = recover_recoil_axis(recoilRemainingYaw, maxStep)

    local deltaRotation = Vector.new(0.0, pitchDelta, yawDelta)
    if type(camera.AddLocalRotation) == "function" then
        camera:AddLocalRotation(deltaRotation)
    elseif type(camera.GetRotation) == "function" and type(camera.SetRotation) == "function" then
        local rotation = camera:GetRotation()
        camera:SetRotation(Vector.new(
            rotation.X,
            rotation.Y + deltaRotation.Y,
            rotation.Z + deltaRotation.Z))
    end
end

local function get_health_ratio()
    if MAX_HEALTH <= 0.0 then return 0.0 end
    return clamp(currentHealth / MAX_HEALTH, 0.0, 1.0)
end

local function make_damage_result(applied, damageApplied, killed)
    return {
        bApplied = applied == true,
        bKilled = killed == true,
        bCritical = false,
        DamageApplied = damageApplied or 0.0,
        RemainingHealth = currentHealth,
        MaxHealth = MAX_HEALTH,
        HealthRatio = get_health_ratio(),
        Victim = obj,
    }
end

local function stop_player_action_audio()
    if GameAudio == nil then return end
    GameAudio.StopWeaponFireLoop()
    GameAudio.UpdateSlideState(false)
    GameAudio.UpdateWallRunState(false)
    GameAudio.StopFallingAudio()
end

local function handle_player_death()
    if deathHandled then return end
    deathHandled = true
    stop_player_action_audio()
    GameOver.Show()
end

local function play_player_impact_audio(context)
    if AudioManager == nil then
        return
    end

    local location = nil
    if type(context) == "table" then
        location = context.HitLocation or context.hitLocation
    end
    if location == nil then
        location = obj.Location
    end

    if AudioManager.PlayEventAt ~= nil then
        AudioManager.PlayEventAt(PLAYER_BULLET_IMPACT_AUDIO_EVENT, location.X or 0.0, location.Y or 0.0, location.Z or 0.0)
    elseif AudioManager.PlayOneShotAt ~= nil then
        AudioManager.PlayOneShotAt(PLAYER_BULLET_IMPACT_AUDIO_EVENT, location.X or 0.0, location.Y or 0.0, location.Z or 0.0)
    elseif AudioManager.PlayEvent ~= nil then
        AudioManager.PlayEvent(PLAYER_BULLET_IMPACT_AUDIO_EVENT)
    end
end

local function apply_player_damage(contextOrAmount)
    local amount = 0.0
    if type(contextOrAmount) == "table" then
        amount = contextOrAmount.Damage or contextOrAmount.damage or 0.0
    else
        amount = contextOrAmount or 0.0
    end

    if amount <= 0.0 or isDead then
        return make_damage_result(false, 0.0, false)
    end
    if InGameDebug.IsInvincible() then
        return make_damage_result(false, 0.0, false)
    end

    play_player_impact_audio(contextOrAmount)

    currentHealth = clamp(currentHealth - amount, 0.0, MAX_HEALTH)
    local killed = currentHealth <= 0.0
    if killed then
        isDead = true
        handle_player_death()
    end

    local ratio = get_health_ratio()
    DamagePostProcess.TriggerHit(ratio)
    DamagePostProcess.SetHealthRatio(ratio)

    return make_damage_result(true, amount, killed)
end

local function set_player_health(value)
    currentHealth = clamp(value, 0.0, MAX_HEALTH)
    isDead = currentHealth <= 0.0
    DamagePostProcess.SetHealthRatio(get_health_ratio())
    if isDead then
        handle_player_death()
    end
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

local function is_player_crouching()
    cache_movement_defaults()
    if movement ~= nil and type(movement.IsCrouching) == "function" then
        return movement:IsCrouching()
    end

    return Input.GetKey(Key.Ctrl) or Input.GetKey(Key.LeftCtrl) or Input.GetKey(Key.RightCtrl)
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
    return CombatEvents.IsDamageable(actor)
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
    request_arm_animation(is_player_crouching() and "reload_crouch" or "reload")
end

local function lerp_red(t)
    t = clamp(t or 0.0, 0.0, 1.0)
    -- 옅은 빨강 (255,180,180) → 진한 빨강 (180,0,0)
    local r = math.floor(255 - (255 - 180) * t)
    local g = math.floor(180 * (1 - t))
    local b = math.floor(180 * (1 - t))
    return r, g, b
end

local function on_attack_hit(context, result)
    local hitLoc = context ~= nil and context.HitLocation or nil
    if hitLoc ~= nil then
        local healthRatio = result ~= nil and result.HealthRatio or nil
        local damageRatio = 1.0 - clamp(healthRatio or 1.0, 0.0, 1.0)
        local r, g, b = lerp_red(damageRatio)
        Debug.DrawSphere(hitLoc, 0.08, r, g, b, 9999.0, 10)
    end

    WeaponHud.TriggerHitMarker()
end

local function on_attack_killed(context, result)
    WeaponHud.TriggerKillMarker()
    TutorialSystem.NotifyEnemyKilled()
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

    CombatEvents.NotifyAttackFired(obj, {
        Instigator = obj,
        DamageCauser = obj,
        ShotDirection = fireDir,
        Damage = BULLET_DAMAGE,
        DamageType = "Bullet",
    })

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

    -- Mesh 종류와 무관하게 가장 먼저 맞은 Primitive를 사용한다.
    -- DummyEnemy.lua가 CombatEvents damageable로 등록한 Actor면 StaticMesh/SkeletalMesh 모두 피격 처리된다.
    local hit = World.RaycastPrimitive(rayStart, fireDir, MAX_RANGE, obj)

    local endPos
    if hit ~= nil then
        endPos = hit.WorldHitLocation
    else
        endPos = Vector.new(
            rayStart.X + fireDir.X * MAX_RANGE,
            rayStart.Y + fireDir.Y * MAX_RANGE,
            rayStart.Z + fireDir.Z * MAX_RANGE)
    end
    Debug.DrawLine(rayStart, endPos, 0, 255, 0, 1.5)

    if hit ~= nil then
        local damageContext = CombatEvents.MakeDamageContext({
            Instigator = obj,
            DamageCauser = obj,
            HitActor = hit.HitActor,
            HitLocation = hit.WorldHitLocation,
            HitNormal = hit.WorldNormal or hit.ImpactNormal,
            ShotDirection = fireDir,
            Damage = BULLET_DAMAGE,
            DamageType = "Bullet",
        })
        CombatEvents.NotifyAttackImpact(obj, damageContext)

        if is_target_actor(hit.HitActor) then
            CombatEvents.ApplyDamageAndNotify(obj, hit.HitActor, damageContext)
        end
    end

    add_weapon_recoil()

    if not Input.GetKey(Key.MouseRight) then
        weaponSpread = weaponSpread + SPREAD_PER_SHOT
        if weaponSpread > MAX_SPREAD then weaponSpread = MAX_SPREAD end
        WeaponHud.SetSpread(weaponSpread)
    end
end

function BeginPlay()
    if ScoreManager ~= nil and ScoreManager.StartRun ~= nil then
        ScoreManager.StartRun()
    end

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
    isDead = false
    deathHandled = false
    reloadTimer = 0.0
    isReloading = false
    reset_reload_audio()
    fireCooldown = 0.0
    recoilPatternIndex = 1
    recoilRemainingPitch = 0.0
    recoilRemainingYaw = 0.0
    recoilRecoverDelay = 0.0
    recoilPatternResetTimer = 0.0
    local requests = get_anim_requests()
    if requests ~= nil then
        requests.shoot = false
        requests.reload = false
        requests.reload_crouch = false
    end
    weaponSpread = 0.0
    DamagePostProcess.Initialize()
    DamagePostProcess.Reset()
    DamagePostProcess.SetHealthRatio(get_health_ratio())
    GameOver.Initialize()
    InGamePause.Initialize()
    HitSpark.Initialize()
    ImpactAudio.Initialize()
    InGameDebug.Initialize({
        ZOrder = 220,
        GetHealth = function() return currentHealth end,
        SetHealth = set_player_health,
        GetHealthRatio = get_health_ratio,
        ApplyDamage = apply_player_damage,
        ApplySpeedMultiplier = apply_debug_speed_multiplier,
    })

    obj:AddTag("player")
    CombatEvents.RegisterDamageable(obj, {
        ApplyDamage = apply_player_damage,
        IsDead = function() return isDead end,
    })
    CombatEvents.RegisterAttackReceiver(obj, {
        OnAttackHit = on_attack_hit,
        OnAttackKilled = on_attack_killed,
    })

    _G.ApplyPlayerDamage = apply_player_damage
    _G.GetPlayerHealthRatio = get_health_ratio
    Engine.SetOnEscape(function()
        if GameOver.IsOpen() then return end
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

    CombatEvents.UnregisterAttackReceiver(obj)
    CombatEvents.UnregisterDamageable(obj)

    WeaponHud.Shutdown()
    ItemInspectSystem.Shutdown()
    GameOver.Shutdown()
    DamagePostProcess.Shutdown()
    InGamePause.Shutdown()
    HitSpark.Shutdown()
    ImpactAudio.Shutdown()
    InGameDebug.Shutdown()
    TutorialSystem.Shutdown()
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
    recoilPatternIndex = 1
    recoilRemainingPitch = 0.0
    recoilRemainingYaw = 0.0
    recoilRecoverDelay = 0.0
    recoilPatternResetTimer = 0.0
    deathHandled = false
    movement = nil
end

function Tick(dt)
    if arms == nil then return end

    if Input.GetKeyDown(Key.F10) then
        InGameDebug.Toggle()
    end

    update_weapon_recoil(dt)

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
    HitSpark.Tick(dt)
    TutorialSystem.Tick(dt)

    if GameOver.IsOpen() then
        stop_player_action_audio()
        return
    end

    if InGamePause.IsOpen() then
        stop_player_action_audio()
        return
    end

    ItemInspectSystem.Tick(camera, obj)
    if ItemInspectSystem.IsOpen() then
        stop_player_action_audio()
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
