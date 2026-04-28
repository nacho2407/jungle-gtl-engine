#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include <optional>

// Shadow Filter Mode — HLSL ShadowFilterMode (b5)와 1:1 대응
enum class EShadowFilterMode : uint32
{
	Hard = 0,
	PCF  = 1,
	VSM  = 2,
};

/*
	FShadowSettings — Shadow 시스템 글로벌 설정.
	콘솔 커맨드 등에서 런타임 오버라이드를 저장하며,
	ShadowMapPass가 프레임마다 여기서 값을 읽는다.
	optional이 비어있으면 per-light 컴포넌트 값 사용.
*/
class FShadowSettings : public TSingleton<FShadowSettings>
{
	friend class TSingleton<FShadowSettings>;

public:
	// --- Directional Shadow CSM ---
	void SetResolution(uint32 Res) { Resolution = Res; }
	void ResetResolution() { Resolution.reset(); }
	std::optional<uint32> GetResolution() const { return Resolution; }

	void SetCSMShadowDistance(float Distance) { DirectionalShadowDistance = Distance; }
	void ResetCSMShadowDistance() { DirectionalShadowDistance.reset(); }
	std::optional<float> GetCSMShadowDistance() const { return DirectionalShadowDistance; }

	void SetCSMCascadeLambda(float Lambda) { CSMSplitLambda = Lambda; }
	void ResetCSMCascadeLambda() { CSMSplitLambda.reset(); }
	std::optional<float> GetCSMCascadeLambda() const { return CSMSplitLambda; }

	void SetCSMShadowCasterDistance(float Lambda) { DirectionalShadowCasterDistance = Lambda; }
	void ResetCSMShadowCasterDistance() { DirectionalShadowCasterDistance.reset(); }
	std::optional<float> GetCSMShadowCasterDistance() const { return DirectionalShadowCasterDistance; }

	void SetCSMBlendEnabled(bool bEnabled) { CSMBlendEnabled = bEnabled; }
	void ResetCSMBlendEnabled() { CSMBlendEnabled.reset(); }
	std::optional<bool> GetCSMBlendEnabled() const { return CSMBlendEnabled; }

	void SetCSMBlendRange(float Range) { CSMBlendRange = Range; }
	void ResetCSMBlendRange() { CSMBlendRange.reset(); }
	std::optional<float> GetCSMBlendRange() const { return CSMBlendRange; }

	// --- Bias ---
	void SetBias(float Bias) { ShadowBias = Bias; }
	void ResetBias() { ShadowBias.reset(); }
	std::optional<float> GetBias() const { return ShadowBias; }

	// --- Slope Bias ---
	void SetSlopeBias(float Slope) { ShadowSlopeBias = Slope; }
	void ResetSlopeBias() { ShadowSlopeBias.reset(); }
	std::optional<float> GetSlopeBias() const { return ShadowSlopeBias; }

	// --- Filter Mode ---
	void SetFilterMode(EShadowFilterMode Mode) { FilterMode = Mode; }
	void ResetFilterMode() { FilterMode.reset(); }
	std::optional<EShadowFilterMode> GetFilterMode() const { return FilterMode; }

	// --- Sharpen ---
	void SetSharpen(float S) { ShadowSharpen = S; }
	void ResetSharpen() { ShadowSharpen.reset(); }
	std::optional<float> GetSharpen() const { return ShadowSharpen; }

	// 모든 오버라이드 해제
	void ResetAll()
	{
		Resolution.reset();
		DirectionalShadowDistance.reset();
		CSMSplitLambda.reset();
		DirectionalShadowCasterDistance.reset();
		CSMBlendEnabled.reset();
		CSMBlendRange.reset();

		ShadowBias.reset();
		ShadowSlopeBias.reset();
		ShadowSharpen.reset();
		FilterMode.reset();
	}

	// 기본값 상수(CSM 전용)
	static constexpr uint32 kDefaultCSMResolution = 2048;
	static constexpr float  kDefaultCSMSplitLambda = 0.85f;
	static constexpr float  kDefaultDirectionalShadowDistance = 300.0f;
	static constexpr float  kDefaultDirectionalShadowCasterDistance = 500.0f;
	static constexpr bool   kDefaultCSMBlendEnabled = true;
	static constexpr float  kDefaultCSMBlendRange = 5.0f;

	// 기본값 상수
	static constexpr float  kDefaultBias = 0.005f;
	static constexpr float  kDefaultSlopeBias = 0.005f;
	static constexpr EShadowFilterMode kDefaultFilterMode = EShadowFilterMode::Hard;

	// 오버라이드 또는 기본값 반환 (편의 함수)
	uint32            GetEffectiveCSMResolution() const { return Resolution.value_or(kDefaultCSMResolution); }
	float             GetEffectiveCSMCascadeLambda() const { return CSMSplitLambda.value_or(kDefaultCSMSplitLambda); }
	float             GetEffectiveCSMDistance() const { return DirectionalShadowDistance.value_or(kDefaultDirectionalShadowDistance); }
	float             GetEffectiveCSMCasterDistance() const { return DirectionalShadowCasterDistance.value_or(kDefaultDirectionalShadowCasterDistance); }
	bool              GetEffectiveCSMBlendEnabled() const { return CSMBlendEnabled.value_or(kDefaultCSMBlendEnabled); }
	float             GetEffectiveCSMBlendRange() const { return CSMBlendRange.value_or(kDefaultCSMBlendRange); }
	
	float             GetEffectiveBias() const { return ShadowBias.value_or(kDefaultBias); }
	float             GetEffectiveSlopeBias() const { return ShadowSlopeBias.value_or(kDefaultSlopeBias); }
	EShadowFilterMode GetEffectiveFilterMode() const { return FilterMode.value_or(kDefaultFilterMode); }
	uint32            GetEffectiveFilterModeU32() const { return static_cast<uint32>(GetEffectiveFilterMode()); }

private:
	std::optional<uint32> Resolution;
	std::optional<float>  CSMSplitLambda;
	std::optional<float>  DirectionalShadowDistance;
	std::optional<float>  DirectionalShadowCasterDistance;
	std::optional<bool>   CSMBlendEnabled;
	std::optional<float>  CSMBlendRange;

	std::optional<float>  ShadowBias;
	std::optional<float>  ShadowSlopeBias;
	std::optional<float>  ShadowSharpen;
	std::optional<EShadowFilterMode> FilterMode;
};
