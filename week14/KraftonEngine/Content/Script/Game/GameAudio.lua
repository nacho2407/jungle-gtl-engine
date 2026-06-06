local GameAudio = {}

local initialized = false
local environmentPlaying = false
local weaponFireLoopPlaying = false
local weaponFireLoopKey = nil
local slideLoopPlaying = false
local wallRunRubPlaying = false

local movementState = {
    footstepDistance = 0.0,
    runStepTimer = 0.0,
    slideDistance = 0.0,
    wallrunDistance = 0.0,
    wallrunStepTimer = 0.0,
    wallrunTime = 0.0,
    wasSliding = false,
    wasWallRunning = false,
    wasFalling = false,
    lastFallDownSpeed = 0.0,
}

local WALK_MIN_SPEED = 1.0
local WALK_STRIDE_DISTANCE = 2.1
local RUN_STEP_INTERVAL = 0.32
local SLIDE_MIN_SPEED = 3.0
local SLIDE_STRIDE_DISTANCE = 2.4
local WALLRUN_STEP_INTERVAL = 0.30
local WALLRUN_LOOP_START_DELAY = 0.42
local HEAVY_LAND_DOWN_SPEED = 9.0

local function can_audio()
    return AudioManager ~= nil
end

local function play_event(eventName)
    if can_audio() and AudioManager.PlayEvent ~= nil then
        return AudioManager.PlayEvent(eventName)
    end
    return false
end

local function load_audio(key, path, loop)
    if can_audio() and AudioManager.Load ~= nil then
        AudioManager.Load(key, path, loop == true)
    end
end

local function play_loop(key, loopName, volume, pitch)
    if can_audio() and AudioManager.PlayLoop ~= nil then
        AudioManager.PlayLoop(key, loopName, volume or 1.0, pitch or 1.0)
    end
end

local function stop_loop(loopName)
    if can_audio() and AudioManager.StopLoop ~= nil then
        AudioManager.StopLoop(loopName)
    end
end

local function stop_event(eventName)
    if can_audio() and AudioManager.StopEvent ~= nil then
        AudioManager.StopEvent(eventName)
    end
end

local function fade_out_event(eventName, fadeMilliseconds)
    if can_audio() and AudioManager.FadeOutEvent ~= nil then
        AudioManager.FadeOutEvent(eventName, fadeMilliseconds or 120.0)
    end
end

local function planar_speed(velocity)
    if velocity == nil then return 0.0 end
    return math.sqrt((velocity.X or 0.0) * (velocity.X or 0.0) + (velocity.Y or 0.0) * (velocity.Y or 0.0))
end

local function reset_movement_state()
    movementState.footstepDistance = 0.0
    movementState.runStepTimer = 0.0
    movementState.slideDistance = 0.0
    movementState.wallrunDistance = 0.0
    movementState.wallrunStepTimer = 0.0
    movementState.wallrunTime = 0.0
    movementState.wasSliding = false
    movementState.wasWallRunning = false
    movementState.wasFalling = false
    movementState.lastFallDownSpeed = 0.0
end

function GameAudio.Initialize()
    if initialized then return end
    initialized = true

    load_audio("WindBgm", "Environment/Winds.mp3", true)
    load_audio("SlideLoop", "Movement/Slide/SlideLoop1.wav", true)
    load_audio("WallRunRub", "Movement/WallRun/WallRunRub1.wav", true)
    load_audio("WeaponFireLoop1", "Weapon/Fire/FireLoop1.wav", true)
    load_audio("WeaponFireLoop2", "Weapon/Fire/FireLoop2.wav", true)
    load_audio("WeaponFireLoop3", "Weapon/Fire/FireLoop3.wav", true)

    reset_movement_state()
end

function GameAudio.Shutdown()
    GameAudio.StopWeaponFireLoop()
    GameAudio.StopSlideLoop(false)
    GameAudio.StopWallRunRub()

    if environmentPlaying and can_audio() and AudioManager.StopBGM ~= nil then
        AudioManager.StopBGM()
    end
    environmentPlaying = false
    initialized = false
    reset_movement_state()
end

function GameAudio.PlayEvent(eventName)
    return play_event(eventName)
end

function GameAudio.PlayDoubleJumpJet()
    if play_event("player.jump.jet") then
        fade_out_event("player.jump.jet", 420.0)
    end
end

function GameAudio.StopFallingAudio()
    stop_event("player.jump.jet")
end

function GameAudio.PlayEnvironmentWind(volume)
    GameAudio.Initialize()
    if can_audio() and AudioManager.PlayBGM ~= nil then
        AudioManager.PlayBGM("WindBgm", volume or 0.35)
        environmentPlaying = true
    end
end

function GameAudio.StartWeaponFireLoop()
    GameAudio.Initialize()
    if weaponFireLoopPlaying then return end

    weaponFireLoopKey = "WeaponFireLoop" .. tostring(math.random(1, 3))
    play_loop(weaponFireLoopKey, "PlayerWeaponFireLoop", 0.86, 1.0)
    weaponFireLoopPlaying = true
end

function GameAudio.StopWeaponFireLoop()
    stop_loop("PlayerWeaponFireLoop")
    weaponFireLoopPlaying = false
    weaponFireLoopKey = nil
end

function GameAudio.StartSlideLoop()
    GameAudio.Initialize()
    if slideLoopPlaying then return end

    play_loop("SlideLoop", "PlayerSlideLoop", 0.50, 1.0)
    slideLoopPlaying = true
end

function GameAudio.StopSlideLoop(playEndSound)
    local wasActive = slideLoopPlaying or movementState.wasSliding

    stop_event("player.slide.start")
    stop_loop("PlayerSlideLoop")
    slideLoopPlaying = false
    movementState.slideDistance = 0.0
    movementState.wasSliding = false

    if wasActive and playEndSound ~= false then
        play_event("player.slide.end")
    end
end

function GameAudio.UpdateSlideState(isSliding)
    if not isSliding then
        GameAudio.StopSlideLoop(true)
    end
end

function GameAudio.StartWallRunRub()
    GameAudio.Initialize()
    if wallRunRubPlaying then return end

    play_loop("WallRunRub", "PlayerWallRunRub", 0.48, 1.0)
    wallRunRubPlaying = true
end

function GameAudio.StopWallRunRub()
    if not wallRunRubPlaying then return end

    stop_loop("PlayerWallRunRub")
    wallRunRubPlaying = false
end

function GameAudio.UpdateWallRunState(isWallRunning)
    if not isWallRunning then
        GameAudio.StopWallRunRub()
        movementState.wallrunDistance = 0.0
        movementState.wallrunStepTimer = 0.0
        movementState.wallrunTime = 0.0
        movementState.wasWallRunning = false
    end
end

function GameAudio.UpdateWeaponFireState(isAiming, isFiring)
    if not isFiring then
        GameAudio.StopWeaponFireLoop()
    end
end

function GameAudio.NotifyShotFired(isAiming)
    play_event("weapon.fire")

    if isAiming then
        GameAudio.StartWeaponFireLoop()
    end
end

function GameAudio.UpdateMovement(movement, dt)
    if movement == nil or dt == nil or dt <= 0.0 then
        GameAudio.UpdateSlideState(false)
        GameAudio.UpdateWallRunState(false)
        GameAudio.StopFallingAudio()
        return
    end
    if movement.GetVelocity == nil then
        GameAudio.UpdateSlideState(false)
        GameAudio.UpdateWallRunState(false)
        GameAudio.StopFallingAudio()
        return
    end

    local velocity = movement:GetVelocity()
    local speed = planar_speed(velocity)
    local isWalking = movement.IsWalking ~= nil and movement:IsWalking()
    local isFalling = movement.IsFalling ~= nil and movement:IsFalling()
    local isWallRunning = movement.IsWallRunning ~= nil and movement:IsWallRunning()
    local isSprinting = movement.IsSprinting ~= nil and movement:IsSprinting()
    local isCrouching = movement.IsCrouching ~= nil and movement:IsCrouching()
    local isSliding = isWalking and isCrouching and speed >= SLIDE_MIN_SPEED

    if Input ~= nil and Input.GetKeyDown ~= nil and Key ~= nil and Input.GetKeyDown(Key.Space) then
        if isFalling then
            play_event("player.double_jump")
            GameAudio.PlayDoubleJumpJet()
        elseif isWalking or isWallRunning then
            play_event("player.jump")
        end
    end

    if isFalling and velocity ~= nil then
        movementState.lastFallDownSpeed = math.max(movementState.lastFallDownSpeed, -(velocity.Z or 0.0))
    end

    if movementState.wasFalling and not isFalling then
        GameAudio.StopFallingAudio()
    end

    if movementState.wasFalling and isWalking then
        if movementState.lastFallDownSpeed >= HEAVY_LAND_DOWN_SPEED then
            play_event("player.land.heavy")
        else
            play_event("player.land")
        end
        movementState.lastFallDownSpeed = 0.0
    end

    if isSliding then
        if not movementState.wasSliding then
            movementState.slideDistance = 0.0
            play_event("player.slide.start")
            GameAudio.StartSlideLoop()
        end

        movementState.slideDistance = movementState.slideDistance + speed * dt
        if movementState.slideDistance >= SLIDE_STRIDE_DISTANCE then
            movementState.slideDistance = movementState.slideDistance % SLIDE_STRIDE_DISTANCE
            play_event("player.slide.rub")
        end
    elseif movementState.wasSliding then
        GameAudio.UpdateSlideState(false)
    end

    if isWallRunning then
        if not movementState.wasWallRunning then
            movementState.wallrunDistance = 0.0
            movementState.wallrunStepTimer = 0.0
            movementState.wallrunTime = 0.0
        end

        movementState.wallrunTime = movementState.wallrunTime + dt
        if movementState.wallrunTime >= WALLRUN_LOOP_START_DELAY then
            GameAudio.StartWallRunRub()
        end

        movementState.wallrunStepTimer = movementState.wallrunStepTimer + dt
        if movementState.wallrunStepTimer >= WALLRUN_STEP_INTERVAL then
            movementState.wallrunStepTimer = movementState.wallrunStepTimer % WALLRUN_STEP_INTERVAL
            play_event("player.wallrun.step")
        end
    elseif movementState.wasWallRunning then
        GameAudio.UpdateWallRunState(false)
    end

    if isWalking and not isSliding and isSprinting and speed >= WALK_MIN_SPEED then
        movementState.footstepDistance = 0.0
        movementState.runStepTimer = movementState.runStepTimer + dt
        if movementState.runStepTimer >= RUN_STEP_INTERVAL then
            movementState.runStepTimer = movementState.runStepTimer % RUN_STEP_INTERVAL
            play_event("player.run.step")
        end
    elseif isWalking and not isSliding and speed >= WALK_MIN_SPEED then
        movementState.runStepTimer = 0.0
        movementState.footstepDistance = movementState.footstepDistance + speed * dt
        if movementState.footstepDistance >= WALK_STRIDE_DISTANCE then
            movementState.footstepDistance = movementState.footstepDistance % WALK_STRIDE_DISTANCE
            play_event("player.walk.step")
        end
    else
        movementState.footstepDistance = 0.0
        movementState.runStepTimer = 0.0
    end

    movementState.wasSliding = isSliding
    movementState.wasWallRunning = isWallRunning
    movementState.wasFalling = isFalling
end

return GameAudio
