#pragma once

#include "Asset/CurveFloatAsset.h"
#include "Core/CoreMinimal.h"
#include "Core/Reflection/ReflectionMacros.h"
#include "Object/ObjectPtr.h"

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
