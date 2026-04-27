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
	// --- Resolution ---
	void SetResolution(uint32 Res) { Resolution = Res; }
	void ResetResolution() { Resolution.reset(); }
	std::optional<uint32> GetResolution() const { return Resolution; }

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
		ShadowBias.reset();
		ShadowSlopeBias.reset();
		ShadowSharpen.reset();
		FilterMode.reset();
	}

	// 기본값 상수
	static constexpr uint32 kDefaultResolution = 2048;
	static constexpr float  kDefaultBias = 0.005f;
	static constexpr float  kDefaultSlopeBias = 0.005f;
	static constexpr EShadowFilterMode kDefaultFilterMode = EShadowFilterMode::Hard;

	// 오버라이드 또는 기본값 반환 (편의 함수)
	uint32            GetEffectiveResolution() const { return Resolution.value_or(kDefaultResolution); }
	float             GetEffectiveBias() const { return ShadowBias.value_or(kDefaultBias); }
	float             GetEffectiveSlopeBias() const { return ShadowSlopeBias.value_or(kDefaultSlopeBias); }
	EShadowFilterMode GetEffectiveFilterMode() const { return FilterMode.value_or(kDefaultFilterMode); }
	uint32            GetEffectiveFilterModeU32() const { return static_cast<uint32>(GetEffectiveFilterMode()); }

private:
	std::optional<uint32> Resolution;
	std::optional<float>  ShadowBias;
	std::optional<float>  ShadowSlopeBias;
	std::optional<float>  ShadowSharpen;
	std::optional<EShadowFilterMode> FilterMode;
};
