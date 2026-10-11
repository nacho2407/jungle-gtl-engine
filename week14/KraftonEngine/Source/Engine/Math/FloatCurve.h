#pragma once

#include "Core/Types/CoreTypes.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

class FArchive;

#include "Source/Engine/Math/FloatCurve.generated.h"

UENUM()
enum class ECurveInterpMode : uint8
{
	Constant,
	Linear,
	Cubic,
};

UENUM()
enum class ECurveExtrapMode : uint8
{
	Clamp,
	Linear,
	Loop,
};

UENUM()
enum class ECurveTangentMode : uint8
{
	Auto,
	User,
	Break,
};

USTRUCT()
struct FCurveKey
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Curve")
	float Time = 0.0f;

	UPROPERTY(Edit, Save, Category="Curve")
	float Value = 0.0f;

	UPROPERTY(Edit, Save, Category="Curve")
	float ArriveTangent = 0.0f;

	UPROPERTY(Edit, Save, Category="Curve")
	float LeaveTangent = 0.0f;

	UPROPERTY(Edit, Save, Category="Curve", Enum=ECurveInterpMode)
	ECurveInterpMode InterpMode = ECurveInterpMode::Linear;

	UPROPERTY(Edit, Save, Category="Curve", Enum=ECurveTangentMode)
	ECurveTangentMode TangentMode = ECurveTangentMode::Auto;
};

USTRUCT()
struct FFloatCurve
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Curve", Type=Array, Struct=FCurveKey)
	TArray<FCurveKey> Keys;

	UPROPERTY(Edit, Save, Category="Curve", Enum=ECurveExtrapMode)
	ECurveExtrapMode PreExtrapMode = ECurveExtrapMode::Clamp;

	UPROPERTY(Edit, Save, Category="Curve", Enum=ECurveExtrapMode)
	ECurveExtrapMode PostExtrapMode = ECurveExtrapMode::Clamp;

	UPROPERTY(Edit, Save, Category="Curve")
	float DefaultValue = 0.0f;

	bool IsEmpty() const;
	void Reset();

	void AddKey(float Time, float Value, ECurveInterpMode InterpMode = ECurveInterpMode::Linear);
	void SortKeys();
	void AutoSetTangents();

	float Evaluate(float Time) const;

private:
	int32 FindKeyIndexBefore(float Time) const;
	float EvaluateSegment(const FCurveKey& A, const FCurveKey& B, float Time) const;
};

FArchive& operator<<(FArchive& Ar, FCurveKey& Key);
FArchive& operator<<(FArchive& Ar, FFloatCurve& Curve);
