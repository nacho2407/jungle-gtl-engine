local WeaponHud = require("HUD/WeaponHud")

local STORY_MODULE = "Dialogue/DrakePreCombat.dialogue"
local VOICE_MODULE = "Dialogue/Generated/DrakePreCombat.voices"

local PLAYER_PAWN_NAME = "kain-temp"
local INTERACT_TEXT = "ACCESS"
local VOICE_END_PADDING = 0.35
local DEFAULT_ENTRY_DURATION = 2.5
local DEFAULT_FADE_IN = 0.3
local FADE_OUT_DURATION = 0.35
local LETTERBOX_CLOSE_DELAY = 0.35
local SKIP_HOLD_DURATION = 3.0
local ACKNOWLEDGED_ENTRY_ID = "DrakePreCombat_Drake_Acknowledged"
local STAY_PUT_ENTRY_ID = "DrakePreCombat_Drake_StayPut"
local BOSS_GUN_PREFAB = "Content/Prefab/EnemyBossGun.prefab"
local BOSS_BODY_PREFAB = "Content/Prefab/EnemyBossBody.prefab"
local BOSS_SPAWN_LOCATION = Vector.new(0.0, 0.0, 100.0)
local BOSS_LAND_LOCATION = Vector.new(0.0, 0.0, 0.0)
local BOSS_LOOK_OFFSET = Vector.new(0.0, 0.0, 5.5)
local CAMERA_TURN_DURATION = 0.85
local BOSS_DROP_DELAY = 0.15
local BOSS_DROP_DURATION = 0.45
local BOSS_IMPACT_HOLD_DURATION = 0.7
local BOSS_IMPACT_SHAKE_SCALE = 1.45
local BOSS_MOVE_IDLE = 0
local BOSS_MOVE_LEAP_FLOAT = 6
local BOSS_LEAP_START_ACTION = "leapStart"
local BOSS_LEAP_START_DURATION = 0.55
local BOSS_LEAP_LAND_ACTION = "leapLand"
local BOSS_LEAP_LAND_DURATION = 0.7
local BOSS_IMPACT_PARTICLE_SPAWN_DURATION = 3.0
local BOSS_IMPACT_PARTICLE_LIFETIME = 8.0
local BOSS_IMPACT_PARTICLES = {
    "Content/Particle/SmokeDirtBlack.uasset",
    "Content/Particle/SmokeDirt.uasset",
}
local POST_COMBAT_INTERACT_TEXT = "INSERT KEY"
local POST_COMBAT_LOCKED_TEXT = "KEY REQUIRED"
local ENDING_MODULE = "Ending"

local story = nil
local voiceEntries = {}
local voiceEntriesById = {}
local loadedVoiceKeys = {}
local activeEntry = nil
local activeVoiceKey = nil
local currentIndex = 0
local entryTimer = 0.0
local entryDuration = 0.0
local skipHoldTime = 0.0

local cutsceneStarted = false
local cutsceneActive = false
local cutsceneClosing = false
local closeTimer = 0.0
local cutscenePhase = "idle"

local bossGunActor = nil
local bossBodyActor = nil
local bossSpawned = false
local bossControlSerial = 0
local bossImpactPlayed = false
local bossImpactParticles = {}
local revealStage = "none"
local revealTimer = 0.0
local postCombatInteractionRegistered = false
local postCombatInteractionHasKey = false

local playerActorForCamera = nil
local cameraComponent = nil
local playerStartControlRotation = nil
local playerStartYaw = 0.0
local cameraStartPitch = 0.0

local inputLocked = false
local movementLocked = false
local autoInputLocked = false
local lockedMovement = nil
local savedMaxWalkSpeed = nil
local savedSprintSpeedMultiplier = nil
local savedWallRunMaxSpeed = nil
local savedAutoInputWASD = nil
local savedAutoInputMouseLook = nil

local start_cutscene = nil
local begin_cutscene_close = nil

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function lerp(a, b, alpha)
    return a + (b - a) * alpha
end

local function smoothstep(alpha)
    local t = clamp(alpha, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)
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

local function normalize_angle(angle)
    while angle > 180.0 do
        angle = angle - 360.0
    end
    while angle < -180.0 do
        angle = angle + 360.0
    end
    return angle
end

local function lerp_angle(a, b, alpha)
    return a + normalize_angle(b - a) * alpha
end

local function vector_lerp(a, b, alpha)
    return Vector.new(
        lerp(a.X, b.X, alpha),
        lerp(a.Y, b.Y, alpha),
        lerp(a.Z, b.Z, alpha))
end

local function get_key(name)
    if Key == nil then return nil end
    return Key[name]
end

local function get_raw_key(key)
    if key == nil or Input == nil then return false end
    if Input.GetRawKey ~= nil then return Input.GetRawKey(key) end
    if Input.GetKey ~= nil then return Input.GetKey(key) end
    return false
end

local function is_skip_held()
    return get_raw_key(get_key("Ctrl"))
        or get_raw_key(get_key("LeftCtrl"))
        or get_raw_key(get_key("RightCtrl"))
        or get_raw_key(get_key("GamepadB"))
end

local function is_valid_actor(actor)
    if actor == nil then return false end
    if type(actor.IsValid) == "function" then
        return actor:IsValid()
    end
    return true
end

local function get_actor_location(actor)
    if not is_valid_actor(actor) then
        return nil
    end
    if type(actor.GetActorLocation) == "function" then
        return actor:GetActorLocation()
    end
    return actor.Location
end

local function set_actor_location(actor, location)
    if not is_valid_actor(actor) or location == nil then
        return
    end
    if type(actor.SetActorLocation) == "function" then
        actor:SetActorLocation(location)
    else
        actor.Location = location
    end
end

local function get_actor_rotation(actor)
    if not is_valid_actor(actor) then
        return nil
    end
    if type(actor.GetActorRotation) == "function" then
        return actor:GetActorRotation()
    end
    return actor.Rotation
end

local function set_actor_rotation(actor, rotation)
    if not is_valid_actor(actor) or rotation == nil then
        return
    end
    if type(actor.SetActorRotation) == "function" then
        actor:SetActorRotation(rotation)
    else
        actor.Rotation = rotation
    end
end

local function get_player_actor()
    if Game ~= nil and Game.GetPlayerPawn ~= nil then
        local pawn = Game.GetPlayerPawn()
        if is_valid_actor(pawn) then
            return pawn
        end
    end

    if World ~= nil and World.FindActorByName ~= nil then
        local player = World.FindActorByName(PLAYER_PAWN_NAME)
        if is_valid_actor(player) then
            return player
        end
    end

    return nil
end

local function get_player_camera(player)
    if is_valid_actor(player) and type(player.GetCamera) == "function" then
        return player:GetCamera()
    end
    return nil
end

local function get_player_control_rotation(player)
    if is_valid_actor(player) and type(player.GetControlRotation) == "function" then
        local controlRotation = player:GetControlRotation()
        if controlRotation ~= nil then
            return controlRotation
        end
    end
    return get_actor_rotation(player) or Vector.new(0.0, 0.0, 0.0)
end

local function set_player_control_rotation(player, rotation)
    if not is_valid_actor(player) or rotation == nil then
        return
    end

    local appliedControlRotation = false
    if type(player.SetControlRotation) == "function" then
        appliedControlRotation = player:SetControlRotation(rotation) == true
    end

    -- ControlRotation을 지원하지 않는 액터에서도 최소한 몸 방향은 맞춰지도록 Actor yaw를 보조 동기화합니다.
    local actorRotation = get_actor_rotation(player) or Vector.new(0.0, 0.0, 0.0)
    set_actor_rotation(player, Vector.new(actorRotation.X, actorRotation.Y, rotation.Z))

    if not appliedControlRotation then
        set_actor_rotation(player, Vector.new(rotation.X, rotation.Y, rotation.Z))
    end
end

local function get_boss_look_target()
    local control = _G.Level3BossIntroBossControl
    local bossLocation = control ~= nil and control.ForcedLocation or get_actor_location(bossBodyActor) or BOSS_LAND_LOCATION
    return bossLocation + BOSS_LOOK_OFFSET
end

local function calculate_yaw(fromLocation, targetLocation)
    if fromLocation == nil or targetLocation == nil then
        return 0.0
    end

    local delta = targetLocation - fromLocation
    return math.deg(atan2(delta.Y, delta.X))
end

local function calculate_pitch(fromLocation, targetLocation)
    if fromLocation == nil or targetLocation == nil then
        return 0.0
    end

    local delta = targetLocation - fromLocation
    local horizontal = math.sqrt(delta.X * delta.X + delta.Y * delta.Y)
    return -math.deg(atan2(delta.Z, horizontal))
end

local function initialize_camera_turn()
    playerActorForCamera = get_player_actor()
    cameraComponent = get_player_camera(playerActorForCamera)

    playerStartControlRotation = get_player_control_rotation(playerActorForCamera)
    playerStartYaw = playerStartControlRotation.Z
    cameraStartPitch = playerStartControlRotation.Y
end

local function update_camera_to_boss(alpha)
    local player = playerActorForCamera
    if not is_valid_actor(player) then
        player = get_player_actor()
        playerActorForCamera = player
    end
    if not is_valid_actor(player) then
        return
    end

    local camera = cameraComponent
    if camera == nil then
        camera = get_player_camera(player)
        cameraComponent = camera
    end

    local targetLocation = get_boss_look_target()
    local playerLocation = get_actor_location(player)
    local desiredYaw = calculate_yaw(playerLocation, targetLocation)
    local desiredPitch = calculate_pitch(camera ~= nil and camera:GetWorldLocation() or playerLocation, targetLocation)
    local blend = alpha == nil and 1.0 or smoothstep(alpha)
    local yaw = alpha == nil and desiredYaw or lerp_angle(playerStartYaw, desiredYaw, blend)
    local pitch = alpha == nil and desiredPitch or lerp_angle(cameraStartPitch, desiredPitch, blend)
    local currentControlRotation = get_player_control_rotation(player)
    set_player_control_rotation(player, Vector.new(currentControlRotation.X, pitch, yaw))
end

local function get_dialogue_text(entry)
    if entry == nil then return "" end
    local speaker = entry.speaker or ""
    local text = entry.text or entry.text_en or ""
    if speaker == "" then
        return text
    end
    return speaker .. ": " .. text
end

local function load_dialogue_assets()
    story = require(STORY_MODULE)
    voiceEntries = {}
    voiceEntriesById = {}

    local ok, voices = pcall(require, VOICE_MODULE)
    if ok and voices ~= nil then
        voiceEntries = voices.entries or {}
        voiceEntriesById = voices.by_id or {}
    end
end

local function get_voice_entry(entry, index)
    if entry ~= nil and entry.id ~= nil and voiceEntriesById ~= nil then
        local byIdEntry = voiceEntriesById[entry.id]
        if byIdEntry ~= nil then
            return byIdEntry
        end
    end
    return voiceEntries ~= nil and voiceEntries[index] or nil
end

local function stop_current_voice()
    if activeVoiceKey ~= nil and AudioManager ~= nil and AudioManager.Stop ~= nil then
        AudioManager.Stop(activeVoiceKey)
    end
    activeVoiceKey = nil
end

local function play_voice(entry, index)
    if AudioManager == nil or AudioManager.Load == nil or AudioManager.Play == nil then
        return nil
    end

    local voiceEntry = get_voice_entry(entry, index)
    if voiceEntry == nil or voiceEntry.key == nil or voiceEntry.path == nil then
        return nil
    end

    stop_current_voice()
    if not loadedVoiceKeys[voiceEntry.key] then
        if not AudioManager.Load(voiceEntry.key, voiceEntry.path, false) then
            return nil
        end
        loadedVoiceKeys[voiceEntry.key] = true
    end

    AudioManager.Play(voiceEntry.key, voiceEntry.volume or 1.0)
    activeVoiceKey = voiceEntry.key
    return voiceEntry
end

local function show_dialogue_entry(entry, index)
    if entry == nil then return end

    local text = get_dialogue_text(entry)
    local fontSize = entry.size or (story ~= nil and story.default_size) or 22
    local lineHeight = fontSize + 26
    local width = clamp(42.0 + string.len(text) * fontSize * 0.52, 520.0, 1280.0)
    local height = clamp(lineHeight, 46.0, 72.0)

    if WeaponHud ~= nil and WeaponHud.ShowDialogue ~= nil then
        WeaponHud.ShowDialogue(text, {
            width = width,
            height = height,
            font = entry.font or (story ~= nil and story.default_font) or "Pretendard",
            fontSize = fontSize,
            weight = entry.weight or (story ~= nil and story.default_weight) or 400,
            lineHeight = height,
            opacity = 0.0,
        })
    end

    local voiceEntry = play_voice(entry, index)
    activeEntry = entry
    entryTimer = 0.0
    entryDuration = voiceEntry ~= nil and voiceEntry.duration ~= nil and voiceEntry.duration > 0.0
        and (voiceEntry.duration + VOICE_END_PADDING)
        or (entry.duration or (story ~= nil and story.default_duration) or DEFAULT_ENTRY_DURATION)
end

local function find_entry_index(entryId)
    if story == nil or story.entries == nil or entryId == nil then
        return nil
    end

    for index, entry in ipairs(story.entries) do
        if entry ~= nil and entry.id == entryId then
            return index
        end
    end
    return nil
end

local function get_first_spawned_actor(actors)
    if actors == nil then
        return nil
    end
    return actors[1]
end

local function spawn_prefab_actor(prefabPath, location)
    if World == nil or World.SpawnPrefab == nil then
        print("[Level3BossIntro] World.SpawnPrefab is not available.")
        return nil
    end

    local actors = World.SpawnPrefab(prefabPath, location)
    return get_first_spawned_actor(actors)
end

local function destroy_impact_particles()
    for _, entry in ipairs(bossImpactParticles) do
        if is_valid_actor(entry.Actor) and type(entry.Actor.Destroy) == "function" then
            entry.Actor:Destroy()
        end
    end
    bossImpactParticles = {}
end

local function update_impact_particles(dt)
    for index = #bossImpactParticles, 1, -1 do
        local entry = bossImpactParticles[index]
        if not is_valid_actor(entry.Actor) then
            table.remove(bossImpactParticles, index)
        else
            if not entry.Deactivated then
                entry.SpawnRemaining = entry.SpawnRemaining - dt
                if entry.SpawnRemaining <= 0.0 then
                    local particle = type(entry.Actor.GetParticleSystem) == "function" and entry.Actor:GetParticleSystem() or nil
                    if particle ~= nil and type(particle.Deactivate) == "function" then
                        particle:Deactivate()
                    end
                    entry.Deactivated = true
                end
            end

            entry.Remaining = entry.Remaining - dt
            if entry.Remaining <= 0.0 then
                if type(entry.Actor.Destroy) == "function" then
                    entry.Actor:Destroy()
                end
                table.remove(bossImpactParticles, index)
            end
        end
    end
end

local function spawn_boss_impact_particles(location)
    if bossImpactPlayed or location == nil or World == nil or World.SpawnParticleSystem == nil then
        return
    end

    destroy_impact_particles()

    for _, particlePath in ipairs(BOSS_IMPACT_PARTICLES) do
        local particleActor = World.SpawnParticleSystem(particlePath, location)
        if is_valid_actor(particleActor) then
            particleActor:AddTag("runtime-boss-impact-smoke")
            bossImpactParticles[#bossImpactParticles + 1] = {
                Actor = particleActor,
                SpawnRemaining = BOSS_IMPACT_PARTICLE_SPAWN_DURATION,
                Remaining = BOSS_IMPACT_PARTICLE_LIFETIME,
                Deactivated = false,
            }
        end
    end
end

local function set_boss_cutscene_active(active)
    if active then
        _G.Level3BossIntroCutsceneActive = true
        return
    end

    _G.Level3BossIntroCutsceneActive = false
    _G.Level3BossIntroBossUUID = nil
    _G.Level3BossIntroBossControl = nil
    _G.Level3BossIntroBossRelease = nil
end

local function publish_boss_control(moveState, actionName, actionDuration, forcedLocation)
    if not is_valid_actor(bossBodyActor) then
        return
    end

    local control = _G.Level3BossIntroBossControl or {}
    control.Active = true
    control.BossUUID = bossBodyActor.UUID
    control.MoveState = moveState or BOSS_MOVE_IDLE
    control.AimPitch = 0.0
    control.AimWeight = 0.0
    if forcedLocation ~= nil then
        control.ForcedLocation = forcedLocation
    end

    if actionName ~= nil then
        bossControlSerial = bossControlSerial + 1
        control.ActionName = actionName
        control.ActionDuration = actionDuration
        control.ActionSerial = bossControlSerial
    end

    _G.Level3BossIntroBossUUID = bossBodyActor.UUID
    _G.Level3BossIntroBossControl = control
end

local function spawn_boss_if_needed()
    if bossSpawned and is_valid_actor(bossBodyActor) then
        return true
    end

    -- 총 프리팹을 먼저 스폰하면 바디가 뒤이어 생성된 뒤 기존 본 부착 로직이 안전하게 따라붙습니다.
    bossGunActor = spawn_prefab_actor(BOSS_GUN_PREFAB, BOSS_SPAWN_LOCATION)
    bossBodyActor = spawn_prefab_actor(BOSS_BODY_PREFAB, BOSS_SPAWN_LOCATION)
    bossSpawned = is_valid_actor(bossBodyActor)

    if bossSpawned then
        set_actor_location(bossBodyActor, BOSS_SPAWN_LOCATION)
        _G.Level3BossIntroBossUUID = bossBodyActor.UUID
        publish_boss_control(BOSS_MOVE_IDLE, BOSS_LEAP_START_ACTION, BOSS_LEAP_START_DURATION, BOSS_SPAWN_LOCATION)
    else
        print("[Level3BossIntro] Boss body prefab spawn failed.")
    end

    return bossSpawned
end

local function play_impact_shake()
    if bossImpactPlayed then
        return
    end
    spawn_boss_impact_particles(BOSS_LAND_LOCATION)
    bossImpactPlayed = true

    if CameraManager ~= nil and CameraManager.StartSequenceShake ~= nil then
        CameraManager.StartSequenceShake(BOSS_IMPACT_SHAKE_SCALE)
    elseif CameraManager ~= nil and CameraManager.StartWaveShake ~= nil then
        CameraManager.StartWaveShake(BOSS_IMPACT_SHAKE_SCALE)
    end
end

local function release_boss_for_combat()
    if is_valid_actor(bossBodyActor) then
        _G.Level3BossIntroBossRelease = {
            BossUUID = bossBodyActor.UUID,
            ForcedLocation = BOSS_LAND_LOCATION,
        }
    end

    set_boss_cutscene_active(false)

    if is_valid_actor(bossBodyActor) then
        _G.Level3BossIntroBossRelease = {
            BossUUID = bossBodyActor.UUID,
            ForcedLocation = BOSS_LAND_LOCATION,
        }
    end
end

local function settle_boss_for_combat()
    if not bossSpawned then
        spawn_boss_if_needed()
    end

    if is_valid_actor(bossBodyActor) then
        set_actor_location(bossBodyActor, BOSS_LAND_LOCATION)
        set_actor_location(bossGunActor, BOSS_LAND_LOCATION)
        publish_boss_control(BOSS_MOVE_IDLE, nil, nil, BOSS_LAND_LOCATION)
        play_impact_shake()
        update_camera_to_boss(nil)
    end
end

local function unregister_interaction()
    if obj ~= nil and obj.UUID ~= nil and _G.InspectableItems ~= nil then
        _G.InspectableItems[obj.UUID] = nil
    end
    postCombatInteractionRegistered = false
end

local function restore_player_after_cutscene()
    if inputLocked and Game ~= nil and Game.SetInputPossessed ~= nil then
        Game.SetInputPossessed(true)
    end
    if Game ~= nil and Game.SetMouseCaptureWhileInputBlocked ~= nil then
        Game.SetMouseCaptureWhileInputBlocked(false)
    end
    inputLocked = false

    if autoInputLocked then
        local player = get_player_actor()
        if player ~= nil and player.SetCharacterAutoInput ~= nil then
            player:SetCharacterAutoInput(savedAutoInputWASD ~= false, savedAutoInputMouseLook ~= false)
        end
    end
    autoInputLocked = false
    savedAutoInputWASD = nil
    savedAutoInputMouseLook = nil

    if movementLocked and lockedMovement ~= nil then
        if savedMaxWalkSpeed ~= nil then lockedMovement:SetMaxWalkSpeed(savedMaxWalkSpeed) end
        if savedSprintSpeedMultiplier ~= nil then lockedMovement:SetSprintSpeedMultiplier(savedSprintSpeedMultiplier) end
        if savedWallRunMaxSpeed ~= nil then lockedMovement:SetWallRunMaxSpeed(savedWallRunMaxSpeed) end
    end
    movementLocked = false
    lockedMovement = nil
    savedMaxWalkSpeed = nil
    savedSprintSpeedMultiplier = nil
    savedWallRunMaxSpeed = nil
end

local function lock_player_for_cutscene()
    if not inputLocked and Game ~= nil and Game.SetInputPossessed ~= nil then
        -- 일반 Lua 입력 스냅샷을 비워 이동/사격/상호작용 입력을 막고, 스킵만 raw input으로 받습니다.
        if Game.SetMouseCaptureWhileInputBlocked ~= nil then
            Game.SetMouseCaptureWhileInputBlocked(true)
        end
        Game.SetInputPossessed(false)
        inputLocked = true
    end

    local player = get_player_actor()
    if player == nil then
        return
    end

    if not autoInputLocked
        and player.GetCharacterAutoInputWASD ~= nil
        and player.GetCharacterAutoInputMouseLook ~= nil
        and player.SetCharacterAutoInput ~= nil then
        -- 컷씬 중 비활성 입력이 카메라 회전값으로 누적되지 않도록 자동 입력을 잠급니다.
        savedAutoInputWASD = player:GetCharacterAutoInputWASD()
        savedAutoInputMouseLook = player:GetCharacterAutoInputMouseLook()
        player:SetCharacterAutoInput(false, false)
        autoInputLocked = true
    end

    if movementLocked or player.GetCharacterMovement == nil then
        return
    end

    lockedMovement = player:GetCharacterMovement()
    if lockedMovement == nil then
        return
    end

    -- 입력 차단 직전 관성이나 보류된 이동값이 남아도 플레이어가 컷씬 중 움직이지 않도록 속도 계수를 잠급니다.
    savedMaxWalkSpeed = lockedMovement:GetMaxWalkSpeed()
    savedSprintSpeedMultiplier = lockedMovement:GetSprintSpeedMultiplier()
    savedWallRunMaxSpeed = lockedMovement:GetWallRunMaxSpeed()
    lockedMovement:SetMaxWalkSpeed(0.0)
    lockedMovement:SetSprintSpeedMultiplier(0.0)
    lockedMovement:SetWallRunMaxSpeed(0.0)
    movementLocked = true
end

local function complete_cutscene_close()
    cutsceneClosing = false
    cutscenePhase = "idle"
    closeTimer = 0.0
    release_boss_for_combat()
    restore_player_after_cutscene()
end

local function show_skip_ui()
    if WeaponHud ~= nil and WeaponHud.ShowSkipPrompt ~= nil then
        WeaponHud.ShowSkipPrompt({
            keyboardText = "Ctrl",
            gamepadText = "B",
            label = "SKIP",
        })
    end
    if WeaponHud ~= nil and WeaponHud.SetSkipProgress ~= nil then
        WeaponHud.SetSkipProgress(0.0)
    end
end

local function hide_skip_ui()
    if WeaponHud ~= nil and WeaponHud.HideSkipPrompt ~= nil then
        WeaponHud.HideSkipPrompt()
    elseif WeaponHud ~= nil and WeaponHud.SetSkipProgress ~= nil then
        WeaponHud.SetSkipProgress(0.0)
    end
end

local function show_final_entry()
    local finalIndex = find_entry_index(STAY_PUT_ENTRY_ID)
    if finalIndex == nil then
        begin_cutscene_close({ forceBossReady = true })
        return
    end

    cutscenePhase = "final_dialogue"
    currentIndex = finalIndex
    show_dialogue_entry(story.entries[finalIndex], finalIndex)
end

local function begin_boss_reveal()
    stop_current_voice()
    activeEntry = nil
    entryTimer = 0.0
    entryDuration = 0.0

    if WeaponHud ~= nil and WeaponHud.HideDialogue ~= nil then
        WeaponHud.HideDialogue()
    end

    cutscenePhase = "boss_reveal"
    revealStage = "turn"
    revealTimer = 0.0
    bossImpactPlayed = false

    set_boss_cutscene_active(true)
    spawn_boss_if_needed()
    initialize_camera_turn()
end

local function update_boss_reveal(dt)
    if revealStage == "turn" then
        revealTimer = revealTimer + dt
        update_camera_to_boss(clamp(revealTimer / CAMERA_TURN_DURATION, 0.0, 1.0))

        if revealTimer >= CAMERA_TURN_DURATION then
            revealStage = "drop_wait"
            revealTimer = 0.0
        end
        return
    end

    if revealStage == "drop_wait" then
        revealTimer = revealTimer + dt
        update_camera_to_boss(nil)

        if revealTimer >= BOSS_DROP_DELAY then
            revealStage = "drop"
            revealTimer = 0.0
            publish_boss_control(BOSS_MOVE_LEAP_FLOAT, nil, nil, BOSS_SPAWN_LOCATION)
        end
        return
    end

    if revealStage == "drop" then
        revealTimer = revealTimer + dt
        local alpha = smoothstep(revealTimer / BOSS_DROP_DURATION)
        local bossLocation = vector_lerp(BOSS_SPAWN_LOCATION, BOSS_LAND_LOCATION, alpha)
        set_actor_location(bossBodyActor, bossLocation)
        publish_boss_control(BOSS_MOVE_LEAP_FLOAT, nil, nil, bossLocation)
        update_camera_to_boss(nil)

        if revealTimer >= BOSS_DROP_DURATION then
            set_actor_location(bossBodyActor, BOSS_LAND_LOCATION)
            play_impact_shake()
            publish_boss_control(BOSS_MOVE_IDLE, BOSS_LEAP_LAND_ACTION, BOSS_LEAP_LAND_DURATION, BOSS_LAND_LOCATION)
            revealStage = "impact"
            revealTimer = 0.0
        end
        return
    end

    if revealStage == "impact" then
        revealTimer = revealTimer + dt
        update_camera_to_boss(nil)

        if revealTimer >= BOSS_IMPACT_HOLD_DURATION then
            show_final_entry()
        end
    end
end

begin_cutscene_close = function(options)
    local forceBossReady = options ~= nil and options.forceBossReady == true
    stop_current_voice()
    activeEntry = nil
    cutsceneActive = false
    cutsceneClosing = true
    cutscenePhase = "closing"
    closeTimer = LETTERBOX_CLOSE_DELAY
    skipHoldTime = 0.0
    hide_skip_ui()

    if forceBossReady then
        settle_boss_for_combat()
        release_boss_for_combat()
    else
        set_boss_cutscene_active(false)
    end

    if WeaponHud ~= nil and WeaponHud.HideDialogue ~= nil then
        WeaponHud.HideDialogue()
    end
    if WeaponHud ~= nil and WeaponHud.HideLetterbox ~= nil then
        WeaponHud.HideLetterbox()
    end
end

local function show_next_entry()
    stop_current_voice()
    currentIndex = currentIndex + 1

    if story == nil or story.entries == nil or currentIndex > #story.entries then
        begin_cutscene_close({ forceBossReady = bossSpawned })
        return
    end

    show_dialogue_entry(story.entries[currentIndex], currentIndex)
end

start_cutscene = function()
    if cutsceneStarted then
        return
    end

    cutsceneStarted = true
    cutsceneActive = true
    cutsceneClosing = false
    cutscenePhase = "dialogue"
    closeTimer = 0.0
    currentIndex = 0
    skipHoldTime = 0.0
    revealStage = "none"
    revealTimer = 0.0
    bossImpactPlayed = false
    bossControlSerial = 0
    playerStartControlRotation = nil

    unregister_interaction()
    lock_player_for_cutscene()
    set_boss_cutscene_active(true)

    if WeaponHud ~= nil and WeaponHud.ShowLetterbox ~= nil then
        WeaponHud.ShowLetterbox()
    end
    show_skip_ui()

    show_next_entry()
end

local function register_interaction()
    if obj == nil or obj.UUID == nil then
        return
    end

    _G.InspectableItems = _G.InspectableItems or {}
    _G.InspectableItems[obj.UUID] = {
        actor = obj,
        interact_text = INTERACT_TEXT,
        on_interact = function()
            start_cutscene()
            return true
        end,
    }
end

local function has_vantus_master_key()
    return _G.PlayerHasVantusMasterKey == true
        or (_G.PickedUpItems ~= nil and _G.PickedUpItems.vantus_master_key == true)
end

local function start_post_combat_ending()
    if not has_vantus_master_key() then
        return true
    end

    unregister_interaction()

    local ok, ending = pcall(require, ENDING_MODULE)
    if not ok or ending == nil or ending.Start == nil then
        print("[Level3BossIntro] Ending module unavailable: " .. tostring(ending))
        return true
    end

    ending.Start()
    return true
end

local function register_post_combat_interaction()
    if obj == nil or obj.UUID == nil then
        return
    end

    local hasKey = has_vantus_master_key()
    if postCombatInteractionRegistered and postCombatInteractionHasKey == hasKey then
        return
    end

    postCombatInteractionRegistered = true
    postCombatInteractionHasKey = hasKey

    _G.InspectableItems = _G.InspectableItems or {}
    _G.InspectableItems[obj.UUID] = {
        actor = obj,
        interact_text = hasKey and POST_COMBAT_INTERACT_TEXT or POST_COMBAT_LOCKED_TEXT,
        on_interact = function()
            return start_post_combat_ending()
        end,
    }
end

local function update_skip(dt)
    if is_skip_held() then
        skipHoldTime = skipHoldTime + dt
        if WeaponHud ~= nil and WeaponHud.SetSkipProgress ~= nil then
            WeaponHud.SetSkipProgress(skipHoldTime / SKIP_HOLD_DURATION)
        end
        if skipHoldTime >= SKIP_HOLD_DURATION then
            begin_cutscene_close({ forceBossReady = true })
            return true
        end
    else
        skipHoldTime = 0.0
        if WeaponHud ~= nil and WeaponHud.SetSkipProgress ~= nil then
            WeaponHud.SetSkipProgress(0.0)
        end
    end

    return false
end

function BeginPlay()
    story = nil
    voiceEntries = {}
    voiceEntriesById = {}
    loadedVoiceKeys = {}
    activeEntry = nil
    activeVoiceKey = nil
    currentIndex = 0
    entryTimer = 0.0
    entryDuration = 0.0
    skipHoldTime = 0.0
    cutsceneStarted = false
    cutsceneActive = false
    cutsceneClosing = false
    closeTimer = 0.0
    cutscenePhase = "idle"
    bossGunActor = nil
    bossBodyActor = nil
    bossSpawned = false
    bossControlSerial = 0
    bossImpactPlayed = false
    bossImpactParticles = {}
    revealStage = "none"
    revealTimer = 0.0
    postCombatInteractionRegistered = false
    postCombatInteractionHasKey = false
    playerActorForCamera = nil
    cameraComponent = nil
    playerStartControlRotation = nil
    playerStartYaw = 0.0
    cameraStartPitch = 0.0
    inputLocked = false
    movementLocked = false
    autoInputLocked = false
    lockedMovement = nil
    savedMaxWalkSpeed = nil
    savedSprintSpeedMultiplier = nil
    savedWallRunMaxSpeed = nil
    savedAutoInputWASD = nil
    savedAutoInputMouseLook = nil

    load_dialogue_assets()
    set_boss_cutscene_active(false)
    register_interaction()
end

function EndPlay()
    unregister_interaction()
    stop_current_voice()

    if WeaponHud ~= nil and WeaponHud.HideDialogue ~= nil then
        WeaponHud.HideDialogue()
    end
    if WeaponHud ~= nil and WeaponHud.HideLetterbox ~= nil then
        WeaponHud.HideLetterbox({ instant = true })
    end
    hide_skip_ui()

    restore_player_after_cutscene()
    set_boss_cutscene_active(false)
    destroy_impact_particles()
    activeEntry = nil
    cutsceneActive = false
    cutsceneClosing = false
    cutscenePhase = "idle"
end

function Tick(dt)
    dt = dt or 0.0
    update_impact_particles(dt)

    if cutsceneClosing then
        closeTimer = closeTimer - dt
        if closeTimer <= 0.0 then
            complete_cutscene_close()
        end
        return
    end

    if not cutsceneActive then
        if cutsceneStarted and _G.Level3BossDefeated == true then
            register_post_combat_interaction()
        end
        return
    end

    if update_skip(dt) then
        return
    end

    if cutscenePhase == "boss_reveal" then
        update_boss_reveal(dt)
        return
    end

    if cutscenePhase == "final_dialogue" then
        update_camera_to_boss(nil)
    end

    if activeEntry == nil then
        return
    end

    entryTimer = entryTimer + dt

    local fadeIn = activeEntry.fade_in or (story ~= nil and story.default_fade_in) or DEFAULT_FADE_IN
    local alpha = 1.0
    if entryTimer < fadeIn then
        alpha = entryTimer / fadeIn
    elseif entryTimer > entryDuration - FADE_OUT_DURATION then
        alpha = (entryDuration - entryTimer) / FADE_OUT_DURATION
    end
    alpha = clamp(alpha, 0.0, 1.0)

    if WeaponHud ~= nil and WeaponHud.SetDialogueOpacity ~= nil then
        WeaponHud.SetDialogueOpacity(alpha)
    end

    if entryTimer >= entryDuration then
        local finishedEntryId = activeEntry.id
        if finishedEntryId == ACKNOWLEDGED_ENTRY_ID then
            begin_boss_reveal()
        elseif finishedEntryId == STAY_PUT_ENTRY_ID then
            begin_cutscene_close({ forceBossReady = true })
        else
            show_next_entry()
        end
    end
end
