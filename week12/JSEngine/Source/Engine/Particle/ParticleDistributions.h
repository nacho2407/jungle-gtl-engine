#pragma once

#include "Asset/CurveFloatAsset.h"
#include "Core/CoreMinimal.h"
#include "Core/Reflection/ReflectionMacros.h"
#include "Object/ObjectPtr.h"
#include "Particle/ParticleRandom.h"

UENUM()
enum class EParticleDistributionMode
{
	Constant UMETA(DisplayName = "Constant"),
	RandomRange UMETA(DisplayName = "Random Range"),
	Curve UMETA(DisplayName = "Curve"),
	RandomRangeCurve UMETA(DisplayName = "Random Range Curve"),
};

USTRUCT()
struct FParticleFloatDistribution
{
	GENERATED_STRUCT_BODY(FParticleFloatDistribution)

	UPROPERTY()
	EParticleDistributionMode Mode = EParticleDistributionMode::Constant;
	UPROPERTY()
	float Constant = 0.0f;
	UPROPERTY()
	float Min = 0.0f;
	UPROPERTY()
	float Max = 0.0f;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> Curve;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MinCurve;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MaxCurve;
};

struct FParticleDistributionContext
{
	FParticleRandomStream* RandomStream = nullptr;
	float RelativeTime = 0.0f;
	float SpawnTime = 0.0f;
};

USTRUCT()
struct FParticleVectorDistribution
{
	GENERATED_STRUCT_BODY(FParticleVectorDistribution)

	UPROPERTY()
	EParticleDistributionMode Mode = EParticleDistributionMode::Constant;
	UPROPERTY()
	FVector Constant = FVector::ZeroVector;
	UPROPERTY()
	FVector Min = FVector::ZeroVector;
	UPROPERTY()
	FVector Max = FVector::ZeroVector;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> CurveX;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> CurveY;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> CurveZ;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MinCurveX;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MinCurveY;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MinCurveZ;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MaxCurveX;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MaxCurveY;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MaxCurveZ;
};

USTRUCT()
struct FParticleColorDistribution
{
	GENERATED_STRUCT_BODY(FParticleColorDistribution)

	UPROPERTY()
	EParticleDistributionMode Mode = EParticleDistributionMode::Constant;
	UPROPERTY()
	FColor Constant = FColor::White();
	UPROPERTY()
	FColor Min = FColor::White();
	UPROPERTY()
	FColor Max = FColor::White();
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> CurveR;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> CurveG;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> CurveB;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> CurveA;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MinCurveR;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MinCurveG;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MinCurveB;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MinCurveA;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MaxCurveR;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MaxCurveG;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MaxCurveB;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveFloatAsset> MaxCurveA;
};

// TODO: Curve 모드와 RandomRangeCurve 모드에 대한 처리 추가 예정. 현재는 fallback

inline float EvaluateParticleFloat(const FParticleFloatDistribution& Distribution, const FParticleDistributionContext& Context)
{
	switch (Distribution.Mode)
	{
	case EParticleDistributionMode::RandomRange:
	case EParticleDistributionMode::RandomRangeCurve:
		return Context.RandomStream != nullptr
			? Context.RandomStream->GetRange(Distribution.Min, Distribution.Max)
			: Distribution.Min;

	case EParticleDistributionMode::Curve:
	case EParticleDistributionMode::Constant:
	default:
		return Distribution.Constant;
	}
}

inline FVector EvaluateParticleVector(const FParticleVectorDistribution& Distribution, const FParticleDistributionContext& Context)
{
	switch (Distribution.Mode)
	{
	case EParticleDistributionMode::RandomRange:
	case EParticleDistributionMode::RandomRangeCurve:
		return Context.RandomStream != nullptr
			? FVector(
				Context.RandomStream->GetRange(Distribution.Min.X, Distribution.Max.X),
				Context.RandomStream->GetRange(Distribution.Min.Y, Distribution.Max.Y),
				Context.RandomStream->GetRange(Distribution.Min.Z, Distribution.Max.Z))
			: Distribution.Min;

	case EParticleDistributionMode::Curve:
	case EParticleDistributionMode::Constant:
	default:
		return Distribution.Constant;
	}
}

inline FColor EvaluateParticleColor(const FParticleColorDistribution& Distribution, const FParticleDistributionContext& Context)
{
	switch (Distribution.Mode)
	{
	case EParticleDistributionMode::RandomRange:
	case EParticleDistributionMode::RandomRangeCurve:
		return Context.RandomStream != nullptr
			? FColor(
				Context.RandomStream->GetRange(Distribution.Min.R, Distribution.Max.R),
				Context.RandomStream->GetRange(Distribution.Min.G, Distribution.Max.G),
				Context.RandomStream->GetRange(Distribution.Min.B, Distribution.Max.B),
				Context.RandomStream->GetRange(Distribution.Min.A, Distribution.Max.A))
			: Distribution.Min;

	case EParticleDistributionMode::Curve:
	case EParticleDistributionMode::Constant:
	default:
		return Distribution.Constant;
	}
}
