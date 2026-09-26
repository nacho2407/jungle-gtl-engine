#pragma once
#include "InputAction.h"
#include <cmath>

class ENGINE_API FInputModifier
{
public:
	virtual ~FInputModifier() = default;
	virtual FInputActionValue ModifyRaw(const FInputActionValue& Value) = 0;
};

class ENGINE_API FModifierNegative : public FInputModifier
{
public:
	bool bX = true, bY = true, bZ = true;

	FInputActionValue ModifyRaw(const FInputActionValue& V) override
	{
		return V * FVector{ bX ? -1.0f : 1.0f, bY ? -1.0f : 1.0f, bZ ? -1.0f : 1.0f };
	}

};
class ENGINE_API FModifierScale : public FInputModifier
{
public:
	FVector Scale = { 1.0f, 1.0f, 1.0f };
	FModifierScale() = default;
	explicit FModifierScale(const FVector& InScale) : Scale(InScale) {}
	FInputActionValue ModifyRaw(const FInputActionValue& V) override
	{
		return V * Scale;
	}
};

class ENGINE_API FModifierDeadzone : public FInputModifier
{
	float LowerThreshold = 0.2f;
	float UpperThreshold = 1.0f;
	FInputActionValue ModifyRaw(const FInputActionValue& V) override
	{
		float Magnitude = std::fabsf(V.Get());
		if (Magnitude < LowerThreshold)
			return FInputActionValue(0.0f);// DeadZone input ignore
		float Range = UpperThreshold - LowerThreshold;
		if (Range <= 0.0f)// Setting value error 
			return V; //prevent devide 0
		float Normalized = (Magnitude - LowerThreshold) / Range;
		if (Normalized > 1.0f) //limit value
		Normalized = 1.0f;
		float Sign = V.Get() >= 0.0f ? 1.0f : -1.0f; // Dirction recovery

		return FInputActionValue(Sign * Normalized);
	}
};
// will be test
class ENGINE_API FModifierSwizzleAxis : public FInputModifier
{
public:
	enum class ESwizzleOrder { YXZ, ZYX, XZY, YZX, ZXY };
	ESwizzleOrder Order = ESwizzleOrder::YXZ;
	FInputActionValue ModifyRaw(const FInputActionValue& V) override
	{
		FVector In = V.GetVector();
		FVector Out;
		switch (Order)
		{
		case ESwizzleOrder::YXZ: Out = { In.Y, In.X, In.Z }; break;
		case ESwizzleOrder::ZYX: Out = { In.Z, In.Y, In.X }; break;
		case ESwizzleOrder::XZY: Out = { In.X, In.Z, In.Y }; break;
		case ESwizzleOrder::YZX: Out = { In.Y, In.Z, In.X }; break;
		case ESwizzleOrder::ZXY: Out = { In.Z, In.X, In.Y }; break;
		}
		return FInputActionValue(Out);
	}
};