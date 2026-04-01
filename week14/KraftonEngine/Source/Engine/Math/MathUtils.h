#pragma once

namespace FMath
{
	constexpr float Pi = 3.14159265358979323846f;
	constexpr float DegToRad = Pi / 180.0f;
	constexpr float RadToDeg = 180.0f / Pi;
	constexpr float Epsilon = 1e-4f;

	inline float Clamp(float Val, float Lo, float Hi)
	{
		if (Val >= Hi) return Hi;
		if (Val <= Lo) return Lo;
		return Val;
	}
}

// 기존 매크로 호환 — 이행 완료 후 제거
#define M_PI FMath::Pi
#define DEG_TO_RAD FMath::DegToRad
#define RAD_TO_DEG FMath::RadToDeg
#define EPSILON FMath::Epsilon

// 기존 전역 Clamp 호환
inline float Clamp(float val, float lo, float hi) { return FMath::Clamp(val, lo, hi); }
