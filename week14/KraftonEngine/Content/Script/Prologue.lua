local WeaponHud = require("HUD/WeaponHud")

local story = nil
local cutsceneWidget = nil
local currentIndex = 0
local entryTime = 0.0
local entryDuration = 0.0
local fadeInDuration = 0.7
local activeEntry = nil
local skipHoldTime = 0.0
local voiceManifest = nil
local voiceEntries = nil
local voiceEntriesById = nil
local loadedVoiceKeys = {}
local pendingAdvance = false
local currentVoiceDuration = 0.0
local currentVoiceKey = nil
local sceneTime = 0.0
local cutsceneFinished = false
local landingPodActor = nil
local landingStartLocation = nil
local landingCutsceneCameraActor = nil
local cutsceneCameraInitialized = false
local cutsceneCameraWarned = false
local playerMovement = nil
local playerMaxWalkSpeed = nil
local playerSprintSpeedMultiplier = nil
local playerWallRunMaxSpeed = nil
local playerMovementLocked = false
local playerAutoInputWASD = nil
local playerAutoInputMouseLook = nil
local playerAutoInputLocked = false
local playerInputPossessionLocked = false
local dialogueStartDelayRemaining = 0.0
local dialogueStarted = false
local landingDescentEndTime = 39.5
local landingFinishTime = 41.0
local postLandingFinishDelayRemaining = 0.0
local postLandingFinishDelayActive = false
local landingShakeTimer = 0.0
local landingShakeStopped = false

local SKIP_HOLD_DURATION = 3.0
local SKIP_RING_FRAMES = 24
local INTRO_DIALOGUE_START_DELAY = 5.0
local POST_LANDING_FINISH_DELAY = 3.0
local PRODUCER_CREDIT_START_TIME = 10.0
local PRODUCER_CREDIT_DISPLAY_DURATION = 5.0
local PRODUCER_CREDIT_MAX_FADE_DURATION = 1.2
local VOICE_END_PADDING = 0.25
local DIALOGUE_BOX_WIDTH = 980.0
local DIALOGUE_BOX_HEIGHT = 48.0
local DIALOGUE_LINE_HEIGHT = 48.0
local DIALOGUE_TEXT_LEFT = 16.0
local DIALOGUE_DEFAULT_FONT_SIZE = 18.0
local LANDING_POD_ACTOR_NAME = "landing-pod-mesh"
local LANDING_CUTSCENE_CAMERA_ACTOR_NAME = "prologue-landing-camera"
local PLAYER_PAWN_NAME = "kain-temp"
local LANDING_START_Z = 1500.0
local LANDING_TARGET_Z = -3.119
local LANDING_COMPRESSED_Z = -3.525
local DIALOGUE_END_BEFORE_LANDING_FINISH = 10.0
local LANDING_REBOUND_DURATION = 1.0
local MIN_LANDING_DESCENT_DURATION = 1.0
local PLAYER_CAMERA_BLEND_TIME = 0.65
local CUTSCENE_CAMERA_BLEND_TIME = 0.0
local LANDING_SHAKE_INTERVAL = 0.20
local LANDING_SHAKE_LEAD_TIME = 10.0
local LANDING_SHAKE_FADE_OUT_TIME = 2.0
local LANDING_SHAKE_START_SCALE = 0.12
local LANDING_SHAKE_END_SCALE = 0.35
local PRODUCER_CREDIT_NAMES = {
    "KIM HYOBEOM",
    "JANG MINJUN",
    "JEON HYEONGIL",
}

local function px(value)
    return string.format("%.2fpx", value)
end

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function lerp(a, b, alpha)
    return a + (b - a) * alpha
end

local function ease_out_cubic(alpha)
    local clamped = clamp(alpha, 0.0, 1.0)
    local inv = 1.0 - clamped
    return 1.0 - inv * inv * inv
end

local function smoothstep(alpha)
    local clamped = clamp(alpha, 0.0, 1.0)
    return clamped * clamped * (3.0 - 2.0 * clamped)
end

local function atan2(y, x)
    if x > 0.0 then
        return math.atan(y / x)
    end
    if x < 0.0 and y >= 0.0 then
        return math.atan(y / x) + math.pi
    end
    if x < 0.0 and y < 0.0 then
        return math.atan(y / x) - math.pi
    end
    if y > 0.0 then
        return math.pi * 0.5
    end
    if y < 0.0 then
        return -math.pi * 0.5
    end
    return 0.0
end

local function make_location_with_z(source, z)
    return Vector.new(source.X, source.Y, z)
end

local function log_prologue(message)
    if Game ~= nil and Game.Log ~= nil then
        Game.Log("[Prologue]", message)
    else
        print("[Prologue] " .. message)
    end
end

local function set_weapon_hud_visible(visible)
    if WeaponHud ~= nil and WeaponHud.SetVisible ~= nil then
        WeaponHud.SetVisible(visible)
    end
end

local function stop_current_voice()
    if currentVoiceKey ~= nil and AudioManager ~= nil and AudioManager.Stop ~= nil then
        -- 컷씬 스킵/종료 시 현재 대사 음성이 장면 밖으로 이어지지 않도록 재생만 중지합니다.
        AudioManager.Stop(currentVoiceKey)
    end
    currentVoiceKey = nil
end

local function stop_camera_shakes(immediate)
    if CameraManager ~= nil and CameraManager.StopAllCameraShakes ~= nil then
        -- 스킵/전환은 즉시 정리하고, 정상 착륙 종료는 CameraShake 자체 blend-out을 사용합니다.
        CameraManager.StopAllCameraShakes(immediate ~= false)
    end
end

local function reset_landing_camera_shake_state()
    landingShakeTimer = 0.0
    landingShakeStopped = false
end

local function start_landing_wave_shake(scale)
    if cutsceneFinished then
        return
    end
    if CameraManager ~= nil and CameraManager.StartWaveShake ~= nil then
        CameraManager.StartWaveShake(scale)
    end
end

local function get_cutscene_key_down(key)
    if Input == nil or key == nil then return false end
    if Input.GetRawKeyDown ~= nil then return Input.GetRawKeyDown(key) end
    if Input.GetKeyDown ~= nil then return Input.GetKeyDown(key) end
    return false
end

local function get_cutscene_key(key)
    if Input == nil or key == nil then return false end
    if Input.GetRawKey ~= nil then return Input.GetRawKey(key) end
    if Input.GetKey ~= nil then return Input.GetKey(key) end
    return false
end

local function find_landing_cutscene_camera()
    if landingCutsceneCameraActor ~= nil then
        return landingCutsceneCameraActor
    end
    if World == nil or World.FindActorByName == nil then
        return nil
    end

    landingCutsceneCameraActor = World.FindActorByName(LANDING_CUTSCENE_CAMERA_ACTOR_NAME)
    return landingCutsceneCameraActor
end

local function get_viewport_size()
    if Engine.GetViewportSize == nil then
        return 1280.0, 720.0
    end

    local size = Engine.GetViewportSize()
    return size.Width or 1280.0, size.Height or 720.0
end

local function get_dialogue_font_size(entry)
    if entry ~= nil and entry.size ~= nil then
        return entry.size
    end
    if story ~= nil and story.default_size ~= nil then
        return story.default_size
    end
    return DIALOGUE_DEFAULT_FONT_SIZE
end

local function estimate_dialogue_bounds(entry)
    return DIALOGUE_BOX_WIDTH, DIALOGUE_BOX_HEIGHT, get_dialogue_font_size(entry), DIALOGUE_LINE_HEIGHT
end

local function get_dialogue_text(entry)
    local speaker = entry.speaker or ""
    local text = entry.text or ""
    if speaker == "" then
        return text
    end
    return speaker .. ": " .. text
end

local function update_skip_ring(progress)
    if cutsceneWidget == nil then return end

    local clamped = clamp(progress or 0.0, 0.0, 1.0)
    cutsceneWidget:SetProperty("skip-ring", "opacity", clamped > 0.001 and "1.0" or "0.0")

    local activeFrame = 0
    if clamped > 0.0 then
        activeFrame = math.ceil(clamped * SKIP_RING_FRAMES)
    end
    activeFrame = clamp(activeFrame, 0, SKIP_RING_FRAMES)
    for index = 0, SKIP_RING_FRAMES do
        local id = string.format("skip-ring-frame-%02d", index)
        cutsceneWidget:SetProperty(id, "opacity", index == activeFrame and "1.0" or "0.0")
    end
end

local function hide_producer_credit()
    if cutsceneWidget == nil then
        return
    end

    cutsceneWidget:SetProperty("producer-credit", "opacity", "0.0")
end

local function update_producer_credit()
    if cutsceneWidget == nil then
        return
    end

    local creditCount = #PRODUCER_CREDIT_NAMES
    local creditEndTime = math.min(
        landingFinishTime,
        PRODUCER_CREDIT_START_TIME + creditCount * PRODUCER_CREDIT_DISPLAY_DURATION)
    if creditCount <= 0 or sceneTime < PRODUCER_CREDIT_START_TIME or sceneTime >= creditEndTime then
        hide_producer_credit()
        return
    end

    local segmentDuration = PRODUCER_CREDIT_DISPLAY_DURATION
    if segmentDuration <= 0.0 then
        hide_producer_credit()
        return
    end

    -- 첫 노출 시각은 유지하고, 각 제작자 이름만 고정 시간으로 짧게 노출합니다.
    local localTime = sceneTime - PRODUCER_CREDIT_START_TIME
    local creditIndex = math.floor(localTime / segmentDuration) + 1
    creditIndex = clamp(creditIndex, 1, creditCount)

    local segmentTime = localTime - (creditIndex - 1) * segmentDuration
    local fadeDuration = math.min(PRODUCER_CREDIT_MAX_FADE_DURATION, segmentDuration * 0.25)
    local alpha = 1.0
    if fadeDuration > 0.0 then
        if segmentTime < fadeDuration then
            alpha = smoothstep(segmentTime / fadeDuration)
        elseif segmentTime > segmentDuration - fadeDuration then
            alpha = smoothstep((segmentDuration - segmentTime) / fadeDuration)
        end
    end

    -- 제작자 크레딧은 정해진 표시 구간 동안 한 명씩 순차적으로 노출합니다.
    cutsceneWidget:SetText("producer-credit-name", PRODUCER_CREDIT_NAMES[creditIndex])
    cutsceneWidget:SetText("producer-credit-role", "PRODUCER")
    cutsceneWidget:SetProperty("producer-credit", "opacity", string.format("%.2f", clamp(alpha, 0.0, 1.0)))
end

local function remove_cutscene_widget()
    if cutsceneWidget ~= nil and cutsceneWidget:IsInViewport() then
        cutsceneWidget:RemoveFromParent()
    end
    cutsceneWidget = nil
end

local function find_landing_pod()
    if landingPodActor ~= nil then
        return landingPodActor
    end
    if World == nil or World.FindActorByName == nil then
        return nil
    end

    landingPodActor = World.FindActorByName(LANDING_POD_ACTOR_NAME)
    if landingPodActor ~= nil then
        landingStartLocation = landingPodActor.Location
    end
    return landingPodActor
end

local function set_landing_pod_z(z)
    local pod = find_landing_pod()
    if pod == nil or landingStartLocation == nil then
        return
    end

    -- X/Y는 씬 배치를 유지하고, Z만 컷씬 시간에 맞춰 보간합니다.
    pod.Location = make_location_with_z(landingStartLocation, z)
end

local function prepare_landing_pod_for_intro()
    local pod = find_landing_pod()
    if pod == nil or landingStartLocation == nil then
        return
    end

    landingStartLocation = make_location_with_z(landingStartLocation, LANDING_START_Z)
    set_landing_pod_z(LANDING_START_Z)
end

local function update_landing_pod_motion()
    local pod = find_landing_pod()
    if pod == nil or landingStartLocation == nil then
        return
    end

    local z = LANDING_TARGET_Z
    if sceneTime < landingDescentEndTime then
        -- 중간 정지 없이 압축 높이까지 한 번에 내려가고, 착륙 직전에만 부드럽게 감속합니다.
        local alpha = ease_out_cubic(sceneTime / landingDescentEndTime)
        z = lerp(landingStartLocation.Z, LANDING_COMPRESSED_Z, alpha)
    elseif sceneTime < landingFinishTime then
        -- 눌린 높이에서 최종 높이로 다시 복귀하는 구간입니다.
        local alpha = smoothstep((sceneTime - landingDescentEndTime) / (landingFinishTime - landingDescentEndTime))
        z = lerp(LANDING_COMPRESSED_Z, LANDING_TARGET_Z, alpha)
    end

    pod.Location = make_location_with_z(landingStartLocation, z)
end

local function update_landing_camera_look_at()
    local cameraActor = find_landing_cutscene_camera()
    local pod = find_landing_pod()
    if cameraActor == nil or pod == nil then
        return
    end

    local from = cameraActor.Location
    local to = pod.Location
    local dx = to.X - from.X
    local dy = to.Y - from.Y
    local dz = to.Z - from.Z
    local horizontalDistance = math.sqrt(dx * dx + dy * dy)
    if horizontalDistance <= 0.001 and math.abs(dz) <= 0.001 then
        return
    end

    -- FRotator는 Lua Vector(X=Roll, Y=Pitch, Z=Yaw)로 주고받으므로, 카메라 forward가 pod를 향하도록 변환합니다.
    local yaw = math.deg(atan2(dy, dx))
    local pitch = -math.deg(atan2(dz, horizontalDistance))
    cameraActor.Rotation = Vector.new(0.0, pitch, yaw)
end

local function update_landing_camera_shake(dt)
    if cutsceneFinished or landingFinishTime <= 0.0 then
        return
    end

    local shakeStartTime = landingFinishTime - LANDING_SHAKE_LEAD_TIME
    if sceneTime < shakeStartTime then
        return
    end

    local shakeEndTime = landingFinishTime + LANDING_SHAKE_FADE_OUT_TIME
    if sceneTime >= shakeEndTime then
        if not landingShakeStopped then
            landingShakeStopped = true
            stop_camera_shakes(false)
        end
        return
    end

    landingShakeTimer = landingShakeTimer - dt
    if landingShakeTimer <= 0.0 then
        -- 착륙 10초 전부터 강해지고, 착륙 직후에는 남은 지연 시간 안에서 자연스럽게 사라집니다.
        local shakeScale = 0.0
        if sceneTime < landingFinishTime then
            local shakeAlpha = clamp((sceneTime - shakeStartTime) / LANDING_SHAKE_LEAD_TIME, 0.0, 1.0)
            shakeScale = lerp(LANDING_SHAKE_START_SCALE, LANDING_SHAKE_END_SCALE, smoothstep(shakeAlpha))
        else
            local fadeAlpha = clamp((sceneTime - landingFinishTime) / LANDING_SHAKE_FADE_OUT_TIME, 0.0, 1.0)
            shakeScale = lerp(LANDING_SHAKE_END_SCALE, 0.0, smoothstep(fadeAlpha))
        end

        start_landing_wave_shake(shakeScale)
        landingShakeTimer = LANDING_SHAKE_INTERVAL
    end
end

local function lock_player_movement_for_cutscene()
    if not playerInputPossessionLocked and Game ~= nil and Game.SetInputPossessed ~= nil then
        Game.SetInputPossessed(false)
        playerInputPossessionLocked = true
    end

    if playerInputPossessionLocked then
        return
    end

    if World == nil or World.FindActorByName == nil then
        return
    end

    local playerActor = World.FindActorByName(PLAYER_PAWN_NAME)
    if playerActor == nil then
        return
    end

    if not playerAutoInputLocked
        and playerActor.GetCharacterAutoInputWASD ~= nil
        and playerActor.GetCharacterAutoInputMouseLook ~= nil
        and playerActor.SetCharacterAutoInput ~= nil then
        -- 컷씬 동안 비활성 카메라를 가진 Kain이 마우스 룩을 누적하지 않도록 자동 입력을 잠급니다.
        playerAutoInputWASD = playerActor:GetCharacterAutoInputWASD()
        playerAutoInputMouseLook = playerActor:GetCharacterAutoInputMouseLook()
        playerActor:SetCharacterAutoInput(false, false)
        playerAutoInputLocked = true
    end

    if playerMovementLocked then
        return
    end
    if playerActor.GetCharacterMovement == nil then
        return
    end

    playerMovement = playerActor:GetCharacterMovement()
    if playerMovement == nil then
        return
    end

    -- 컷씬 중 WASD 이동이 누적되지 않도록 이동 속도만 임시로 잠급니다.
    playerMaxWalkSpeed = playerMovement:GetMaxWalkSpeed()
    playerSprintSpeedMultiplier = playerMovement:GetSprintSpeedMultiplier()
    playerWallRunMaxSpeed = playerMovement:GetWallRunMaxSpeed()
    playerMovement:SetMaxWalkSpeed(0.0)
    playerMovement:SetSprintSpeedMultiplier(0.0)
    playerMovement:SetWallRunMaxSpeed(0.0)
    playerMovementLocked = true
end

local function restore_player_movement()
    if Game ~= nil and Game.SetInputPossessed ~= nil then
        -- BeginPlay 시점에는 GameViewport input possession이 아직 false일 수 있습니다.
        -- 컷씬 종료는 실제 플레이 복귀 지점이므로 저장된 초기값에 의존하지 않고 명시적으로 켭니다.
        Game.SetInputPossessed(true)
    end
    playerInputPossessionLocked = false

    if playerAutoInputLocked and World ~= nil and World.FindActorByName ~= nil then
        local playerActor = World.FindActorByName(PLAYER_PAWN_NAME)
        if playerActor ~= nil and playerActor.SetCharacterAutoInput ~= nil then
            playerActor:SetCharacterAutoInput(playerAutoInputWASD ~= false, playerAutoInputMouseLook ~= false)
        end
    end

    playerAutoInputWASD = nil
    playerAutoInputMouseLook = nil
    playerAutoInputLocked = false

    if not playerMovementLocked or playerMovement == nil then
        return
    end

    if playerMaxWalkSpeed ~= nil then
        playerMovement:SetMaxWalkSpeed(playerMaxWalkSpeed)
    end
    if playerSprintSpeedMultiplier ~= nil then
        playerMovement:SetSprintSpeedMultiplier(playerSprintSpeedMultiplier)
    end
    if playerWallRunMaxSpeed ~= nil then
        playerMovement:SetWallRunMaxSpeed(playerWallRunMaxSpeed)
    end

    playerMovement = nil
    playerMaxWalkSpeed = nil
    playerSprintSpeedMultiplier = nil
    playerWallRunMaxSpeed = nil
    playerMovementLocked = false
end

local function try_initialize_cutscene_camera()
    if cutsceneCameraInitialized then
        return
    end
    if Game == nil or Game.GetPlayerController == nil then
        return
    end

    local playerController = Game.GetPlayerController()
    if playerController == nil then
        return
    end

    local pod = find_landing_pod()
    if pod == nil then
        return
    end

    local cameraActor = find_landing_cutscene_camera()
    local viewTarget = cameraActor ~= nil and cameraActor or pod
    local camera = nil
    if cameraActor ~= nil then
        update_landing_camera_look_at()
        if cameraActor.GetCamera ~= nil then
            camera = cameraActor:GetCamera()
        end
    elseif pod.GetCamera ~= nil then
        camera = pod:GetCamera()
        if camera ~= nil and not cutsceneCameraWarned then
            log_prologue(LANDING_CUTSCENE_CAMERA_ACTOR_NAME .. "를 찾지 못해서 " .. LANDING_POD_ACTOR_NAME .. " 카메라를 사용합니다.")
            cutsceneCameraWarned = true
        end
    end

    if camera ~= nil and CameraManager ~= nil and CameraManager.PossessCamera ~= nil then
        CameraManager.PossessCamera(camera)
    elseif not cutsceneCameraWarned then
        log_prologue(viewTarget.Name .. "에 CameraComponent가 없어서 기존 카메라를 유지합니다.")
        cutsceneCameraWarned = true
    end

    if playerController.SetViewTargetWithBlend ~= nil then
        playerController:SetViewTargetWithBlend(viewTarget, CUTSCENE_CAMERA_BLEND_TIME)
    end

    cutsceneCameraInitialized = true
end

local function finish_cutscene_in_current_scene()
    if cutsceneFinished then
        return
    end

    cutsceneFinished = true
    activeEntry = nil
    pendingAdvance = false
    skipHoldTime = 0.0
    currentVoiceDuration = 0.0
    postLandingFinishDelayActive = false
    postLandingFinishDelayRemaining = 0.0
    stop_current_voice()
    stop_camera_shakes(true)
    reset_landing_camera_shake_state()
    update_skip_ring(0.0)
    hide_producer_credit()
    remove_cutscene_widget()
    set_landing_pod_z(LANDING_TARGET_Z)
    restore_player_movement()
    set_weapon_hud_visible(true)

    if Game == nil or Game.GetPlayerController == nil then
        return
    end

    local playerController = Game.GetPlayerController()
    if playerController == nil then
        return
    end

    local playerActor = nil
    if World ~= nil and World.FindActorByName ~= nil then
        playerActor = World.FindActorByName(PLAYER_PAWN_NAME)
    end

    if playerActor ~= nil and playerController.SetViewTargetWithBlend ~= nil then
        playerController:SetViewTargetWithBlend(playerActor, PLAYER_CAMERA_BLEND_TIME)
    end

    if Game.PossessPawnByName ~= nil and Game.PossessPawnByName(PLAYER_PAWN_NAME) then
        return
    end

    local currentPawn = Game.GetPlayerPawn ~= nil and Game.GetPlayerPawn() or nil
    if currentPawn ~= nil and currentPawn.Name == PLAYER_PAWN_NAME and playerController.Possess ~= nil then
        playerController:Possess(currentPawn)
    else
        log_prologue(PLAYER_PAWN_NAME .. " Pawn possess에 실패했습니다.")
    end
end

local function is_waiting_for_voice_end()
    return currentVoiceDuration > 0.0 and entryTime < currentVoiceDuration + VOICE_END_PADDING
end

local function load_voice_manifest(moduleName)
    voiceManifest = nil
    voiceEntries = nil
    voiceEntriesById = nil
    loadedVoiceKeys = {}

    local ok, result = pcall(require, moduleName)
    if not ok or result == nil or result.entries == nil then
        return
    end

    voiceManifest = result
    voiceEntries = {}
    voiceEntriesById = result.by_id or {}
    for _, voiceEntry in ipairs(result.entries) do
        if voiceEntry.index ~= nil then
            voiceEntries[voiceEntry.index] = voiceEntry
        end
        if voiceEntry.id ~= nil then
            voiceEntriesById[voiceEntry.id] = voiceEntry
        end
    end
end

local function get_dialogue_entry_id(entry, index)
    if entry ~= nil and entry.id ~= nil then
        return entry.id
    end
    return string.format("%s_%04d", story.id or "Dialogue", index)
end

local function get_voice_entry(entry, index)
    if voiceEntries == nil then return nil end
    local entryId = get_dialogue_entry_id(entry, index)
    local voiceEntry = nil
    if voiceEntriesById ~= nil then
        voiceEntry = voiceEntriesById[entryId]
    end
    if voiceEntry == nil then
        voiceEntry = voiceEntries[index]
    end
    return voiceEntry
end

local function get_entry_play_duration(entry, index)
    local voiceEntry = get_voice_entry(entry, index)
    if voiceEntry ~= nil and voiceEntry.duration ~= nil and voiceEntry.duration > 0.0 then
        return voiceEntry.duration + VOICE_END_PADDING
    end
    return entry.duration or story.default_duration or 3.4
end

local function get_dialogue_total_duration()
    if story == nil or story.entries == nil then
        return 0.0
    end

    local total = 0.0
    for index, entry in ipairs(story.entries) do
        total = total + get_entry_play_duration(entry, index)
    end
    return total
end

local function configure_landing_timeline()
    local dialogueTotalDuration = get_dialogue_total_duration()
    local desiredFinishTime = INTRO_DIALOGUE_START_DELAY + dialogueTotalDuration + DIALOGUE_END_BEFORE_LANDING_FINISH
    local minimumFinishTime = MIN_LANDING_DESCENT_DURATION + LANDING_REBOUND_DURATION

    -- 대사 길이가 바뀌어도 대사 종료 후 10초 뒤 착륙이 완료되도록 컷씬 시간축에서 역산합니다.
    landingFinishTime = math.max(desiredFinishTime, minimumFinishTime)
    landingDescentEndTime = landingFinishTime - LANDING_REBOUND_DURATION
end

local function play_dialogue_voice(entry, index)
    if voiceEntries == nil or AudioManager == nil then return nil end

    local voiceEntry = get_voice_entry(entry, index)
    if voiceEntry == nil or voiceEntry.key == nil or voiceEntry.path == nil then return nil end

    if not loadedVoiceKeys[voiceEntry.key] then
        local loaded = AudioManager.Load(voiceEntry.key, voiceEntry.path, false)
        if not loaded then return nil end
        loadedVoiceKeys[voiceEntry.key] = true
    end

    AudioManager.Play(voiceEntry.key, voiceEntry.volume or 1.0)
    currentVoiceKey = voiceEntry.key
    return voiceEntry
end

local function apply_entry(entry)
    if cutsceneWidget == nil or entry == nil then
        return
    end

    local viewportWidth, viewportHeight = get_viewport_size()
    local width, height, fontSize, lineHeight = estimate_dialogue_bounds(entry)
    local left = (viewportWidth - width) * 0.5
    local bottom = viewportHeight * 0.135 + 18.0

    cutsceneWidget:SetText("dialogue-line", get_dialogue_text(entry))
    cutsceneWidget:SetProperty("dialogue-box", "left", px(left))
    cutsceneWidget:SetProperty("dialogue-box", "bottom", px(bottom))
    cutsceneWidget:SetProperty("dialogue-box", "width", px(width))
    cutsceneWidget:SetProperty("dialogue-box", "height", px(height))
    cutsceneWidget:SetProperty("dialogue-box", "opacity", "0.0")
    cutsceneWidget:SetProperty("dialogue-line", "left", px(DIALOGUE_TEXT_LEFT))
    cutsceneWidget:SetProperty("dialogue-line", "top", "0px")
    cutsceneWidget:SetProperty("dialogue-line", "width", px(width - DIALOGUE_TEXT_LEFT * 2.0))
    cutsceneWidget:SetProperty("dialogue-line", "height", px(height))
    cutsceneWidget:SetProperty("dialogue-line", "font-family", entry.font or story.default_font or "Pretendard")
    cutsceneWidget:SetProperty("dialogue-line", "font-size", px(fontSize))
    cutsceneWidget:SetProperty("dialogue-line", "font-weight", tostring(entry.weight or story.default_weight or 400))
    cutsceneWidget:SetProperty("dialogue-line", "line-height", px(lineHeight))
    cutsceneWidget:SetProperty("dialogue-line", "text-align", "left")

    cutsceneWidget:SetProperty("skip-ring", "right", "54px")
    cutsceneWidget:SetProperty("skip-ring", "bottom", px(bottom + 6.0))
end

local function hide_dialogue_box()
    if cutsceneWidget == nil then
        return
    end

    -- 마지막 대사가 끝난 뒤에는 레터박스와 스킵 UI는 유지하고, 대사 영역만 비웁니다.
    cutsceneWidget:SetText("dialogue-line", "")
    cutsceneWidget:SetProperty("dialogue-box", "opacity", "0.0")
end

local function begin_post_landing_finish_delay()
    activeEntry = nil
    pendingAdvance = false
    entryTime = 0.0
    entryDuration = 0.0
    currentVoiceDuration = 0.0
    currentVoiceKey = nil
    postLandingFinishDelayActive = true
    postLandingFinishDelayRemaining = POST_LANDING_FINISH_DELAY
    hide_dialogue_box()

    if sceneTime >= landingFinishTime and postLandingFinishDelayRemaining <= 0.0 then
        postLandingFinishDelayActive = false
        finish_cutscene_in_current_scene()
    end
end

local function show_next_entry()
    stop_current_voice()
    currentIndex = currentIndex + 1
    if story == nil or story.entries == nil or currentIndex > #story.entries then
        begin_post_landing_finish_delay()
        return
    end

    activeEntry = story.entries[currentIndex]
    entryTime = 0.0
    entryDuration = activeEntry.duration or story.default_duration or 3.4
    fadeInDuration = activeEntry.fade_in or story.default_fade_in or 0.7
    pendingAdvance = false
    currentVoiceDuration = 0.0
    currentVoiceKey = nil
    apply_entry(activeEntry)
    local voiceEntry = play_dialogue_voice(activeEntry, currentIndex)
    if voiceEntry ~= nil and voiceEntry.duration ~= nil and voiceEntry.duration > 0.0 then
        currentVoiceDuration = voiceEntry.duration
        entryDuration = currentVoiceDuration + VOICE_END_PADDING
    end
end

local function should_advance_dialogue()
    if get_cutscene_key_down(Key.MouseLeft) then return true end
    if get_cutscene_key_down(Key.Space) then return true end
    if Key.Enter ~= nil and get_cutscene_key_down(Key.Enter) then return true end
    return false
end

local function update_scene_skip(dt)
    if Key.Ctrl == nil then return false end

    if get_cutscene_key(Key.Ctrl) then
        skipHoldTime = skipHoldTime + dt
        local progress = clamp(skipHoldTime / SKIP_HOLD_DURATION, 0.0, 1.0)
        update_skip_ring(progress)
        if skipHoldTime >= SKIP_HOLD_DURATION then
            finish_cutscene_in_current_scene()
            return true
        end
    else
        skipHoldTime = 0.0
        update_skip_ring(0.0)
    end

    return false
end

local function play_story(moduleName)
    story = require(moduleName)
    load_voice_manifest("Dialogue/Generated/" .. (story.id or "Prologue") .. ".voices")
    configure_landing_timeline()
    currentIndex = 0
    activeEntry = nil
    pendingAdvance = false
    currentVoiceDuration = 0.0
    currentVoiceKey = nil
    dialogueStarted = false
    dialogueStartDelayRemaining = INTRO_DIALOGUE_START_DELAY
    postLandingFinishDelayActive = false
    postLandingFinishDelayRemaining = 0.0
    reset_landing_camera_shake_state()
end

local function update_dialogue_start_delay(dt)
    if dialogueStarted or story == nil then
        return
    end

    dialogueStartDelayRemaining = dialogueStartDelayRemaining - dt
    if dialogueStartDelayRemaining <= 0.0 then
        -- 컷씬 시작 직후 착륙선과 카메라를 먼저 보여준 뒤 첫 대사를 시작합니다.
        dialogueStarted = true
        dialogueStartDelayRemaining = 0.0
        show_next_entry()
    end
end

local function update_post_landing_finish_delay(dt)
    if not postLandingFinishDelayActive then
        return
    end

    if sceneTime < landingFinishTime then
        return
    end

    postLandingFinishDelayRemaining = postLandingFinishDelayRemaining - dt
    if postLandingFinishDelayRemaining <= 0.0 then
        -- 착륙 완료 이후 컷씬 카메라를 잠깐 유지한 뒤 실제 플레이어로 전환합니다.
        postLandingFinishDelayActive = false
        postLandingFinishDelayRemaining = 0.0
        finish_cutscene_in_current_scene()
    end
end

function BeginPlay()
    sceneTime = 0.0
    cutsceneFinished = false
    landingPodActor = nil
    landingStartLocation = nil
    landingCutsceneCameraActor = nil
    cutsceneCameraInitialized = false
    cutsceneCameraWarned = false
    playerMovement = nil
    playerMaxWalkSpeed = nil
    playerSprintSpeedMultiplier = nil
    playerWallRunMaxSpeed = nil
    playerMovementLocked = false
    playerAutoInputWASD = nil
    playerAutoInputMouseLook = nil
    playerAutoInputLocked = false
    playerInputPossessionLocked = false
    dialogueStartDelayRemaining = 0.0
    dialogueStarted = false
    landingDescentEndTime = 39.5
    landingFinishTime = 41.0
    postLandingFinishDelayRemaining = 0.0
    postLandingFinishDelayActive = false
    currentVoiceKey = nil
    reset_landing_camera_shake_state()

    set_weapon_hud_visible(false)
    cutsceneWidget = UI.CreateWidget("Content/UI/Cutscene/Cutscene.rml")
    if cutsceneWidget ~= nil then
        cutsceneWidget:SetWantsMouse(false)
        cutsceneWidget:AddToViewportZ(120)
        update_skip_ring(0.0)
        hide_producer_credit()
    end
    find_landing_pod()
    prepare_landing_pod_for_intro()
    lock_player_movement_for_cutscene()
    play_story("Dialogue/Prologue.dialogue")
end

function EndPlay()
    stop_current_voice()
    stop_camera_shakes(true)
    reset_landing_camera_shake_state()
    hide_producer_credit()
    remove_cutscene_widget()
    restore_player_movement()
    set_weapon_hud_visible(true)
    story = nil
    voiceManifest = nil
    voiceEntries = nil
    voiceEntriesById = nil
    loadedVoiceKeys = {}
    pendingAdvance = false
    currentVoiceDuration = 0.0
    currentVoiceKey = nil
    dialogueStartDelayRemaining = 0.0
    dialogueStarted = false
    postLandingFinishDelayRemaining = 0.0
    postLandingFinishDelayActive = false
    cutsceneFinished = false
    landingPodActor = nil
    landingStartLocation = nil
    landingCutsceneCameraActor = nil
    cutsceneCameraInitialized = false
    cutsceneCameraWarned = false
    landingDescentEndTime = 39.5
    landingFinishTime = 41.0
end

function Tick(dt)
    if dt == nil then
        dt = 0.0
    end

    if not cutsceneFinished then
        sceneTime = sceneTime + dt
        update_landing_pod_motion()
        update_producer_credit()
        try_initialize_cutscene_camera()
        update_landing_camera_look_at()
        lock_player_movement_for_cutscene()
        if cutsceneWidget ~= nil and update_scene_skip(dt) then
            return
        end
        update_landing_camera_shake(dt)
        update_dialogue_start_delay(dt)
        update_post_landing_finish_delay(dt)
    end

    if cutsceneWidget == nil or activeEntry == nil then
        return
    end

    if should_advance_dialogue() then
        if is_waiting_for_voice_end() then
            pendingAdvance = true
        else
            show_next_entry()
            return
        end
    end

    entryTime = entryTime + dt
    local alpha = 1.0
    if fadeInDuration > 0.0 then
        alpha = clamp(entryTime / fadeInDuration, 0.0, 1.0)
    end
    cutsceneWidget:SetProperty("dialogue-box", "opacity", string.format("%.2f", alpha))

    if entryTime >= entryDuration then
        show_next_entry()
    end
end
