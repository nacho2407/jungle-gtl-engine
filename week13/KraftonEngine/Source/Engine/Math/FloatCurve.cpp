#include "FloatCurve.h"

#include <algorithm>
#include <cmath>

bool FFloatCurve::IsEmpty() const
{
	return Keys.empty();
}

void FFloatCurve::Reset()
{
	Keys.clear();
	PreExtrapMode = ECurveExtrapMode::Clamp;
	PostExtrapMode = ECurveExtrapMode::Clamp;
	DefaultValue = 0.0f;
}

void FFloatCurve::AddKey(float Time, float Value, ECurveInterpMode InterpMode)
{
	FCurveKey NewKey;
	NewKey.Time = Time;
	NewKey.Value = Value;
	NewKey.InterpMode = InterpMode;
	Keys.push_back(NewKey);
}

void FFloatCurve::SortKeys()
{
	std::sort(Keys.begin(), Keys.end(), [](const FCurveKey& A, const FCurveKey& B)
	{
		return A.Time < B.Time;
	});
}

void FFloatCurve::AutoSetTangents()
{
	for (int32 i = 0; i < (int32)Keys.size(); ++i)
	{
		if (Keys[i].TangentMode == ECurveTangentMode::Auto)
		{
			const bool bHasPrev = i > 0;
			const bool bHasNext = i + 1 < static_cast<int32>(Keys.size());

			float Slope = 0.0f;
			if (bHasPrev && bHasNext)
			{
				const float Dt = Keys[i + 1].Time - Keys[i - 1].Time;
				Slope = fabsf(Dt) < 1e-6f ? 0.0f : (Keys[i + 1].Value - Keys[i - 1].Value) / Dt;
			}
			else if (bHasNext)
			{
				const float Dt = Keys[i + 1].Time - Keys[i].Time;
				Slope = fabsf(Dt) < 1e-6f ? 0.0f : (Keys[i + 1].Value - Keys[i].Value) / Dt;
			}
			else if (bHasPrev)
			{
				const float Dt = Keys[i].Time - Keys[i - 1].Time;
				Slope = fabsf(Dt) < 1e-6f ? 0.0f : (Keys[i].Value - Keys[i - 1].Value) / Dt;
			}

			Keys[i].ArriveTangent = Slope;
			Keys[i].LeaveTangent = Slope;
		}
	}
}

float FFloatCurve::Evaluate(float Time) const
{
	if (Keys.empty())
	{
		return DefaultValue;
	}
	if (Keys.size() == 1)
	{
		return Keys[0].Value;
	}

	const float FirstTime = Keys.front().Time;
	const float LastTime = Keys.back().Time;
	const float Range = LastTime - FirstTime;

	if (Time < FirstTime)
	{
		switch (PreExtrapMode)
		{
		case ECurveExtrapMode::Loop:
			if (std::fabs(Range) > 1e-6f)
			{
				float Wrapped = std::fmod(Time - FirstTime, Range);
				if (Wrapped < 0.0f) Wrapped += Range;
				Time = FirstTime + Wrapped;
				break;
			}
			return Keys.front().Value;
		case ECurveExtrapMode::Linear:
		{
			const FCurveKey& A = Keys[0];
			const FCurveKey& B = Keys[1];
			const float Dt = B.Time - A.Time;
			const float Slope = std::fabs(Dt) < 1e-6f ? 0.0f : (B.Value - A.Value) / Dt;
			return A.Value + (Time - A.Time) * Slope;
		}
		case ECurveExtrapMode::Clamp:
		default:
			return Keys.front().Value;
		}
	}

	if (Time > LastTime)
	{
		switch (PostExtrapMode)
		{
		case ECurveExtrapMode::Loop:
			if (std::fabs(Range) > 1e-6f)
			{
				float Wrapped = std::fmod(Time - FirstTime, Range);
				if (Wrapped < 0.0f) Wrapped += Range;
				Time = FirstTime + Wrapped;
				break;
			}
			return Keys.back().Value;
		case ECurveExtrapMode::Linear:
		{
			const FCurveKey& A = Keys[Keys.size() - 2];
			const FCurveKey& B = Keys[Keys.size() - 1];
			const float Dt = B.Time - A.Time;
			const float Slope = std::fabs(Dt) < 1e-6f ? 0.0f : (B.Value - A.Value) / Dt;
			return B.Value + (Time - B.Time) * Slope;
		}
		case ECurveExtrapMode::Clamp:
		default:
			return Keys.back().Value;
		}
	}

	if (Time <= FirstTime)
	{
		return Keys.front().Value;
	}
	if (Time >= LastTime)
	{
		return Keys.back().Value;
	}

	const int32 Index = FindKeyIndexBefore(Time);
	return EvaluateSegment(Keys[Index], Keys[Index + 1], Time);
}

int32 FFloatCurve::FindKeyIndexBefore(float Time) const
{
	for (int32 i = static_cast<int32>(Keys.size()) - 1; i >= 0; --i)
	{
		if (Keys[i].Time <= Time)
		{
			return i;
		}
	}
	return -1;
}

float FFloatCurve::EvaluateSegment(const FCurveKey& A, const FCurveKey& B, float Time) const
{
	float DeltaTime = B.Time - A.Time;
	if (fabsf(DeltaTime) < 1e-6f)
	{
		return B.Value; // Avoid division by zero
	}

	float Alpha = (Time - A.Time) / (B.Time - A.Time);

	switch (A.InterpMode)
	{
	case ECurveInterpMode::Constant:
		return A.Value;
	case ECurveInterpMode::Linear:
		return A.Value + Alpha * (B.Value - A.Value);
	case ECurveInterpMode::Cubic:
	{
		float P0 = A.Value;
		float P1 = A.Value + A.LeaveTangent * (B.Time - A.Time) / 3.0f;
		float P2 = B.Value - B.ArriveTangent * (B.Time - A.Time) / 3.0f;
		float P3 = B.Value;
		float T = Alpha;
		float T2 = T * T;
		float T3 = T2 * T;
		return P0 * (1 - 3 * T + 3 * T2 - T3) +
			P1 * (3 * T - 6 * T2 + 3 * T3) +
			P2 * (3 * T2 - 3 * T3) +
			P3 * T3;
	}
	default:
		return A.Value; // Fallback to constant if unknown mode
	}
}
