#pragma once

#include "Core/CoreTypes.h"

enum class ECurveInterpMode : uint8
{
	Constant,
	Linear,
	Cubic,
};

enum class ECurveExtrapMode : uint8
{
	Clamp,
	Linear,
	Loop,
};

struct FCurveKey
{
	float Time;
	float Value;

	float ArriveTangent = 0.0f;
	float LeaveTangent = 0.0f;

	ECurveInterpMode InterpMode = ECurveInterpMode::Linear;
};

struct FFloatCurve
{
	TArray<FCurveKey> Keys;

	ECurveExtrapMode PreExtrapMode = ECurveExtrapMode::Clamp;
	ECurveExtrapMode PostExtrapMode = ECurveExtrapMode::Clamp;

	float DefaultValue = 0.0f;

	bool IsEmpty() const;
	void Reset();

	void AddKey(float Time, float Value, ECurveInterpMode InterpMode = ECurveInterpMode::Linear);
	void SortKeys();

	float Evaluate(float Time) const;

private:
	int32 FindKeyIndexBefore(float Time) const;
	float EvaluateSegment(const FCurveKey& A, const FCurveKey& B, float Time) const;
};
