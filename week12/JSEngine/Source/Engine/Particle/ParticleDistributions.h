#pragma once

#include "Asset/CurveColorAsset.h"
#include "Asset/CurveFloatAsset.h"
#include "Asset/CurveVectorAsset.h"
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
	float CurveTime = 0.0f;
	float EmitterTime = 0.0f;
	const float* RandomAlpha = nullptr;
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
	TSoftObjectPtr<UCurveVectorAsset> Curve;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveVectorAsset> MinCurve;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveVectorAsset> MaxCurve;
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
	TSoftObjectPtr<UCurveColorAsset> Curve;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveColorAsset> MinCurve;
	UPROPERTY(ReferenceType = Asset)
	TSoftObjectPtr<UCurveColorAsset> MaxCurve;
};

float EvaluateParticleFloat(const FParticleFloatDistribution& Distribution, const FParticleDistributionContext& Context);
FVector EvaluateParticleVector(const FParticleVectorDistribution& Distribution, const FParticleDistributionContext& Context);
FColor EvaluateParticleColor(const FParticleColorDistribution& Distribution, const FParticleDistributionContext& Context);
