#pragma once

#include "Core/CoreMinimal.h"

struct ID3D11DeviceContext;
class UMaterialInterface;
class UParticleModuleRequired;
class UStaticMesh;

namespace ERHIFeatureLevel
{
	enum Type
	{
		SM5, // DirectX 11 Shader Model 5.0
	};
}

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
	int32 ParticleStride = 0;
	FParticleDataContainer DataContainer;

	FVector Scale = FVector::OneVector;
	EParticleSortMode SortMode = EParticleSortMode::ViewDepthBackToFront;
};

struct FDynamicSpriteEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	FDynamicSpriteEmitterReplayDataBase() { eEmitterType = EDynamicEmitterType::Sprite; }

	UMaterialInterface* MaterialInterface = nullptr;
	UParticleModuleRequired* RequiredModule = nullptr;
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
	virtual void Render(ID3D11DeviceContext* Context) = 0;
};

struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	void SortSpriteParticles() {}
	virtual int32 GetDynamicVertexStride(ERHIFeatureLevel::Type InFeatureLevel) const = 0;
};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicSpriteEmitterReplayDataBase ReplayData;
	TArray<uint8> OwnedParticleData;
	TArray<uint16> OwnedParticleIndices;

	const FDynamicEmitterReplayDataBase& GetSource() const override { return ReplayData; }
	void Render(ID3D11DeviceContext* Context) override { (void)Context; }
	int32 GetDynamicVertexStride(ERHIFeatureLevel::Type InFeatureLevel) const override
	{
		(void)InFeatureLevel;
		return sizeof(FParticleSpriteVertex);
	}
};

struct FDynamicMeshEmitterData : public FDynamicSpriteEmitterDataBase
{
	// StaticMesh asset들은 ResourceManager가 소유
	UStaticMesh* Mesh = nullptr;
	TArray<FMeshParticleInstanceVertex> InstanceVertices;
	FDynamicMeshEmitterReplayDataBase ReplayData;

	const FDynamicEmitterReplayDataBase& GetSource() const override { return ReplayData; }
	void Render(ID3D11DeviceContext* Context) override { (void)Context; }
	int32 GetDynamicVertexStride(ERHIFeatureLevel::Type InFeatureLevel) const override
	{
		(void)InFeatureLevel;
		return sizeof(FMeshParticleInstanceVertex);
	}
};

struct FDynamicBeamEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicBeamEmitterReplayDataBase ReplayData;

	const FDynamicEmitterReplayDataBase& GetSource() const override { return ReplayData; }
	void Render(ID3D11DeviceContext* Context) override { (void)Context; }
	int32 GetDynamicVertexStride(ERHIFeatureLevel::Type InFeatureLevel) const override
	{
		(void)InFeatureLevel;
		return sizeof(FParticleSpriteVertex);
	}
};

struct FDynamicTrailsEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicRibbonEmitterReplayDataBase ReplayData;

	const FDynamicEmitterReplayDataBase& GetSource() const override { return ReplayData; }
	void Render(ID3D11DeviceContext* Context) override { (void)Context; }
	int32 GetDynamicVertexStride(ERHIFeatureLevel::Type InFeatureLevel) const override
	{
		(void)InFeatureLevel;
		return sizeof(FParticleSpriteVertex);
	}
};

struct FDynamicAnimTrailsEmitterData : public FDynamicTrailsEmitterData
{
};

struct FDynamicRibbonEmitterData : public FDynamicTrailsEmitterData
{
};
