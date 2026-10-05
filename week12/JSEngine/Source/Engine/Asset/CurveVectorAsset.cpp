#include "Asset/CurveVectorAsset.h"

#include "Serialization/Archive.h"

#include <algorithm>

namespace
{
	void SerializeFloatCurve(FArchive& Ar, const char* Prefix, FFloatCurve& Curve);

	template <typename... TCurves>
	float GetCurveStartTime(const TCurves&... Curves);

	template <typename... TCurves>
	float GetCurveEndTime(const TCurves&... Curves);
}

FVector FVectorCurve::Evaluate(float Time) const
{
	return FVector(XCurve.Evaluate(Time), YCurve.Evaluate(Time), ZCurve.Evaluate(Time));
}

void FVectorCurve::SortKeys()
{
	XCurve.SortKeys();
	YCurve.SortKeys();
	ZCurve.SortKeys();
}

float FVectorCurve::GetStartTime() const
{
	return GetCurveStartTime(XCurve, YCurve, ZCurve);
}

float FVectorCurve::GetEndTime() const
{
	return GetCurveEndTime(XCurve, YCurve, ZCurve);
}

FVector UCurveVectorAsset::Evaluate(float Time) const
{
	return VectorCurve.Evaluate(Time);
}

FVectorCurve& UCurveVectorAsset::GetMutableCurve()
{
	return VectorCurve;
}

const FVectorCurve& UCurveVectorAsset::GetCurve() const
{
	return VectorCurve;
}

void UCurveVectorAsset::SetAssetPath(const FString& InPath)
{
	AssetPath = InPath;
}

const FString& UCurveVectorAsset::GetAssetPath() const
{
	return AssetPath;
}

void UCurveVectorAsset::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
	Ar << "AssetPath" << AssetPath;
	SerializeFloatCurve(Ar, "X", VectorCurve.XCurve);
	SerializeFloatCurve(Ar, "Y", VectorCurve.YCurve);
	SerializeFloatCurve(Ar, "Z", VectorCurve.ZCurve);
}

namespace
{
	void SerializeFloatCurve(FArchive& Ar, const char* Prefix, FFloatCurve& Curve)
	{
		TArray<float> Times;
		TArray<float> Values;
		TArray<int32> InterpModes;
		TArray<int32> TangentModes;
		TArray<float> ArriveTangents;
		TArray<float> LeaveTangents;

		if (Ar.IsSaving())
		{
			Times.reserve(Curve.Keys.size());
			Values.reserve(Curve.Keys.size());
			InterpModes.reserve(Curve.Keys.size());
			TangentModes.reserve(Curve.Keys.size());
			ArriveTangents.reserve(Curve.Keys.size());
			LeaveTangents.reserve(Curve.Keys.size());

			for (const FCurveKey& Key : Curve.Keys)
			{
				Times.push_back(Key.Time);
				Values.push_back(Key.Value);
				InterpModes.push_back(static_cast<int32>(Key.InterpMode));
				TangentModes.push_back(static_cast<int32>(Key.TangentMode));
				ArriveTangents.push_back(Key.ArriveTangent);
				LeaveTangents.push_back(Key.LeaveTangent);
			}
		}

		const FString TimesKey = FString(Prefix) + "Times";
		const FString ValuesKey = FString(Prefix) + "Values";
		const FString InterpModesKey = FString(Prefix) + "InterpModes";
		const FString TangentModesKey = FString(Prefix) + "TangentModes";
		const FString ArriveTangentsKey = FString(Prefix) + "ArriveTangents";
		const FString LeaveTangentsKey = FString(Prefix) + "LeaveTangents";

		Ar << TimesKey.c_str() << Times;
		Ar << ValuesKey.c_str() << Values;
		Ar << InterpModesKey.c_str() << InterpModes;
		Ar << TangentModesKey.c_str() << TangentModes;
		Ar << ArriveTangentsKey.c_str() << ArriveTangents;
		Ar << LeaveTangentsKey.c_str() << LeaveTangents;

		if (Ar.IsLoading())
		{
			const size_t KeyCount = Times.size();
			Curve.Keys.clear();
			Curve.Keys.reserve(KeyCount);
			for (size_t Index = 0; Index < KeyCount; ++Index)
			{
				FCurveKey Key;
				Key.Time = Times[Index];
				Key.Value = Index < Values.size() ? Values[Index] : 0.0f;
				Key.InterpMode = Index < InterpModes.size()
					? static_cast<ECurveInterpMode>(InterpModes[Index])
					: ECurveInterpMode::Cubic;
				Key.TangentMode = Index < TangentModes.size()
					? static_cast<ECurveTangentMode>(TangentModes[Index])
					: ECurveTangentMode::Auto;
				Key.ArriveTangent = Index < ArriveTangents.size() ? ArriveTangents[Index] : 0.0f;
				Key.LeaveTangent = Index < LeaveTangents.size() ? LeaveTangents[Index] : 0.0f;
				Curve.Keys.push_back(Key);
			}
			Curve.SortKeys();
		}
	}

	template <typename... TCurves>
	float GetCurveStartTime(const TCurves&... Curves)
	{
		bool bHasValue = false;
		float Result = 0.0f;
		auto Accumulate = [&bHasValue, &Result](const FFloatCurve& Curve)
		{
			if (Curve.Keys.empty())
			{
				return;
			}
			const float Time = Curve.GetStartTime();
			Result = bHasValue ? std::min(Result, Time) : Time;
			bHasValue = true;
		};
		(Accumulate(Curves), ...);
		return Result;
	}

	template <typename... TCurves>
	float GetCurveEndTime(const TCurves&... Curves)
	{
		bool bHasValue = false;
		float Result = 0.0f;
		auto Accumulate = [&bHasValue, &Result](const FFloatCurve& Curve)
		{
			if (Curve.Keys.empty())
			{
				return;
			}
			const float Time = Curve.GetEndTime();
			Result = bHasValue ? std::max(Result, Time) : Time;
			bHasValue = true;
		};
		(Accumulate(Curves), ...);
		return Result;
	}
}
