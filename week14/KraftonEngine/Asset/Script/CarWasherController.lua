local ObjRegistry = require("ObjRegistry")
local bCarWashSucceeded = false
local previousPhase = nil
local gunForwardOffset = 1.0
local gunRightOffset = 0.3
local WATER_LOOP_SOUND_NAME = "Water"
local WATER_LOOP_NAME = "CarWashWaterLoop"
local COMPLETE_SOUND_NAME = "Complete"
local COMPLETE_SOUND_PATH = "complete.mp3"
local bWaterLoopPlaying = false
local previousUnwashedDirtCount = nil
local bCompleteSoundLoaded = false

local function UpdateCarWasherTransform()
    local man = ObjRegistry.manObj
    local manCamera = ObjRegistry.manCamera
    if man == nil or manCamera == nil then
        return
    end

    obj.Location = man.Location + manCamera.Forward * gunForwardOffset + manCamera.Right * gunRightOffset
    obj.Rotation = Vector.new(manCamera.Rotation.X * (-1.0), manCamera.Rotation.Y * (-1.0), manCamera.Rotation.Z + 180)
end

function BeginPlay()
    ObjRegistry.RegisterCarWasher(obj)
    LoadAudio(COMPLETE_SOUND_NAME, COMPLETE_SOUND_PATH)
    bCompleteSoundLoaded = true
    obj:SetVisible(false)
    obj:SetCarWashStreamVisible(false)
end 

local function SetWaterLoopPlaying(bShouldPlay)
    if bShouldPlay == bWaterLoopPlaying then
        return
    end

    bWaterLoopPlaying = bShouldPlay
    if bWaterLoopPlaying then
        AudioManager.PlayLoop(WATER_LOOP_SOUND_NAME, WATER_LOOP_NAME, 0.8, 1.0)
    else
        AudioManager.StopLoop(WATER_LOOP_NAME)
    end
end

local function PlayDirtCompleteSound()
    if not bCompleteSoundLoaded then
        LoadAudio(COMPLETE_SOUND_NAME, COMPLETE_SOUND_PATH)
        bCompleteSoundLoaded = true
    end

    AudioManager.Play(COMPLETE_SOUND_NAME, 1.0)
end

local function UpdateDirtCompleteSound(car)
    local unwashedDirtCount = car:CountUnwashedDirtComponents()
    if previousUnwashedDirtCount ~= nil and unwashedDirtCount < previousUnwashedDirtCount then
        for i = 1, previousUnwashedDirtCount - unwashedDirtCount do
            PlayDirtCompleteSound()
        end
    end

    previousUnwashedDirtCount = unwashedDirtCount
    return unwashedDirtCount
end

function EndPlay()
    SetWaterLoopPlaying(false)
    ObjRegistry.UnregisterCarWasher(obj)
end

function Tick(dt)
    local gs = GetGameState()
    if gs == nil then return false end

    local phase = gs:GetPhase()
    local bIsCarWashPhase = phase == ECarGamePhase.CarWash

    if previousPhase ~= phase then
        previousPhase = phase
        if bIsCarWashPhase then
            bCarWashSucceeded = false
            previousUnwashedDirtCount = nil
            obj:SetVisible(true)
        else
            previousUnwashedDirtCount = nil
            obj:SetVisible(false)
            obj:SetCarWashStreamVisible(false)
            SetWaterLoopPlaying(false)
        end
    end

    if bIsCarWashPhase then
        UpdateCarWasherTransform()

        local isSpraying = Input.GetKey(Key.Space)
        obj:SetCarWashStreamVisible(isSpraying)
        SetWaterLoopPlaying(isSpraying)

        if obj:IsCarWashStreamVisible() then
            obj:FireCarWashRay()
        end

        local car = ObjRegistry.dirtyCar
        if car ~= nil then
            local unwashedDirtCount = UpdateDirtCompleteSound(car)
            if not bCarWashSucceeded and unwashedDirtCount == 0 then
                bCarWashSucceeded = true
                obj:SetCarWashStreamVisible(false)
                SetWaterLoopPlaying(false)
                local gameMode = GetGameMode()
                if gameMode ~= nil then
                    gameMode:SuccessPhase()
                end
            end
        end
    else
        obj:SetCarWashStreamVisible(false)
        SetWaterLoopPlaying(false)
    end
end
