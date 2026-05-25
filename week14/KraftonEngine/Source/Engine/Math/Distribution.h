#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/MathUtils.h"
#include "Math/Vector.h"
#include "Object/Object.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

#include <cstdlib>

#include "Source/Engine/Math/Distribution.generated.h"

UENUM()
enum class EDistributionValueMode : uint8
{
	Constant,
	Uniform,
};

USTRUCT()
struct FDistributionLookupTable
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Distribution")
	float TimeScale = 1.0f;

	UPROPERTY(Edit, Save, Category="Distribution")
	float TimeBias = 0.0f;

	UPROPERTY(Edit, Save, Category="Distribution")
	TArray<float> Values;

	UPROPERTY(Edit, Save, Category="Distribution")
	uint8 Op = 0;

	UPROPERTY(Edit, Save, Category="Distribution")
	uint8 EntryCount = 0;

	UPROPERTY(Edit, Save, Category="Distribution")
	uint8 EntryStride = 0;

	UPROPERTY(Edit, Save, Category="Distribution")
	uint8 SubEntryStride = 0;

	UPROPERTY(Edit, Save, Category="Distribution")
	uint8 LockFlag = 0;
};

USTRUCT()
struct FRawDistribution
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Distribution", Type=Struct, Struct=FDistributionLookupTable)
	FDistributionLookupTable Table;
};

namespace FDistributionSampling
{
	inline float RandomUnit()
	{
		return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	}

	inline FVector RandomUnitVector()
	{
		return FVector(RandomUnit(), RandomUnit(), RandomUnit());
	}

	inline FVector Lerp(const FVector& A, const FVector& B, const FVector& Alpha)
	{
		return FVector(
			FMath::Lerp(A.X, B.X, Alpha.X),
			FMath::Lerp(A.Y, B.Y, Alpha.Y),
			FMath::Lerp(A.Z, B.Z, Alpha.Z));
	}
}

USTRUCT()
struct FRawDistributionFloat
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Distribution", Type=Struct, Struct=FRawDistribution)
	FRawDistribution RawDistribution;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Mode", Enum=EDistributionValueMode)
	EDistributionValueMode Mode = EDistributionValueMode::Constant;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Constant")
	float Constant = 0.0f;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Min")
	float MinValue = 0.0f;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Max")
	float MaxValue = 0.0f;

	float GetValue(float RandomFraction = FDistributionSampling::RandomUnit()) const
	{
		if (Mode == EDistributionValueMode::Uniform)
		{
			return FMath::Lerp(MinValue, MaxValue, RandomFraction);
		}

		return Constant;
	}
};

USTRUCT()
struct FRawDistributionVector
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Distribution", Type=Struct, Struct=FRawDistribution)
	FRawDistribution RawDistribution;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Mode", Enum=EDistributionValueMode)
	EDistributionValueMode Mode = EDistributionValueMode::Constant;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Constant")
	FVector Constant = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Min")
	FVector MinValue = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Max")
	FVector MaxValue = FVector::ZeroVector;

	FVector GetValue(const FVector& RandomFraction = FDistributionSampling::RandomUnitVector()) const
	{
		if (Mode == EDistributionValueMode::Uniform)
		{
			return FDistributionSampling::Lerp(MinValue, MaxValue, RandomFraction);
		}

		return Constant;
	}
};

UCLASS()
class UDistribution : public UObject
{
public:
	GENERATED_BODY()

	inline static constexpr float DefaultValue = 0.0f;

	virtual bool IsConstant() const { return true; }
};

UCLASS()
class UDistributionFloat : public UDistribution
{
public:
	GENERATED_BODY()

	virtual float GetValue(float Time = 0.0f) const { return DefaultValue; }
};

UCLASS()
class UDistributionFloatConstant : public UDistributionFloat
{
public:
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Constant")
	float Constant = 0.0f;

	float GetValue(float Time = 0.0f) const override { return Constant; }
};

UCLASS()
class UDistributionFloatUniform : public UDistributionFloat
{
public:
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Min")
	float MinValue = 0.0f;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Max")
	float MaxValue = 0.0f;

	bool IsConstant() const override { return false; }
	float GetValue(float Time = 0.0f) const override { return GetValueWithRandom(FDistributionSampling::RandomUnit()); }
	float GetValueWithRandom(float RandomFraction) const { return FMath::Lerp(MinValue, MaxValue, RandomFraction); }
};

UCLASS()
class UDistributionVector : public UDistribution
{
public:
	GENERATED_BODY()

	virtual FVector GetValue(float Time = 0.0f) const { return FVector::ZeroVector; }
};

UCLASS()
class UDistributionVectorConstant : public UDistributionVector
{
public:
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Constant")
	FVector Constant = FVector::ZeroVector;

	FVector GetValue(float Time = 0.0f) const override { return Constant; }
};

UCLASS()
class UDistributionVectorUniform : public UDistributionVector
{
public:
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Min")
	FVector MinValue = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Max")
	FVector MaxValue = FVector::ZeroVector;

	bool IsConstant() const override { return false; }
	FVector GetValue(float Time = 0.0f) const override { return GetValueWithRandom(FDistributionSampling::RandomUnitVector()); }
	FVector GetValueWithRandom(const FVector& RandomFraction) const { return FDistributionSampling::Lerp(MinValue, MaxValue, RandomFraction); }
};
