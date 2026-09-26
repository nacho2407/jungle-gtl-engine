#pragma once

#include "CoreMinimal.h"
#include <cmath>

enum class EInputKey : int32
{
	MouseX = 256,
	MouseY = 257,
};

enum class EInputActionValueType : uint32
{
	Bool,
	Float,
	Axis2D,
	Axis3D,
};

struct ENGINE_API FInputActionValue
{
	EInputActionValueType ValueType = EInputActionValueType::Bool; // Input type
	FVector Value = { 0.0f, 0.0f, 0.0f }; //Input Value Container float, bool : x, 2D :x,y  3D x,y,z
	FInputActionValue() = default;

	explicit FInputActionValue(bool bValue)
		: ValueType(EInputActionValueType::Bool)
	{
		if (bValue)
			Value = { 1.0f, 1.0f, 1.0f };
		else
			Value = { 0.0f, 0.0f, 0.0f };
	}

	explicit FInputActionValue(float fValue)
		: ValueType(EInputActionValueType::Float)
	{
		Value = { fValue, 0.0f, 0.0f };
	}
	explicit FInputActionValue(const FVector& vValue)
		: ValueType(EInputActionValueType::Axis3D)
		, Value(vValue) {}

	bool IsNonZero() const
	{
		return std::fabsf(Value.X) > 1e-6f
			|| std::fabsf(Value.Y) > 1e-6f
			|| std::fabsf(Value.Z) > 1e-6f;
	}
	float Get() const { return Value.X; }
	FVector GetVector() const { return Value; }

	FInputActionValue operator - () const
	{
		FInputActionValue result;
		result.ValueType = ValueType;
		result.Value = { -Value.X, -Value.Y, -Value.Z };
		return result;
	};
	FInputActionValue operator*(const FVector& Scale) const
	{
		FInputActionValue result;
		result.ValueType = ValueType;
		result.Value = { Value.X * Scale.X, Value.Y * Scale.Y, Value.Z * Scale.Z };
		return result;


	}
	FInputActionValue operator+(const FInputActionValue& Other) const
	{
		FInputActionValue result;
		result.ValueType = ValueType;
		result.Value = { Value.X + Other.Value.X, Value.Y + Other.Value.Y, Value.Z + Other.Value.Z };
		return result;

	}
};
struct ENGINE_API FInputAction
{
	FString ActionName;
	EInputActionValueType ValueType = EInputActionValueType::Float;

	FInputAction() = default;
	FInputAction(const FString& InName, EInputActionValueType InType)
		: ActionName(InName), ValueType(InType) {
	}
};