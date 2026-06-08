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
local RECOIL_ADS_MULTIPLIER          = 0.5
local RECOIL_ADS_YAW_MULTIPLIER      = 0.0
local RECOIL_ADS_CLEAR_YAW_ON_ENTER  = true
local ADS_SUPPRESS_SHOOT_ANIMATION   = true
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

local BULLET_DAMAGE = 15
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

-- 슬라이드 전용 애니가 없어서 SkeletalMesh 의 relative transform 으로 총구를 옆으로 기울임.
-- 값은 카메라 기준 (X=Roll/Y=Pitch/Z=Yaw, 위치는 Right/Up 추정) 이며 게임에서 보고 튜닝.
local SLIDE_TILT_ROLL_DEG    =  40.0  -- 양수: 총을 오른쪽으로 굴림
local SLIDE_TILT_PITCH_DEG   = -12.0   -- 약간 아래로 숙임
local SLIDE_LOC_OFFSET_RIGHT =  0.04  -- 오른쪽으로 살짝
local SLIDE_LOC_OFFSET_DOWN  = -0.50  -- 아래로 살짝
local SLIDE_BLEND_HZ         =  14.0  -- 클수록 빨리 자세 잡힘

-- ADS (Aim-Down-Sights). 한 점만 맞추면 총이 옆에 남는다.
-- 뒤쪽 sight 를 카메라 앞에 두고, muzzle 을 화면 중앙 광축에 맞춰서 "총구-조준경" 라인을 만든다.
local ADS_REAR_SIGHT_BONE   = "def_c_sightRear"
local ADS_FRONT_SIGHT_BONE  = "def_c_sightFront"
local ADS_MUZZLE_BONE       = "muzzle_flash"
local ADS_VIEW_FALLBACK_BONE = "jx_c_pov"
local ADS_BLEND_HZ          = 18.0      -- 클수록 빨리 조준 자세 잡힘
local ADS_DOF_FOCUS_DISTANCE = 1.35     -- 머즐/가늠쇠 근처에 초점
local ADS_DOF_FSTOP         = 1.6       -- 작을수록 주변 흐림 강함
local ADS_FOV_SCALE         = 0.76      -- 조준 시 FOV 줄여 줌
local ADS_WEAPON_SCALE      = 1.0      -- 조준 시 총 반경 감소

-- 카메라 로컬 좌표계 기준 목표점.
-- rear sight 는 카메라 바로 앞, muzzle 은 더 앞의 화면 중앙에 둔다.
local ADS_REAR_TARGET_FWD   = -0.1
local ADS_REAR_TARGET_RIGHT = 0.0
local ADS_REAR_TARGET_UP    = -0.5
local ADS_MUZZLE_TARGET_FWD = 2.20
local ADS_MUZZLE_TARGET_RIGHT = 0.0
local ADS_MUZZLE_TARGET_UP  = 0.0

-- World-space rear placement gets us close; viewport-space correction finishes the center alignment.
local ADS_MUZZLE_CENTER_WEIGHT = 0.0
local ADS_SCREEN_CENTER_WEIGHT = 0.85
local ADS_SCREEN_MAX_RIGHT     = 0.45
local ADS_SCREEN_MAX_UP        = 0.20
local ADS_SCREEN_CORRECTION_PASSES = 2
local ADS_MAX_ALIGN_ROT_DEG = 5.0

-- ADS 동안 팔에 추가로 적용할 로컬 회전(도). SlideTilt 와 같은 X=Roll/Y=Pitch/Z=Yaw 규약.
local ADS_TILT_ROLL_DEG     =  8.0
local ADS_TILT_PITCH_DEG    =  0.0
local ADS_TILT_YAW_DEG      =  0.0
local ADS_LEVEL_ROLL_DEG    =  0.0

-- ADS 블렌드가 이 임계값을 넘으면 화면 중앙 HUD 크로스헤어(< ^ > v 화살표) 와
-- 총 메시의 v_cro (def_c_cro 본에 매달린 인게임 가늠점 메시) 를 숨긴다.
-- 본 스케일을 0 으로 만들어 def_c_cro 와 그 자식 메시 섹션이 화면에서 사라지게 한다.
local ADS_HIDE_CROSSHAIR_AT = 0.35
local ADS_HIDE_VCRO_BONE    = "def_c_cro"
local ADS_HIDE_HUD_CROSSHAIR = false
-- SetBoneScale 은 내부 BoneEditPose 를 켜므로, ADS 해제 때 ResetBoneEditPose 로 반드시 정리한다.
local ADS_HIDE_WEAPON_CRO_BONE = true
local ADS_DEBUG_LOG = true

local crosshairHidden = false
local vcroHidden       = false
local lastAimDown      = false
local lastAdsSettledState = "hip"
local adsDebugTimer    = 0.0

local armsBaseLoc   = nil
local armsBaseRot   = nil
local armsBaseScale = nil
local slideBlend    = 0.0
local adsBlend      = 0.0
local dofBaseEnabled       = nil
local dofBaseFocusDistance = nil
local dofBaseFStop         = nil
local fovBase              = nil

local function vec_copy(v)
    if v == nil then return nil end
    return Vector.new(v.X, v.Y, v.Z)
end

local function vec_add(a, b)
    return Vector.new(a.X + b.X, a.Y + b.Y, a.Z + b.Z)
end

local function vec_sub(a, b)
    return Vector.new(a.X - b.X, a.Y - b.Y, a.Z - b.Z)
end

local function vec_mul(v, s)
    return Vector.new(v.X * s, v.Y * s, v.Z * s)
end

local function vec_dot(a, b)
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z
end

local function clamp_abs(value, maxAbs)
    if value < -maxAbs then return -maxAbs end
    if value > maxAbs then return maxAbs end
    return value
end

local function vec_text(v)
    if v == nil then return "nil" end
    return string.format("(%.3f, %.3f, %.3f)", v.X or 0.0, v.Y or 0.0, v.Z or 0.0)
end

local function debug_ads_state(label)
    if not ADS_DEBUG_LOG then return end

    local aimNow = false
    if Key ~= nil and Input ~= nil then
        if Key.MouseRight ~= nil then aimNow = aimNow or Input.GetKey(Key.MouseRight) == true end
        if Key.GamepadLeftTrigger ~= nil then aimNow = aimNow or Input.GetKey(Key.GamepadLeftTrigger) == true end
    end

    local armsRel = (arms ~= nil) and arms.RelativeLocation or nil
    local armsRot = (arms ~= nil and type(arms.GetRotation) == "function") and arms:GetRotation() or nil
    local armsScale = (arms ~= nil and type(arms.GetRelativeScale) == "function") and arms:GetRelativeScale() or nil
    local camLoc = (camera ~= nil and type(camera.GetWorldLocation) == "function") and camera:GetWorldLocation() or nil
    local camRot = (camera ~= nil and type(camera.GetRotation) == "function") and camera:GetRotation() or nil
    local fov = (camera ~= nil and type(camera.GetFOV) == "function") and camera:GetFOV() or -1.0

    print(string.format(
        "ADS %s aim=%s blend=%.3f armsRel=%s baseRel=%s armsRot=%s baseRot=%s armsScale=%s baseScale=%s camLoc=%s camRot=%s fov=%.3f baseFov=%.3f",
        label,
        tostring(aimNow),
        adsBlend or 0.0,
        vec_text(armsRel),
        vec_text(armsBaseLoc),
        vec_text(armsRot),
        vec_text(armsBaseRot),
        vec_text(armsScale),
        vec_text(armsBaseScale),
        vec_text(camLoc),
        vec_text(camRot),
        fov,
        fovBase or -1.0))
end

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function is_input_down(key)
    return key ~= nil and Input.GetKey(key)
end

local function is_input_pressed(key)
    return key ~= nil and Input.GetKeyDown(key)
end

local function is_fire_down()
    return is_input_down(Key.MouseLeft) or is_input_down(Key.GamepadRightTrigger)
end

local function is_aim_down()
    return is_input_down(Key.MouseRight) or is_input_down(Key.GamepadLeftTrigger)
end

local function is_reload_pressed()
    return is_input_pressed(Key.R) or is_input_pressed(Key.GamepadX)
end

local function is_crouch_input_down()
    return is_input_down(Key.Ctrl)
        or is_input_down(Key.LeftCtrl)
        or is_input_down(Key.RightCtrl)
        or is_input_down(Key.GamepadB)
end

local function add_weapon_recoil()
    if camera == nil then return end

    local step = RECOIL_PATTERN[recoilPatternIndex] or RECOIL_PATTERN[#RECOIL_PATTERN]
    local aimNow = is_aim_down()
    local pitchScale = RECOIL_STRENGTH_MULTIPLIER
    local yawScale = RECOIL_STRENGTH_MULTIPLIER
    if aimNow then
        pitchScale = pitchScale * RECOIL_ADS_MULTIPLIER
        yawScale = yawScale * RECOIL_ADS_YAW_MULTIPLIER
    end

    local deltaRotation = Vector.new(
        0.0,
        step.pitch * pitchScale,
        step.yaw * yawScale)

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

    local isFiring = is_fire_down() and not isReloading and currentAmmo > 0
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

local function cache_arms_base_transform()
    if arms == nil or armsBaseLoc ~= nil then return end
    armsBaseLoc = vec_copy(arms.RelativeLocation)
    armsBaseRot = vec_copy(arms:GetRotation())
    if type(arms.GetRelativeScale) == "function" then
        armsBaseScale = vec_copy(arms:GetRelativeScale())
    else
        armsBaseScale = Vector.new(1.0, 1.0, 1.0)
    end
    debug_ads_state("cache-base")
end

local function reset_weapon_bone_edit_pose(reason)
    if arms == nil or type(arms.ResetBoneEditPose) ~= "function" then return end
    arms:ResetBoneEditPose()
    vcroHidden = false
    debug_ads_state("reset-bone-edit-" .. (reason or "unknown"))
end

local function damp_toward(current, target, hz, dt)
    if dt == nil or dt <= 0.0 then return current end
    local k = 1.0 - math.exp(-hz * dt)
    return current + (target - current) * k
end

local function get_arm_anim_flags()
    if obj == nil then return nil end
    return _G.FPSArmAnimFlags and _G.FPSArmAnimFlags[obj.UUID]
end

local function get_ads_bone_world_location(boneName)
    if boneName == nil or boneName == "" then return nil end
    local boneWorld = arms:GetBoneSocketLocation(boneName, Vector.new(0.0, 0.0, 0.0))
    if boneWorld == nil then return nil end

    -- GetBoneSocketLocation 은 본을 못 찾으면 ZeroVector 를 돌려준다. 카메라에서 너무 먼 값은 실패로 본다.
    local camPos = camera:GetWorldLocation()
    local camDX = boneWorld.X - camPos.X
    local camDY = boneWorld.Y - camPos.Y
    local camDZ = boneWorld.Z - camPos.Z
    if camDX*camDX + camDY*camDY + camDZ*camDZ > 10000.0 then return nil end

    return boneWorld
end

local function make_camera_space_point(offsetFwd, offsetRight, offsetUp)
    local camPos = camera:GetWorldLocation()
    local fwd   = camera.Forward
    local right = camera.Right
    local up    = camera.Up

    return Vector.new(
        camPos.X + fwd.X * offsetFwd + right.X * offsetRight + up.X * offsetUp,
        camPos.Y + fwd.Y * offsetFwd + right.Y * offsetRight + up.Y * offsetUp,
        camPos.Z + fwd.Z * offsetFwd + right.Z * offsetRight + up.Z * offsetUp)
end

local function get_ads_rear_world()
    return get_ads_bone_world_location(ADS_REAR_SIGHT_BONE)
        or get_ads_bone_world_location(ADS_VIEW_FALLBACK_BONE)
        or get_ads_bone_world_location(ADS_FRONT_SIGHT_BONE)
end

local function get_ads_muzzle_world()
    return get_ads_bone_world_location(ADS_MUZZLE_BONE)
        or get_ads_bone_world_location(ADS_FRONT_SIGHT_BONE)
end

local function project_ads_world(world)
    if camera == nil or world == nil then return nil end
    if type(camera.ProjectWorldToScreen) ~= "function" then return nil end
    return camera:ProjectWorldToScreen(world)
end

local function screen_text(name, world)
    local screen = project_ads_world(world)
    if screen == nil then
        return name .. "=no-project"
    end

    return string.format(
        "%s=(valid=%s x=%.1f y=%.1f ndc=(%.2f,%.2f,%.2f) depth=%.2f)",
        name,
        tostring(screen.Valid == true),
        screen.X or 0.0,
        screen.Y or 0.0,
        screen.NdcX or 0.0,
        screen.NdcY or 0.0,
        screen.NdcZ or 0.0,
        screen.Depth or 0.0)
end

local function debug_ads_projection(label)
    if not ADS_DEBUG_LOG or camera == nil then return end

    local rearWorld = get_ads_rear_world()
    local frontWorld = get_ads_bone_world_location(ADS_FRONT_SIGHT_BONE)
    local muzzleWorld = get_ads_muzzle_world()
    local muzzleScreen = project_ads_world(muzzleWorld)

    local viewportWidth = 0.0
    local viewportHeight = 0.0
    if muzzleScreen ~= nil then
        viewportWidth = muzzleScreen.ViewportWidth or 0.0
        viewportHeight = muzzleScreen.ViewportHeight or 0.0
    elseif Engine ~= nil and type(Engine.GetViewportSize) == "function" then
        local viewport = Engine.GetViewportSize()
        viewportWidth = viewport.Width or 0.0
        viewportHeight = viewport.Height or 0.0
    end

    local errX = 0.0
    local errY = 0.0
    if muzzleScreen ~= nil then
        errX = viewportWidth * 0.5 - (muzzleScreen.X or 0.0)
        errY = viewportHeight * 0.5 - (muzzleScreen.Y or 0.0)
    end

    print(string.format(
        "ADS-PROJ %s viewport=(%.0f, %.0f) muzzleErrPx=(%.1f, %.1f) %s %s %s",
        label,
        viewportWidth,
        viewportHeight,
        errX,
        errY,
        screen_text("rear", rearWorld),
        screen_text("front", frontWorld),
        screen_text("muzzle", muzzleWorld)))
end

local function compute_ads_rotation_correction()
    if arms == nil or camera == nil then return nil end
    if type(arms.GetBoneSocketLocation) ~= "function" then return nil end

    local rearWorld = get_ads_rear_world()
    local muzzleWorld = get_ads_muzzle_world()
    if rearWorld == nil or muzzleWorld == nil then return nil end

    local sightLine = vec_sub(muzzleWorld, rearWorld)
    local lineFwd = vec_dot(sightLine, camera.Forward)
    if math.abs(lineFwd) < 0.001 then return nil end

    local lineRight = vec_dot(sightLine, camera.Right)
    local lineUp = vec_dot(sightLine, camera.Up)
    local yawDeg = math.deg(math.atan(lineRight / lineFwd))
    local pitchDeg = -math.deg(math.atan(lineUp / lineFwd))

    return Vector.new(
        0.0,
        clamp_abs(pitchDeg, ADS_MAX_ALIGN_ROT_DEG),
        clamp_abs(yawDeg, ADS_MAX_ALIGN_ROT_DEG))
end

-- ADS 위치 보정. rear sight 를 카메라 앞쪽으로 가져오고, muzzle 은 화면 중앙 right/up 오차만 추가로 줄인다.
local function compute_ads_screen_delta()
    if camera == nil then return nil end

    local muzzleWorld = get_ads_muzzle_world()
    local screen = project_ads_world(muzzleWorld)
    if screen == nil or screen.Valid ~= true then return nil end

    local viewportWidth = screen.ViewportWidth or 0.0
    local viewportHeight = screen.ViewportHeight or 0.0
    if viewportWidth <= 0.0 or viewportHeight <= 0.0 then return nil end

    local fov = (type(camera.GetFOV) == "function") and camera:GetFOV() or 1.0
    local depth = screen.Depth or ADS_MUZZLE_TARGET_FWD
    if depth < 0.25 then depth = 0.25 end

    local errX = viewportWidth * 0.5 - (screen.X or 0.0)
    local errY = viewportHeight * 0.5 - (screen.Y or 0.0)
    local unitsPerPixelY = 2.0 * math.tan(fov * 0.5) * depth / viewportHeight
    local unitsPerPixelX = unitsPerPixelY * (viewportWidth / viewportHeight)

    return Vector.new(
        0.0,
        clamp_abs(errX * unitsPerPixelX * ADS_SCREEN_CENTER_WEIGHT, ADS_SCREEN_MAX_RIGHT),
        clamp_abs(-errY * unitsPerPixelY * ADS_SCREEN_CENTER_WEIGHT, ADS_SCREEN_MAX_UP))
end

local function compute_ads_world_delta()
    if arms == nil or camera == nil then return nil end
    if type(arms.GetBoneSocketLocation) ~= "function" then return nil end

    local rearWorld = get_ads_rear_world()
    if rearWorld == nil then return nil end

    local rearTarget = make_camera_space_point(
        ADS_REAR_TARGET_FWD,
        ADS_REAR_TARGET_RIGHT,
        ADS_REAR_TARGET_UP)
    local delta = vec_sub(rearTarget, rearWorld)

    local muzzleWorld = get_ads_muzzle_world()
    if muzzleWorld ~= nil then
        local muzzleTarget = make_camera_space_point(
            ADS_MUZZLE_TARGET_FWD,
            ADS_MUZZLE_TARGET_RIGHT,
            ADS_MUZZLE_TARGET_UP)
        local muzzleDelta = vec_sub(muzzleTarget, muzzleWorld)
        local rightFix = vec_mul(camera.Right, vec_dot(muzzleDelta, camera.Right) * ADS_MUZZLE_CENTER_WEIGHT)
        local upFix = vec_mul(camera.Up, vec_dot(muzzleDelta, camera.Up) * ADS_MUZZLE_CENTER_WEIGHT)
        delta = vec_add(delta, vec_add(rightFix, upFix))
    end

    return delta
end

local function update_ads_post_fx(blend)
    if camera == nil then return end

    local shouldHideCrosshair = ADS_HIDE_HUD_CROSSHAIR and blend >= ADS_HIDE_CROSSHAIR_AT
    if shouldHideCrosshair ~= crosshairHidden then
        crosshairHidden = shouldHideCrosshair
        if type(WeaponHud.SetCrosshairVisible) == "function" then
            WeaponHud.SetCrosshairVisible(not shouldHideCrosshair)
        end
    end

    if ADS_HIDE_WEAPON_CRO_BONE and arms ~= nil and type(arms.SetBoneScale) == "function" then
        local shouldHideVCro = blend >= ADS_HIDE_CROSSHAIR_AT
        if shouldHideVCro ~= vcroHidden then
            vcroHidden = shouldHideVCro
            local scale = shouldHideVCro and Vector.new(0.0, 0.0, 0.0) or Vector.new(1.0, 1.0, 1.0)
            arms:SetBoneScale(ADS_HIDE_VCRO_BONE, scale)
            if not shouldHideVCro then
                reset_weapon_bone_edit_pose("vcro-show")
            end
        end
    end

    if fovBase ~= nil and type(camera.SetFOV) == "function" then
        local scale = 1.0 + (ADS_FOV_SCALE - 1.0) * blend
        camera:SetFOV(fovBase * scale)
    end

    if type(camera.SetDOFEnabled) ~= "function" then return end
    if dofBaseEnabled == nil then return end

    if blend <= 0.0001 then
        camera:SetDOFEnabled(dofBaseEnabled)
        if dofBaseFocusDistance ~= nil then camera:SetDOFFocusDistance(dofBaseFocusDistance) end
        if dofBaseFStop ~= nil then camera:SetDOFFStop(dofBaseFStop) end
        return
    end

    camera:SetDOFEnabled(true)
    local focus = (dofBaseFocusDistance or 10.0) + (ADS_DOF_FOCUS_DISTANCE - (dofBaseFocusDistance or 10.0)) * blend
    local fstop = (dofBaseFStop or 4.0) + (ADS_DOF_FSTOP - (dofBaseFStop or 4.0)) * blend
    camera:SetDOFFocusDistance(focus)
    camera:SetDOFFStop(fstop)
end

local function update_arm_transforms(dt)
    if arms == nil then return end

    cache_arms_base_transform()
    if armsBaseLoc == nil or armsBaseRot == nil then return end

    local flags = get_arm_anim_flags()
    -- movement:IsSliding() 는 lua 에 노출 안 돼 있어 AnimInstance 가 매 프레임 채워주는 플래그를 읽는다.
    local slideTarget = (flags ~= nil and flags.needsSlideTilt == true and flags.isSliding == true) and 1.0 or 0.0
    slideBlend = damp_toward(slideBlend, slideTarget, SLIDE_BLEND_HZ, dt)

    -- 슬라이딩 중에는 조준 자세를 잡지 않는다.
    local aimNow = is_aim_down()
    if aimNow ~= lastAimDown then
        lastAimDown = aimNow
        lastAdsSettledState = "moving"
        adsDebugTimer = 0.0
        if aimNow and RECOIL_ADS_CLEAR_YAW_ON_ENTER then
            recoilRemainingYaw = 0.0
        end
        debug_ads_state(aimNow and "aim-down" or "aim-up")
        if not aimNow then
            reset_weapon_bone_edit_pose("aim-up")
        end
    end

    local adsTarget = (aimNow and slideTarget <= 0.0) and 1.0 or 0.0
    adsBlend = damp_toward(adsBlend, adsTarget, ADS_BLEND_HZ, dt)
    -- damp_toward 는 지수적 접근이라 정확히 0/1 에 안 닿는다. 작은 잔존값이 다음 프레임의
    -- SetLocation 으로 누적될 수 있어 임계값 아래는 스냅해서 베이스로 깔끔히 돌아오게.
    if adsTarget <= 0.0 and adsBlend < 0.001 then adsBlend = 0.0 end
    if adsTarget >= 1.0 and adsBlend > 0.999 then adsBlend = 1.0 end

    local locX = armsBaseLoc.X
    local locY = armsBaseLoc.Y + SLIDE_LOC_OFFSET_RIGHT * slideBlend
    local locZ = armsBaseLoc.Z + SLIDE_LOC_OFFSET_DOWN  * slideBlend
    local scaleBlend = 1.0 + (ADS_WEAPON_SCALE - 1.0) * adsBlend
    local scaleX = armsBaseScale.X * scaleBlend
    local scaleY = armsBaseScale.Y * scaleBlend
    local scaleZ = armsBaseScale.Z * scaleBlend
    local adsRollOffset = (ADS_LEVEL_ROLL_DEG - armsBaseRot.X) + ADS_TILT_ROLL_DEG
    local rotX = armsBaseRot.X + SLIDE_TILT_ROLL_DEG  * slideBlend + adsRollOffset * adsBlend
    local rotY = armsBaseRot.Y + SLIDE_TILT_PITCH_DEG * slideBlend + ADS_TILT_PITCH_DEG * adsBlend
    local rotZ = armsBaseRot.Z + ADS_TILT_YAW_DEG * adsBlend
    local finalLocX = locX
    local finalLocY = locY
    local finalLocZ = locZ

    -- 1) 슬라이드/베이스 relative transform 을 먼저 반영한다.
    --    이 상태가 매 프레임의 깨끗한 출발점이다.
    arms:SetRelativeLocation(Vector.new(locX, locY, locZ))
    if type(arms.SetRelativeScale) == "function" then
        arms:SetRelativeScale(Vector.new(scaleX, scaleY, scaleZ))
    end
    arms:SetRotation(Vector.new(rotX, rotY, rotZ))

    -- 2) rear sight -> muzzle 라인이 카메라 forward 와 일치하도록 회전을 보정한다.
    --    위치 보정보다 먼저 해야 muzzle 중앙 정렬이 덜 밀린다.
    if adsBlend > 0.0 then
        local rotFix = compute_ads_rotation_correction()
        if rotFix ~= nil then
            rotX = armsBaseRot.X + SLIDE_TILT_ROLL_DEG  * slideBlend + (adsRollOffset + rotFix.X) * adsBlend
            rotY = armsBaseRot.Y + SLIDE_TILT_PITCH_DEG * slideBlend + (ADS_TILT_PITCH_DEG + rotFix.Y) * adsBlend
            rotZ = armsBaseRot.Z + (ADS_TILT_YAW_DEG + rotFix.Z) * adsBlend
            arms:SetRotation(Vector.new(rotX, rotY, rotZ))
        end
    end

    -- 3) ADS 위치 보정.
    --    SetLocation(world) 을 쓰면 GetLocation 캐시 문제로 매 프레임 delta 가 누적되어
    --    총이 카메라쪽으로 빨려들어가는 버그가 있었음. 대신 월드 delta 를 카메라(=캐릭터 캡슐)
    --    의 축으로 투영해서 relative location 에만 더한다. 캡슐의 forward/right/up = 카메라
    --    의 forward/right/up 이므로 축 일치.
    if adsBlend > 0.0 then
        local worldDelta = compute_ads_world_delta()
        if worldDelta ~= nil and camera ~= nil then
            local fwd   = camera.Forward
            local right = camera.Right
            local up    = camera.Up
            local dFwd   = worldDelta.X * fwd.X   + worldDelta.Y * fwd.Y   + worldDelta.Z * fwd.Z
            local dRight = worldDelta.X * right.X + worldDelta.Y * right.Y + worldDelta.Z * right.Z
            local dUp    = worldDelta.X * up.X    + worldDelta.Y * up.Y    + worldDelta.Z * up.Z
            finalLocX = locX + dFwd   * adsBlend
            finalLocY = locY + dRight * adsBlend
            finalLocZ = locZ + dUp    * adsBlend
            arms:SetRelativeLocation(Vector.new(
                finalLocX,
                finalLocY,
                finalLocZ))
        end
    end

    -- 4) Viewport-space muzzle centering. This is intentionally after the world/bone solve,
    -- because the player judges ADS by pixels, not by raw world coordinates.
    if adsBlend > 0.0 then
        for _ = 1, ADS_SCREEN_CORRECTION_PASSES do
            local screenDelta = compute_ads_screen_delta()
            if screenDelta ~= nil then
                finalLocX = finalLocX + screenDelta.X * adsBlend
                finalLocY = finalLocY + screenDelta.Y * adsBlend
                finalLocZ = finalLocZ + screenDelta.Z * adsBlend
                arms:SetRelativeLocation(Vector.new(finalLocX, finalLocY, finalLocZ))
            end
        end
    end

    update_ads_post_fx(adsBlend)

    if ADS_DEBUG_LOG and adsBlend > 0.0 then
        adsDebugTimer = adsDebugTimer + dt
        if adsDebugTimer >= 0.25 then
            adsDebugTimer = 0.0
            debug_ads_projection("tracking")
        end
    end

    if adsBlend <= 0.0 and lastAdsSettledState ~= "hip" then
        lastAdsSettledState = "hip"
        debug_ads_state("settled-hip")
        debug_ads_projection("settled-hip")
    elseif adsBlend >= 1.0 and lastAdsSettledState ~= "ads" then
        lastAdsSettledState = "ads"
        debug_ads_state("settled-ads")
        debug_ads_projection("settled-ads")
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

    return is_crouch_input_down()
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
    if is_aim_down() or weaponSpread <= 0.0 then
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

    if not (is_aim_down() and ADS_SUPPRESS_SHOOT_ANIMATION) then
        request_arm_animation("shoot")
    end
    GameAudio.NotifyShotFired(is_aim_down())

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

    if not is_aim_down() then
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
    reset_weapon_bone_edit_pose("begin")
    movement = nil
    baseMaxWalkSpeed = nil
    baseSprintSpeedMultiplier = nil
    baseWallRunMaxSpeed = nil
    armsBaseLoc = nil
    armsBaseRot = nil
    armsBaseScale = nil
    slideBlend = 0.0
    adsBlend = 0.0
    if camera ~= nil then
        if type(camera.GetFOV) == "function" then
            fovBase = camera:GetFOV()
        end
        if type(camera.GetDOFEnabled) == "function" then
            dofBaseEnabled = camera:GetDOFEnabled()
            dofBaseFocusDistance = camera:GetDOFFocusDistance()
            dofBaseFStop = camera:GetDOFFStop()
        end
    end
    cache_movement_defaults()
    cache_arms_base_transform()
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
        movement = movement,
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
    if camera ~= nil then
        if fovBase ~= nil and type(camera.SetFOV) == "function" then
            camera:SetFOV(fovBase)
        end
        if dofBaseEnabled ~= nil and type(camera.SetDOFEnabled) == "function" then
            camera:SetDOFEnabled(dofBaseEnabled)
            if dofBaseFocusDistance ~= nil then camera:SetDOFFocusDistance(dofBaseFocusDistance) end
            if dofBaseFStop ~= nil then camera:SetDOFFStop(dofBaseFStop) end
        end
    end
    if arms ~= nil and armsBaseScale ~= nil and type(arms.SetRelativeScale) == "function" then
        arms:SetRelativeScale(armsBaseScale)
    end
    reset_weapon_bone_edit_pose("end")
    fovBase = nil
    dofBaseEnabled = nil
    dofBaseFocusDistance = nil
    dofBaseFStop = nil
    adsBlend = 0.0
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
    update_arm_transforms(dt)

    cache_movement_defaults()
    if movement ~= nil and baseSprintSpeedMultiplier ~= nil then
        local adsNow = is_aim_down()
        if adsNow then
            movement:SetSprintSpeedMultiplier(1.0)
        else
            movement:SetSprintSpeedMultiplier(baseSprintSpeedMultiplier)
        end
        if adsNow ~= _lastAdsForSprint then
            _lastAdsForSprint = adsNow
            print(string.format(
                "[ADS-sprint] ads=%s base_mult=%.3f current_mult=%.3f max_walk=%.3f is_sprinting=%s",
                tostring(adsNow),
                baseSprintSpeedMultiplier,
                movement:GetSprintSpeedMultiplier(),
                movement:GetMaxWalkSpeed(),
                tostring(movement:IsSprinting())))
        end
    end

    if fireCooldown > 0 then
        fireCooldown = fireCooldown - dt
    end

    if weaponSpread > 0.0 then
        weaponSpread = weaponSpread - SPREAD_DECAY_PER_SEC * dt
        if weaponSpread < 0.0 then weaponSpread = 0.0 end
    end

    if is_aim_down() then
        weaponSpread = 0.0
    end

    GameAudio.UpdateWeaponFireState(
        is_aim_down(),
        is_fire_down() and not isReloading and currentAmmo > 0)

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

    local itemInspectInputConsumed = ItemInspectSystem.Tick(camera, obj) == true
    if ItemInspectSystem.IsOpen() then
        stop_player_action_audio()
        return
    end

    GameAudio.UpdateMovement(movement, dt)

    if not itemInspectInputConsumed and is_reload_pressed() then
        start_reload()
    end

    if is_fire_down() and fireCooldown <= 0 then
        try_shoot()
        fireCooldown = FIRE_INTERVAL
        GameAudio.UpdateWeaponFireState(
            is_aim_down(),
            is_fire_down() and not isReloading and currentAmmo > 0)
    end

end
