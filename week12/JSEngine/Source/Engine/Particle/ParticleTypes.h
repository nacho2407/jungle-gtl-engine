#pragma once

#include "Core/CoreMinimal.h"

struct ID3D11DeviceContext;

namespace ERHIFeatureLevel
{
	enum Type
	{
		SM5
	};
}

enum class EParticleSortMode
{
	None,
	ViewDepthBackToFront,
	ViewDepthFrontToBack,
	RelativeTime,
};

enum class EParticleCoordinateSpace
{
	Local,
	World,
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
	FVector Location;
	FVector Velocity;
	float RelativeTime = 0.0f;
	float Lifetime = 0.0f;
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
