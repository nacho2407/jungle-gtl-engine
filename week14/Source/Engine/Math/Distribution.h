#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/FloatCurve.h"
#include "Math/MathUtils.h"
#include "Math/Vector.h"
#include "Object/Object.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

#include <algorithm>
#include <cstdlib>

#include "Source/Engine/Math/Distribution.generated.h"

UENUM()
enum class EDistributionValueMode : uint8
{
	Constant,
	Uniform,
	ConstantCurve,
	UniformCurve,
};

USTRUCT()
struct FDistributionLookupTable
{
	GENERATED_BODY()

	UPROPERTY(Save, Category="Distribution")
	float TimeScale = 1.0f;

	UPROPERTY(Save, Category="Distribution")
	float TimeBias = 0.0f;

	UPROPERTY(Save, Category="Distribution")
	TArray<float> Values;

	UPROPERTY(Save, Category="Distribution")
	uint8 Op = 0;

	UPROPERTY(Save, Category="Distribution")
	uint8 EntryCount = 0;

	UPROPERTY(Save, Category="Distribution")
	uint8 EntryStride = 0;

	UPROPERTY(Save, Category="Distribution")
	uint8 SubEntryStride = 0;

	UPROPERTY(Save, Category="Distribution")
	uint8 LockFlag = 0;
};

USTRUCT()
struct FRawDistribution
{
	GENERATED_BODY()

	UPROPERTY(Save, Category="Distribution", Type=Struct, Struct=FDistributionLookupTable)
	FDistributionLookupTable Table;
};

namespace FDistributionSampling
{
	inline uint32 Hash(uint32 Value)
	{
		Value ^= Value >> 16;
		Value *= 0x7feb352du;
		Value ^= Value >> 15;
		Value *= 0x846ca68bu;
		Value ^= Value >> 16;
		return Value;
	}

	inline uint32 HashString(const char* Text)
	{
		uint32 HashValue = 2166136261u;
		while (Text && *Text)
		{
			HashValue ^= static_cast<uint8>(*Text++);
			HashValue *= 16777619u;
		}
		return HashValue;
	}

	inline uint32 CombineSeed(uint32 Seed, const char* StreamName, uint32 Channel = 0)
	{
		return Hash(Seed ^ HashString(StreamName) ^ Hash(Channel + 0x9e3779b9u));
	}

	inline uint32 RandomSeed()
	{
		const uint32 High = static_cast<uint32>(std::rand()) & 0xffffu;
		const uint32 Low = static_cast<uint32>(std::rand()) & 0xffffu;
		return Hash((High << 16) | Low);
	}

	inline uint32 RandomSeed(uint32 EmitterSeed, uint32 ParticleSerial, uint32 SpawnSerial)
	{
		uint32 Seed = Hash(EmitterSeed ^ Hash(ParticleSerial + 0x9e3779b9u));
		Seed = Hash(Seed ^ Hash(SpawnSerial + 0x85ebca6bu));
		return Seed;
	}

	inline float RandomUnit()
	{
		return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	}

	inline float RandomUnit(uint32 Seed, const char* StreamName, uint32 Channel = 0)
	{
		const uint32 Value = CombineSeed(Seed, StreamName, Channel);
		return static_cast<float>(Value & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
	}

	inline FVector RandomUnitVector()
	{
		return FVector(RandomUnit(), RandomUnit(), RandomUnit());
	}

	inline FVector RandomUnitVector(uint32 Seed, const char* StreamName)
	{
		return FVector(
			RandomUnit(Seed, StreamName, 0),
			RandomUnit(Seed, StreamName, 1),
			RandomUnit(Seed, StreamName, 2));
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
struct FFloatVectorCurve
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Curve", DisplayName="X", Type=Struct, Struct=FFloatCurve)
	FFloatCurve X;

	UPROPERTY(Edit, Save, Category="Curve", DisplayName="Y", Type=Struct, Struct=FFloatCurve)
	FFloatCurve Y;

	UPROPERTY(Edit, Save, Category="Curve", DisplayName="Z", Type=Struct, Struct=FFloatCurve)
	FFloatCurve Z;

	FVector Evaluate(float Time) const
	{
		return FVector(X.Evaluate(Time), Y.Evaluate(Time), Z.Evaluate(Time));
	}
};

USTRUCT()
struct FRawDistributionFloat
{
	GENERATED_BODY()

	UPROPERTY(Save, Category="Distribution", Type=Struct, Struct=FRawDistribution)
	FRawDistribution RawDistribution;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Mode", Enum=EDistributionValueMode)
	EDistributionValueMode Mode = EDistributionValueMode::Constant;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Constant", EditCondition="Mode == Constant")
	float Constant = 0.0f;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Min", EditCondition="Mode == Uniform")
	float MinValue = 0.0f;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Max", EditCondition="Mode == Uniform")
	float MaxValue = 0.0f;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Constant Curve", EditCondition="Mode == ConstantCurve", Type=Struct, Struct=FFloatCurve)
	FFloatCurve ConstantCurve;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Min Curve", EditCondition="Mode == UniformCurve", Type=Struct, Struct=FFloatCurve)
	FFloatCurve MinCurve;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Max Curve", EditCondition="Mode == UniformCurve", Type=Struct, Struct=FFloatCurve)
	FFloatCurve MaxCurve;

	float GetValue(float RandomFraction = FDistributionSampling::RandomUnit()) const
	{
		return GetValue(0.0f, RandomFraction);
	}

	float GetValue(float Time, float RandomFraction) const
	{
		if (Mode == EDistributionValueMode::Uniform)
		{
			return FMath::Lerp(MinValue, MaxValue, RandomFraction);
		}
		if (Mode == EDistributionValueMode::ConstantCurve)
		{
			return ConstantCurve.Evaluate(Time);
		}
		if (Mode == EDistributionValueMode::UniformCurve)
		{
			float Min = MinCurve.Evaluate(Time);
			float Max = MaxCurve.Evaluate(Time);
			if (Max < Min)
			{
				std::swap(Min, Max);
			}
			return FMath::Lerp(Min, Max, RandomFraction);
		}

		return Constant;
	}
};

USTRUCT()
struct FRawDistributionVector
{
	GENERATED_BODY()

	UPROPERTY(Save, Category="Distribution", Type=Struct, Struct=FRawDistribution)
	FRawDistribution RawDistribution;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Mode", Enum=EDistributionValueMode)
	EDistributionValueMode Mode = EDistributionValueMode::Constant;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Constant", EditCondition="Mode == Constant")
	FVector Constant = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Min", EditCondition="Mode == Uniform")
	FVector MinValue = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Max", EditCondition="Mode == Uniform")
	FVector MaxValue = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Constant Curve", EditCondition="Mode == ConstantCurve", Type=Struct, Struct=FFloatVectorCurve)
	FFloatVectorCurve ConstantCurve;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Min Curve", EditCondition="Mode == UniformCurve", Type=Struct, Struct=FFloatVectorCurve)
	FFloatVectorCurve MinCurve;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Max Curve", EditCondition="Mode == UniformCurve", Type=Struct, Struct=FFloatVectorCurve)
	FFloatVectorCurve MaxCurve;

	FVector GetValue(const FVector& RandomFraction = FDistributionSampling::RandomUnitVector()) const
	{
		return GetValue(0.0f, RandomFraction);
	}

	FVector GetValue(float Time, const FVector& RandomFraction) const
	{
		if (Mode == EDistributionValueMode::Uniform)
		{
			return FDistributionSampling::Lerp(MinValue, MaxValue, RandomFraction);
		}
		if (Mode == EDistributionValueMode::ConstantCurve)
		{
			return ConstantCurve.Evaluate(Time);
		}
		if (Mode == EDistributionValueMode::UniformCurve)
		{
			FVector Min = MinCurve.Evaluate(Time);
			FVector Max = MaxCurve.Evaluate(Time);
			if (Max.X < Min.X)
			{
				std::swap(Min.X, Max.X);
			}
			if (Max.Y < Min.Y)
			{
				std::swap(Min.Y, Max.Y);
			}
			if (Max.Z < Min.Z)
			{
				std::swap(Min.Z, Max.Z);
			}
			return FDistributionSampling::Lerp(Min, Max, RandomFraction);
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
