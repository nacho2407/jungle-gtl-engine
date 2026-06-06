local DamagePostProcess = {}

local WIDGET_PATH = "Content/UI/PostProcess/DamagePostProcess.rml"
local Z_ORDER = 70

local LOW_HEALTH_THRESHOLD = 0.50
local VIGNETTE_PEAK = 0.85
local VIGNETTE_IN_SPEED = 12.0
local VIGNETTE_OUT_SPEED = 2.8

local widget = nil
local vignetteIntensity = 0.0
local vignetteTarget = 0.0
local vignetteRising = false
local vignetteIsRed = false

local function clamp(value, minValue, maxValue)
	if value < minValue then return minValue end
	if value > maxValue then return maxValue end
	return value
end

local function set_opacity(id, value)
	if widget == nil then return end
	widget:SetProperty(id, "opacity", string.format("%.3f", clamp(value, 0.0, 1.0)))
end

function DamagePostProcess.Initialize()
	if widget ~= nil then return end

	widget = UI.CreateWidget(WIDGET_PATH)
	if widget == nil then return end

	widget:SetWantsMouse(false)
	widget:AddToViewportZ(Z_ORDER)
	DamagePostProcess.Reset()
end

function DamagePostProcess.Shutdown()
	if widget ~= nil and widget:IsInViewport() then
		widget:RemoveFromParent()
	end
	widget = nil
end

function DamagePostProcess.Reset()
	vignetteIntensity = 0.0
	vignetteTarget = 0.0
	vignetteRising = false
	vignetteIsRed = false
	set_opacity("damage-vignette-white", 0.0)
	set_opacity("damage-vignette-red", 0.0)
end

function DamagePostProcess.SetHealthRatio(ratio)
	DamagePostProcess.Initialize()
	ratio = clamp(ratio or 0.0, 0.0, 1.0)
end

function DamagePostProcess.TriggerHit(healthRatio)
	DamagePostProcess.Initialize()
	vignetteIsRed = (healthRatio or 1.0) <= LOW_HEALTH_THRESHOLD
	vignetteTarget = VIGNETTE_PEAK
	vignetteRising = true
end

function DamagePostProcess.Tick(dt)
	if widget == nil then return end

	if vignetteRising then
		vignetteIntensity = vignetteIntensity + VIGNETTE_IN_SPEED * dt
		if vignetteIntensity >= vignetteTarget then
			vignetteIntensity = vignetteTarget
			vignetteRising = false
			vignetteTarget = 0.0
		end
	elseif vignetteIntensity > 0.0 then
		vignetteIntensity = vignetteIntensity - VIGNETTE_OUT_SPEED * dt
		if vignetteIntensity < 0.0 then
			vignetteIntensity = 0.0
		end
	end

	local whiteOpacity = 0.0
	local redOpacity = 0.0
	if vignetteIntensity > 0.001 then
		if vignetteIsRed then
			redOpacity = vignetteIntensity
		else
			whiteOpacity = vignetteIntensity
		end
	end
	set_opacity("damage-vignette-white", whiteOpacity)
	set_opacity("damage-vignette-red", redOpacity)
end

return DamagePostProcess
