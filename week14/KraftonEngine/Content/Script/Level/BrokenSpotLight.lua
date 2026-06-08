local MIN_STABLE_TIME = 1.2
local MAX_STABLE_TIME = 4.0
local MIN_PULSE_TIME = 0.035
local MAX_PULSE_TIME = 0.16
local MIN_RECOVER_TIME = 0.08
local MAX_RECOVER_TIME = 0.28
local MIN_PULSE_COUNT = 3
local MAX_PULSE_COUNT = 8
local OFF_INTENSITY = 0.0
local FALLBACK_ON_INTENSITY = 3.0

local light = nil
local originalIntensity = FALLBACK_ON_INTENSITY
local currentIntensity = FALLBACK_ON_INTENSITY
local targetIntensity = FALLBACK_ON_INTENSITY
local startIntensity = FALLBACK_ON_INTENSITY
local elapsed = 0.0
local phaseDuration = 0.0
local pulseCountRemaining = 0
local mode = "stable"

local function rand_range(minValue, maxValue)
    return minValue + (maxValue - minValue) * math.random()
end

local function rand_int(minValue, maxValue)
    return math.floor(rand_range(minValue, maxValue + 1))
end

local function clamp01(value)
    if value < 0.0 then
        return 0.0
    end
    if value > 1.0 then
        return 1.0
    end
    return value
end

local function smooth_step(alpha)
    alpha = clamp01(alpha)
    return alpha * alpha * (3.0 - 2.0 * alpha)
end

local function lerp(a, b, alpha)
    return a + (b - a) * alpha
end

local function choose_fault_intensity()
    local roll = math.random()
    if roll < 0.52 then
        return OFF_INTENSITY
    end
    if roll < 0.86 then
        return originalIntensity * rand_range(0.08, 0.35)
    end
    return originalIntensity * rand_range(0.65, 1.15)
end

local function begin_phase(nextMode, nextTarget, duration)
    mode = nextMode
    elapsed = 0.0
    phaseDuration = duration
    startIntensity = currentIntensity
    targetIntensity = nextTarget
end

local function begin_stable_phase()
    currentIntensity = originalIntensity
    if light ~= nil then
        light:SetIntensity(currentIntensity)
    end
    begin_phase("stable", originalIntensity, rand_range(MIN_STABLE_TIME, MAX_STABLE_TIME))
end

local function begin_fault_burst()
    pulseCountRemaining = rand_int(MIN_PULSE_COUNT, MAX_PULSE_COUNT)
    begin_phase("fault", choose_fault_intensity(), rand_range(MIN_PULSE_TIME, MAX_PULSE_TIME))
end

local function begin_recover_phase()
    begin_phase("recover", originalIntensity, rand_range(MIN_RECOVER_TIME, MAX_RECOVER_TIME))
end

function BeginPlay()
    light = obj:GetSpotLight()
    if light == nil and obj.GetPointLight ~= nil then
        light = obj:GetPointLight()
    end
    if light == nil then
        print("[BrokenSpotLight] SpotLightComponent not found: " .. obj.Name)
        return
    end

    originalIntensity = light:GetIntensity()
    if originalIntensity <= 0.001 then
        originalIntensity = FALLBACK_ON_INTENSITY
    end

    currentIntensity = originalIntensity
    targetIntensity = originalIntensity
    startIntensity = originalIntensity
    begin_stable_phase()
end

function Tick(dt)
    if light == nil then
        return
    end

    elapsed = elapsed + dt
    local alpha = smooth_step(elapsed / phaseDuration)
    currentIntensity = lerp(startIntensity, targetIntensity, alpha)
    light:SetIntensity(currentIntensity)

    if elapsed < phaseDuration then
        return
    end

    if mode == "stable" then
        begin_fault_burst()
        return
    end

    if mode == "fault" then
        pulseCountRemaining = pulseCountRemaining - 1
        if pulseCountRemaining > 0 then
            begin_phase("fault", choose_fault_intensity(), rand_range(MIN_PULSE_TIME, MAX_PULSE_TIME))
        else
            begin_recover_phase()
        end
        return
    end

    begin_stable_phase()
end

function EndPlay()
    if light ~= nil then
        light:SetIntensity(originalIntensity)
    end
end
