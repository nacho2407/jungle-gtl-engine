#pragma once

#include "Core/CoreMinimal.h"

class UMaterialInterface;

UENUM()
enum class EParticleSortMode
{
	None UMETA(DisplayName = "None"),
	ViewDepthBackToFront UMETA(DisplayName = "View Depth Back To Front"),
	ViewDepthFrontToBack UMETA(DisplayName = "View Depth Front To Back"),
	RelativeTime UMETA(DisplayName = "Relative Time"),
};

UENUM()
enum class EParticleCoordinateSpace
{
	Local UMETA(DisplayName = "Local"),
	World UMETA(DisplayName = "World"),
};

enum class EDynamicEmitterType
{
	Sprite,
	Mesh,
	Beam,
	Ribbon,
};

struct FParticleEventSpawnData
{
	int32 ParticleIndex = -1;
	FVector Location = FVector::ZeroVector;
};

struct FParticleEventDeathData
{
	int32 ParticleIndex = -1;
	FVector Location = FVector::ZeroVector;
};

struct FParticleEventCollideData
{
	int32 ParticleIndex = -1;
	FVector Location = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
};

struct FParticleEventBurstData
{
	int32 SpawnCount = 0;
	FVector Location = FVector::ZeroVector;
};

struct FBaseParticle
{
	FVector Location = FVector::ZeroVector;
    FVector OldLocation = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
    FVector BaseVelocity = FVector::ZeroVector;
	float RelativeTime = 0.0f;
	float Lifetime = 1.0f;
    float OneOverMaxLifetime = 1.0f; // particle이 현재 수명의 몇 퍼센트 지점에 있는지 계산할 때 사용하는 MaxLifetime의 역수
	float Rotation = 0.0f;
	float RotationRate = 0.0f;
	FVector Size = FVector::OneVector;
	FVector BaseSize = FVector::OneVector;
	FColor Color = FColor::White();
	uint32 Flags = 0;
    uint32 Seed = 0; // seeded procedural 재현성을 위한 랜덤 시드값
};

struct FParticleSpriteVertex
{
	FVector Position;
	FVector2 UV;
	FVector4 Color;
};

struct FMeshParticleInstanceVertex
{
	FMatrix Transform = FMatrix::Identity;
	FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
};

struct FParticleDataContainer
{
	int32 MemBlockSize = 0;
	int32 ParticleDataNumBytes = 0;
	int32 ParticleIndicesNumShorts = 0;

	uint8* ParticleData = nullptr;
	uint16* ParticleIndices = nullptr;
};

struct FDynamicEmitterReplayDataBase
{
	EDynamicEmitterType eEmitterType = EDynamicEmitterType::Sprite;
	int32 ActiveParticleCount = 0;
	// Simulation memory stride. This is not the renderer vertex stride.
	int32 ParticleStride = 0;
	FParticleDataContainer DataContainer;

	UMaterialInterface* Material = nullptr;
	FMatrix ComponentToWorld = FMatrix::Identity;
	EParticleCoordinateSpace CoordinateSpace = EParticleCoordinateSpace::Local;
	FVector Scale = FVector::OneVector;
	EParticleSortMode SortMode = EParticleSortMode::ViewDepthBackToFront;

    // active particle 순회 헬퍼 함수
	const FBaseParticle* GetParticleByActiveIndex(int32 ActiveIndex) const
	{
		if (ActiveIndex < 0 || ActiveIndex >= ActiveParticleCount)
		{
			return nullptr;
		}

		if (DataContainer.ParticleData == nullptr || DataContainer.ParticleIndices == nullptr || ParticleStride <= 0)
		{
			return nullptr;
		}

		const uint16 PhysicalIndex = DataContainer.ParticleIndices[ActiveIndex];
		const size_t ParticleOffset = static_cast<size_t>(PhysicalIndex) * static_cast<size_t>(ParticleStride);
		if (ParticleOffset + sizeof(FBaseParticle) > static_cast<size_t>(DataContainer.ParticleDataNumBytes))
		{
			return nullptr;
		}

		return reinterpret_cast<const FBaseParticle*>(DataContainer.ParticleData + ParticleOffset);
	}
};

struct FDynamicSpriteEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	FDynamicSpriteEmitterReplayDataBase() { eEmitterType = EDynamicEmitterType::Sprite; }
};

struct FDynamicMeshEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	FDynamicMeshEmitterReplayDataBase() { eEmitterType = EDynamicEmitterType::Mesh; }
};

struct FDynamicBeamEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	FDynamicBeamEmitterReplayDataBase() { eEmitterType = EDynamicEmitterType::Beam; }
};

struct FDynamicRibbonEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	FDynamicRibbonEmitterReplayDataBase() { eEmitterType = EDynamicEmitterType::Ribbon; }
};

struct FDynamicEmitterDataBase
{
	virtual ~FDynamicEmitterDataBase() = default;

	int32 EmitterIndex = -1;

	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;
	EDynamicEmitterType GetEmitterType() const { return GetSource().eEmitterType; }
};

struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicSpriteEmitterReplayDataBase ReplayData;

	const FDynamicEmitterReplayDataBase& GetSource() const override { return ReplayData; }
};

struct FDynamicMeshEmitterData : public FDynamicEmitterDataBase
{
	FDynamicMeshEmitterReplayDataBase ReplayData;

	const FDynamicEmitterReplayDataBase& GetSource() const override { return ReplayData; }
};

struct FDynamicBeamEmitterData : public FDynamicEmitterDataBase
{
	FDynamicBeamEmitterReplayDataBase ReplayData;

	const FDynamicEmitterReplayDataBase& GetSource() const override { return ReplayData; }
};

struct FDynamicTrailsEmitterData : public FDynamicEmitterDataBase
{
	FDynamicRibbonEmitterReplayDataBase ReplayData;

	const FDynamicEmitterReplayDataBase& GetSource() const override { return ReplayData; }
};

struct FDynamicAnimTrailsEmitterData : public FDynamicTrailsEmitterData
{
};

struct FDynamicRibbonEmitterData : public FDynamicTrailsEmitterData
{
};
