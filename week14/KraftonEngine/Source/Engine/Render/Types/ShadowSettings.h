#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include <optional>

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

	// 모든 오버라이드 해제
	void ResetAll()
	{
		Resolution.reset();
		ShadowBias.reset();
		ShadowSlopeBias.reset();
	}

	// 기본값 상수
	static constexpr uint32 kDefaultResolution = 2048;
	static constexpr float  kDefaultBias = 0.005f;
	static constexpr float  kDefaultSlopeBias = 1.0f;

	// 오버라이드 또는 기본값 반환 (편의 함수)
	uint32 GetEffectiveResolution() const { return Resolution.value_or(kDefaultResolution); }
	float  GetEffectiveBias() const { return ShadowBias.value_or(kDefaultBias); }
	float  GetEffectiveSlopeBias() const { return ShadowSlopeBias.value_or(kDefaultSlopeBias); }

private:
	std::optional<uint32> Resolution;
	std::optional<float>  ShadowBias;
	std::optional<float>  ShadowSlopeBias;
};
