#pragma once
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"

namespace FMath
{
	constexpr float Pi = 3.14159265358979323846f;
	constexpr float DegToRad = Pi / 180.0f;
	constexpr float RadToDeg = 180.0f / Pi;
	constexpr float Epsilon = 1e-4f;
	constexpr float SMALL_NUMBER = 1e-8f;
	constexpr float KINDA_SMALL_NUMBER = 1e-4f;
	static int32 GSRandSeed;

	inline float Clamp(float Val, float Lo, float Hi)
	{
		if (Val >= Hi) return Hi;
		if (Val <= Lo) return Lo;
		return Val;
	}

	inline float Lerp(float A, float B, float Alpha)
	{
		return A + Alpha * (B - A);
	}

	/**
	 * Returns value based on comparand. The main purpose of this function is to avoid
	 * branching based on floating point comparison which can be avoided via compiler
	 * intrinsics.
	 *
	 * Please note that we don't define what happens in the case of NaNs as there might
	 * be platform specific differences.
	 *
	 * @param	Comparand		Comparand the results are based on
	 * @param	ValueGEZero		Return value if Comparand >= 0
	 * @param	ValueLTZero		Return value if Comparand < 0
	 *
	 * @return	ValueGEZero if Comparand >= 0, ValueLTZero otherwise
	 */
	[[nodiscard]] static constexpr float FloatSelect(float Comparand, float ValueGEZero, float ValueLTZero)
	{
		return Comparand >= 0.f ? ValueGEZero : ValueLTZero;
	}

	/**
	 * Converts a float to an integer with truncation towards zero.
	 * @param F		Floating point value to convert
	 * @return		Truncated integer.
	 */
	[[nodiscard]] static constexpr int32 TruncToInt32(float F)
	{
		return (int32)F;
	}
	[[nodiscard]] static constexpr int32 TruncToInt32(double F)
	{
		return (int32)F;
	}
	[[nodiscard]] static constexpr int64 TruncToInt64(double F)
	{
		return (int64)F;
	}

	[[nodiscard]] static constexpr int32 TruncToInt(float F) { return TruncToInt32(F); }
	[[nodiscard]] static constexpr int64 TruncToInt(double F) { return TruncToInt64(F); }

	/**
	 * Converts a float to an integer value with truncation towards zero.
	 * @param F		Floating point value to convert
	 * @return		Truncated integer value.
	 */
	[[nodiscard]] static float TruncToFloat(float F)
	{
		return truncf(F);
	}

	/** Returns lower value in a generic way */
	template <typename T>
	[[nodiscard]] static constexpr T Min(T A, T B)
	{
		// Even though this should be covered by the variadic, we still need this overload because of the many instances of
		// FMath::Min<T>(a, b) with an explicit template parameter, which needs to continue to be supported.

		return (A < B) ? A : B;
	}

	/** Returns higher value in a generic way */
	template <typename T>
	[[nodiscard]] static constexpr T Max(T A, T B)
	{
		// Even though this should be covered by the variadic, we still need this overload because of the many instances of
		// FMath::Max<T>(a, b) with an explicit template parameter, which needs to continue to be supported.

		return (B < A) ? A : B;
	}

	/**
	* Returns signed fractional part of a float.
	* @param Value	Floating point value to convert
	* @return		A float between >=0 and < 1 for nonnegative input. A float between >= -1 and < 0 for negative input.
	*/
	[[nodiscard]] static float Fractional(float Value)
	{
		return Value - TruncToFloat(Value);
	}

	[[nodiscard]] static float SRand()
	{
		GSRandSeed = (GSRandSeed * 196314165) + 907633515;
		union { float f; int32 i; } Result;
		union { float f; int32 i; } Temp;
		const float SRandTemp = 1.0f;
		Temp.f = SRandTemp;
		Result.i = (Temp.i & 0xff800000) | (GSRandSeed & 0x007fffff);
		return Fractional(Result.f);
	}

	[[nodiscard]] static float FRand()
	{
		// 현재는 SRand 가 FRand 의 구현과 같아서 재활용 (향후 바꿔야할 경우를 위해 분리는 해둠)
		return SRand();
	}

	inline static float Abs(float Value)
	{
		return fabsf(Value);
	}

	inline static double Abs(double Value)
	{
		return fabs(Value);
	}

	inline static int32 Abs(int32 Value)
	{
		return abs(Value);
	}

	// 단일 float
	inline static bool IsNearlyZero(float Value, float Tolerance = KINDA_SMALL_NUMBER)
	{
		return Abs(Value) <= Tolerance;
	}

	// FVector 전체가 nearly zero인지
	inline static bool IsNearlyZero(const FVector& V, float Tolerance = KINDA_SMALL_NUMBER)
	{
		return Abs(V.X) <= Tolerance
			&& Abs(V.Y) <= Tolerance
			&& Abs(V.Z) <= Tolerance;
	}
}

// 기존 매크로 호환 — 이행 완료 후 제거
#define M_PI FMath::Pi
#define DEG_TO_RAD FMath::DegToRad
#define RAD_TO_DEG FMath::RadToDeg
#define EPSILON FMath::Epsilon

// 기존 전역 Clamp 호환
inline float Clamp(float val, float lo, float hi) { return FMath::Clamp(val, lo, hi); }
