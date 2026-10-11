local FADE_OUT_DURATION = 0.45
local FADE_IN_DURATION = 0.45
local OFF_INTENSITY = 0.0
local FALLBACK_ON_INTENSITY = 2.5

local light = nil
local originalIntensity = FALLBACK_ON_INTENSITY
local elapsed = 0.0
local bFadingOut = true

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

function BeginPlay()
    light = obj:GetPointLight()
    if light == nil then
        print("[WarningPointLight] PointLightComponent not found: " .. obj.Name)
        return
    end

    -- 원래 intensity를 켜짐 값으로 사용하되, 0이면 기본 경고등 밝기로 대체
    originalIntensity = light:GetIntensity()
    if originalIntensity <= 0.001 then
        originalIntensity = FALLBACK_ON_INTENSITY
    end

    elapsed = 0.0
    bFadingOut = true
    light:SetIntensity(originalIntensity)
end

function Tick(dt)
    if light == nil then
        return
    end

    elapsed = elapsed + dt
    local phaseDuration = bFadingOut and FADE_OUT_DURATION or FADE_IN_DURATION
    if phaseDuration <= 0.001 then
        phaseDuration = 0.001
    end

    while elapsed >= phaseDuration do
        elapsed = elapsed - phaseDuration
        bFadingOut = not bFadingOut
        phaseDuration = bFadingOut and FADE_OUT_DURATION or FADE_IN_DURATION
        if phaseDuration <= 0.001 then
            phaseDuration = 0.001
        end
    end

    -- phase별 smoothstep 보간으로 경고등이 갑자기 꺼지지 않고 부드럽게 밝아지고 어두워집니다.
    local alpha = smooth_step(elapsed / phaseDuration)
    local intensity = originalIntensity
    if bFadingOut then
        intensity = lerp(originalIntensity, OFF_INTENSITY, alpha)
    else
        intensity = lerp(OFF_INTENSITY, originalIntensity, alpha)
    end

    light:SetIntensity(intensity)
end

function EndPlay()
    if light ~= nil then
        light:SetIntensity(originalIntensity)
    end
end
