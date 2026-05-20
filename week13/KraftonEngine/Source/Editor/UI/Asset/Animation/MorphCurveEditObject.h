#pragma once

#include "Core/Types/CoreTypes.h"
#include "Object/Object.h"
#include "Animation/Sequence/RawAnimSequenceTrack.h"

#include <algorithm>
#include <cstring>

#include "Source/Editor/UI/Asset/Animation/MorphCurveEditObject.generated.h"

UENUM()
enum class EMorphCurveEditInterpolation : uint8
{
	Constant = 0,
	Linear   = 1,
	Bezier   = 2,
};

inline const char* GMorphCurveEditInterpolationNames[] = {
	"Constant",
	"Linear",
	"Bezier",
};
inline constexpr uint32 GMorphCurveEditInterpolationCount = sizeof(GMorphCurveEditInterpolationNames) / sizeof(GMorphCurveEditInterpolationNames[0]);

UENUM()
enum class EMorphCurveEditHandleMode : uint8
{
	Auto    = 0,
	Aligned = 1,
	Free    = 2,
};

inline const char* GMorphCurveEditHandleModeNames[] = {
	"Auto",
	"Aligned",
	"Free",
};
inline constexpr uint32 GMorphCurveEditHandleModeCount = sizeof(GMorphCurveEditHandleModeNames) / sizeof(GMorphCurveEditHandleModeNames[0]);

UCLASS()
class UMorphCurveEditObject : public UObject
{
public:
	GENERATED_BODY()

	using Super = UObject;

	UPROPERTY(Edit, Category="Morph Curve", DisplayName="Enabled")
	bool bEnabled = true;

	UPROPERTY(Edit, Category="Morph Curve", DisplayName="Weight Scale", Min=-10.0f, Max=10.0f, Speed=0.01f)
	float WeightScale = 1.0f;

	UPROPERTY(Edit, Category="Morph Curve", DisplayName="Weight Bias", Min=-10.0f, Max=10.0f, Speed=0.01f)
	float WeightBias = 0.0f;

	UPROPERTY(Edit, Category="Morph Key", DisplayName="Time", Min=0.0f, Speed=0.01f)
	float KeyTimeSeconds = 0.0f;

	UPROPERTY(Edit, Category="Morph Key", DisplayName="Value", Min=-1.0f, Max=1.0f, Speed=0.01f)
	float KeyValue = 0.0f;

	UPROPERTY(Edit, Category="Morph Key", DisplayName="Interpolation", Enum=EMorphCurveEditInterpolation)
	EMorphCurveEditInterpolation Interpolation = EMorphCurveEditInterpolation::Linear;

	UPROPERTY(Edit, Category="Morph Key", DisplayName="Handle Mode", Enum=EMorphCurveEditHandleMode)
	EMorphCurveEditHandleMode HandleMode = EMorphCurveEditHandleMode::Auto;

	void SetContext(bool bInHasKey, bool bInBezierKey, float InPlayLength)
	{
		bHasKey = bInHasKey;
		bBezierKey = bInBezierKey;
		PlayLength = InPlayLength;
	}

	void LoadFrom(const FMorphTargetCurve& Curve, const FRawFloatCurveKey* Key, float InPlayLength)
	{
		bEnabled = Curve.bEnabled;
		WeightScale = Curve.WeightScale;
		WeightBias = Curve.WeightBias;

		SetContext(Key != nullptr, Key && ((Key->Interpolation & 4) == 4), InPlayLength);
		if (Key)
		{
			KeyTimeSeconds = Key->TimeSeconds;
			KeyValue = ClampValue(Key->Value, -1.0f, 1.0f);
			Interpolation = ToEditInterpolation(Key->Interpolation);
			HandleMode = ToEditHandleMode(Key->TangentMode);
		}
	}

	void ApplyTo(FMorphTargetCurve& Curve, FRawFloatCurveKey* Key) const
	{
		Curve.bEnabled = bEnabled;
		Curve.WeightScale = WeightScale;
		Curve.WeightBias = WeightBias;

		if (Key)
		{
			Key->TimeSeconds = ClampValue(KeyTimeSeconds, 0.0f, PlayLength);
			Key->Value = ClampValue(KeyValue, -1.0f, 1.0f);
			Key->Interpolation = FromEditInterpolation(Interpolation);
			Key->TangentMode = FromEditHandleMode(HandleMode);
		}
	}

	bool ShouldExposeProperty(const FProperty& Property) const override
	{
		const char* Name = Property.Name ? Property.Name : "";
		const bool bKeyProperty =
			std::strcmp(Name, "KeyTimeSeconds") == 0 ||
			std::strcmp(Name, "KeyValue") == 0 ||
			std::strcmp(Name, "Interpolation") == 0 ||
			std::strcmp(Name, "HandleMode") == 0;

		if (bKeyProperty && !bHasKey)
		{
			return false;
		}

		if (std::strcmp(Name, "HandleMode") == 0 && !bBezierKey)
		{
			return false;
		}

		return Super::ShouldExposeProperty(Property);
	}

	void PostEditProperty(const char* PropertyName) override
	{
		(void)PropertyName;
		KeyTimeSeconds = ClampValue(KeyTimeSeconds, 0.0f, PlayLength);
		KeyValue = ClampValue(KeyValue, -1.0f, 1.0f);
	}

private:
	bool bHasKey = false;
	bool bBezierKey = false;
	float PlayLength = 0.0f;

	static float ClampValue(float Value, float MinValue, float MaxValue)
	{
		return std::max(MinValue, std::min(Value, MaxValue));
	}

	static EMorphCurveEditInterpolation ToEditInterpolation(int32 Raw)
	{
		if ((Raw & 4) == 4) return EMorphCurveEditInterpolation::Bezier;
		if ((Raw & 1) == 1) return EMorphCurveEditInterpolation::Constant;
		return EMorphCurveEditInterpolation::Linear;
	}

	static int32 FromEditInterpolation(EMorphCurveEditInterpolation Value)
	{
		switch (Value)
		{
		case EMorphCurveEditInterpolation::Constant: return 1;
		case EMorphCurveEditInterpolation::Bezier:   return 4;
		case EMorphCurveEditInterpolation::Linear: default: return 2;
		}
	}

	static EMorphCurveEditHandleMode ToEditHandleMode(int32 Raw)
	{
		if (Raw == 1) return EMorphCurveEditHandleMode::Aligned;
		if (Raw == 2) return EMorphCurveEditHandleMode::Free;
		return EMorphCurveEditHandleMode::Auto;
	}

	static int32 FromEditHandleMode(EMorphCurveEditHandleMode Value)
	{
		switch (Value)
		{
		case EMorphCurveEditHandleMode::Aligned: return 1;
		case EMorphCurveEditHandleMode::Free:    return 2;
		case EMorphCurveEditHandleMode::Auto: default: return 0;
		}
	}
};
