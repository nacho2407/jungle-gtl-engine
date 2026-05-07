local PresentationSequence = require("Game.Core.PresentationSequence")

local EndingState = {}
EndingState.__index = EndingState

local FADE_DURATION = 0.5
local OPENING_FLASH_COUNT = 10
local OPENING_FLASH_HALF_DURATION = 0.05
local FIRST_CAMERA_HOLD = 3.0
local SECOND_CAMERA_HOLD = 3.0
local FINAL_CAMERA_BLEND = 5.0
local BODY_BLINK_DURATION = 3.0
local BODY_BLINK_INTERVAL = 0.18
local LETTERBOX_ASPECT = 2.35

local function GetPlayerController()
    return Engine.API.GetPlayerController()
end

local function StartCameraFade(fromAlpha, toAlpha, duration, r, g, b)
    local pc = GetPlayerController()
    if pc ~= nil then
        pc:StartCameraFade(fromAlpha, toAlpha, duration, r or 0.0, g or 0.0, b or 0.0)
    end
end

local function SetCameraByTag(tag, blendTime, blendType)
    local pc = GetPlayerController()
    local actor = Engine.API.World.FindActorByTag(tag)
    if pc == nil or actor == nil then
        Engine.API.Debug.Warn("[EndingState] Missing camera tag: " .. tostring(tag))
        return nil
    end

    pc:SetViewTargetWithBlend(actor, blendTime or 0.0, blendType or CameraBlendType.SmoothStep)
    return actor
end

local function SetActorPrimitiveVisible(actor, visible)
    if actor == nil then
        return
    end

    actor.Visible = visible == true
end

local function SetTaggedPrimitiveVisible(tag, visible)
    local actors = Engine.API.World.FindActorsByTag(tag)
    if actors == nil then
        return {}
    end

    for _, actor in ipairs(actors) do
        SetActorPrimitiveVisible(actor, visible)
    end
    return actors
end

local function SetActorAndPrimitiveVisible(actor, visible)
    if actor == nil then
        return
    end

    actor.Visible = visible == true
end

local function SetTaggedActorVisible(tag, visible)
    local actors = Engine.API.World.FindActorsByTag(tag)
    if actors == nil then
        return {}
    end

    for _, actor in ipairs(actors) do
        SetActorAndPrimitiveVisible(actor, visible)
    end
    return actors
end

local function DestroyTaggedActors(tag)
    local actors = Engine.API.World.FindActorsByTag(tag)
    if actors == nil then
        return
    end

    for _, actor in ipairs(actors) do
        if actor ~= nil then
            Engine.API.World.DestroyActor(actor)
        end
    end
end

local function StopActorSequence(actor)
    if actor == nil then
        return
    end

    local sequence = actor:GetActorSequenceComponent()
    if sequence ~= nil then
        sequence:Stop()
        sequence:SetComponentTickEnabled(false)
    end
end

local function SetComponentsTickEnabled(actor, componentType, enabled)
    if actor == nil then
        return
    end

    local components = actor:GetComponentsByType(componentType)
    if components == nil then
        return
    end

    for _, component in ipairs(components) do
        if component ~= nil then
            component:SetComponentTickEnabled(enabled == true)
        end
    end
end

local function FreezePlayerActorsForEnding()
    local players = Engine.API.World.FindActorsByTag("Player")
    if players == nil then
        return
    end

    for _, player in ipairs(players) do
        StopActorSequence(player)
        SetComponentsTickEnabled(player, "ScriptComponent", false)
    end
end

local function HideGameUI(ui)
    if ui == nil then
        return
    end

    ui:HideAllOverlays()
    ui:Hide("HUD")
    ui:Hide("Loading")
    ui:Hide("Pause")
    ui:Hide("Result")
end

local function HasRequiredCameras()
    return Engine.API.World.FindActorByTag("Camera1") ~= nil
        and Engine.API.World.FindActorByTag("Camera2") ~= nil
        and Engine.API.World.FindActorByTag("Camera3") ~= nil
        and Engine.API.World.FindActorByTag("Camera4") ~= nil
        and Engine.API.World.FindActorByTag("Camera5") ~= nil
        and Engine.API.World.FindActorByTag("Camera6") ~= nil
end

local function AppendOpeningFlashes(sequence)
    for _ = 1, OPENING_FLASH_COUNT do
        sequence
            :Call(function()
                StartCameraFade(0.0, 1.0, OPENING_FLASH_HALF_DURATION, 0.0, 0.0, 0.0)
            end)
            :Delay(OPENING_FLASH_HALF_DURATION)
            :Call(function()
                StartCameraFade(1.0, 0.0, OPENING_FLASH_HALF_DURATION, 0.0, 0.0, 0.0)
            end)
            :Delay(OPENING_FLASH_HALF_DURATION)
    end
end

function EndingState.new()
    return setmetatable({
        presentation = PresentationSequence.new(),
        snapshot = nil,
        bodyActors = {},
        cubeActors = {},
        blinkRemaining = 0.0,
        blinkIntervalRemaining = 0.0,
        blinkVisible = true,
        bodiesConverted = false
    }, EndingState)
end

function EndingState:Enter(context, snapshot)
    self.snapshot = snapshot or {}
    self.bodyActors = {}
    self.cubeActors = {}
    self.blinkRemaining = 0.0
    self.blinkIntervalRemaining = 0.0
    self.blinkVisible = true
    self.bodiesConverted = false

    Engine.API.World.SetTimeScale(1.0)
    HideGameUI(context.managers.UI)
    Engine.API.Input.SetInputModeUIOnly()
    Engine.API.Input.SetCursorVisible(false)
    DestroyTaggedActors("Enemy")
    context.managers.Sound:PlayBGM(context.managers.Sound.BGM.Clear, 0.5)

    if not HasRequiredCameras() then
        Engine.API.Debug.Warn("[EndingState] Missing required camera tags. Skipping ending cinematic.")
        context.stateMachine:Change("Result", self.snapshot)
        return
    end

    local pc = GetPlayerController()
    if pc ~= nil then
        pc:ClearCameraLetterbox()
        pc:ClearCameraVignette()
        pc:StopCameraEffects()
    end
    FreezePlayerActorsForEnding()

    self.presentation:Clear()
    AppendOpeningFlashes(self.presentation)
    self.presentation
        :Call(function()
            SetTaggedActorVisible("Player", false)
            StartCameraFade(0.0, 1.0, FADE_DURATION, 0.0, 0.0, 0.0)
        end)
        :Delay(FADE_DURATION)
        :Call(function()
            self.bodyActors = SetTaggedPrimitiveVisible("Body", true)
            self.cubeActors = SetTaggedPrimitiveVisible("Cube", false)
            SetTaggedActorVisible("Sword", true)
            SetTaggedPrimitiveVisible("EndingVisible", true)
            SetCameraByTag("Camera1", 0.0, CameraBlendType.Linear)
            local controller = GetPlayerController()
            if controller ~= nil then
                controller:StartCameraLetterbox(LETTERBOX_ASPECT, FADE_DURATION)
            end
            StartCameraFade(1.0, 0.0, FADE_DURATION, 0.0, 0.0, 0.0)
        end)
        :Delay(FADE_DURATION)
        :Call(function()
            SetCameraByTag("Camera5", FIRST_CAMERA_HOLD, CameraBlendType.SmoothStep)
        end)
        :Delay(FIRST_CAMERA_HOLD)
        :Call(function()
            StartCameraFade(0.0, 1.0, FADE_DURATION, 0.0, 0.0, 0.0)
        end)
        :Delay(FADE_DURATION)
        :Call(function()
            local camera2 = SetCameraByTag("Camera2", 0.0, CameraBlendType.Linear)
            SetTaggedActorVisible("Sword", true)
            StartCameraFade(1.0, 0.0, FADE_DURATION, 0.0, 0.0, 0.0)
        end)
        :Delay(FADE_DURATION)
        :Call(function()
            SetCameraByTag("Camera6", SECOND_CAMERA_HOLD, CameraBlendType.SmoothStep)
        end)
        :Delay(SECOND_CAMERA_HOLD)
        :Call(function()
            StartCameraFade(0.0, 1.0, FADE_DURATION, 0.0, 0.0, 0.0)
        end)
        :Delay(FADE_DURATION)
        :Call(function()
            SetCameraByTag("Camera3", 0.0, CameraBlendType.Linear)
            StartCameraFade(1.0, 0.0, FADE_DURATION, 0.0, 0.0, 0.0)
        end)
        :Delay(FADE_DURATION)
        :Call(function()
            SetCameraByTag("Camera4", FINAL_CAMERA_BLEND, CameraBlendType.SmoothStep)
            self:StartBodyBlink()
        end)
        :Delay(FINAL_CAMERA_BLEND)
        :Call(function()
            StartCameraFade(0.0, 1.0, FADE_DURATION, 0.0, 0.0, 0.0)
        end)
        :Delay(FADE_DURATION)
        :Call(function()
            context.stateMachine:Change("Result", self.snapshot)
        end)
        :Play()
end

function EndingState:StartBodyBlink()
    if self.bodyActors == nil or #self.bodyActors == 0 then
        self.bodyActors = Engine.API.World.FindActorsByTag("Body") or {}
    end
    if self.cubeActors == nil or #self.cubeActors == 0 then
        self.cubeActors = Engine.API.World.FindActorsByTag("Cube") or {}
    end

    self.blinkRemaining = BODY_BLINK_DURATION
    self.blinkIntervalRemaining = BODY_BLINK_INTERVAL
    self.blinkVisible = true
    self:SetBodiesAndCubesVisible(true, false)
end

function EndingState:SetBodiesVisible(visible)
    if self.bodyActors == nil then
        return
    end

    for _, actor in ipairs(self.bodyActors) do
        if actor ~= nil then
            SetActorPrimitiveVisible(actor, visible)
        end
    end
end

function EndingState:SetCubesVisible(visible)
    if self.cubeActors == nil then
        return
    end

    for _, actor in ipairs(self.cubeActors) do
        if actor ~= nil then
            SetActorPrimitiveVisible(actor, visible)
        end
    end
end

function EndingState:SetBodiesAndCubesVisible(bodyVisible, cubeVisible)
    self:SetBodiesVisible(bodyVisible)
    self:SetCubesVisible(cubeVisible)
end

function EndingState:ConvertBodiesToCubes()
    if self.bodiesConverted then
        return
    end
    self.bodiesConverted = true

    if self.bodyActors == nil then
        return
    end

    if self.cubeActors == nil or #self.cubeActors == 0 then
        self.cubeActors = Engine.API.World.FindActorsByTag("Cube") or {}
    end

    self:SetBodiesAndCubesVisible(false, true)
end

function EndingState:TickBodyBlink(dt)
    if self.blinkRemaining <= 0.0 then
        return
    end

    self.blinkRemaining = self.blinkRemaining - dt
    self.blinkIntervalRemaining = self.blinkIntervalRemaining - dt

    if self.blinkIntervalRemaining <= 0.0 then
        self.blinkIntervalRemaining = BODY_BLINK_INTERVAL
        self.blinkVisible = not self.blinkVisible
        self:SetBodiesAndCubesVisible(self.blinkVisible, not self.blinkVisible)
    end

    if self.blinkRemaining <= 0.0 then
        self.blinkRemaining = 0.0
        self:ConvertBodiesToCubes()
    end
end

function EndingState:Tick(context, dt)
    local realDt = Engine.API.World.GetUnscaledDeltaTime() or dt or 0.0
    self.presentation:Tick(realDt)
    self:TickBodyBlink(realDt)
end

function EndingState:Exit(context)
    self.presentation:Stop()
    self.snapshot = nil
    self.bodyActors = {}
    self.cubeActors = {}
    self.blinkRemaining = 0.0
    self.blinkIntervalRemaining = 0.0
    self.bodiesConverted = false

    local pc = GetPlayerController()
    if pc ~= nil then
        pc:ClearCameraLetterbox()
        pc:ClearCameraVignette()
        pc:StopCameraFade()
    end
end

return EndingState
