#pragma once

#include "Core/Types/CoreTypes.h"
#include "Particles/Module/ParticleModuleTypeDataBase.h"
#include "Render/Geometry/MeshParticleGeometry.h"
#include "Render/Geometry/SpriteParticleGeometry.h"

class UMaterialInterface;
struct FFrameContext;
struct FParticleEmitterInstance;
struct FParticleMeshEmitterInstance;

EParticleRenderType GetEmitterRenderType(const FParticleEmitterInstance* Instance);

struct FDynamicEmitterReplayDataBase
{
	FParticleEmitterInstance* Instance = nullptr;
	UMaterialInterface* Material = nullptr;
	EParticleRenderType RenderType = EParticleRenderType::Sprite;
};

struct FDynamicEmitterDataBase
{
	FDynamicEmitterDataBase(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource);
	virtual ~FDynamicEmitterDataBase() = default;

	virtual const FDynamicEmitterReplayDataBase& GetSource() const;
	virtual int32 GetDynamicVertexStride() const = 0;

	int32 EmitterIndex = -1;

protected:
	FDynamicEmitterReplayDataBase Source;
};

struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	FDynamicSpriteEmitterDataBase(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource, bool bInDepthSort);

	void GatherAliveParticleIndices(TArray<uint16>& OutRenderOrder) const;
	void SortSpriteParticles(const FFrameContext& Frame, TArray<uint16>& RenderOrder) const;
	uint32 BuildDynamicVertexData(const FFrameContext& Frame, FSpriteParticleGeometry& OutGeometry) const;

	bool bDepthSort = false;
};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicSpriteEmitterData(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource, bool bInDepthSort);

	int32 GetDynamicVertexStride() const override;
};

struct FDynamicRibbonEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicRibbonEmitterData(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource);

	int32 GetDynamicVertexStride() const override;
};

struct FDynamicBeamEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicBeamEmitterData(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource);

	int32 GetDynamicVertexStride() const override;
};

struct FDynamicMeshEmitterData : public FDynamicEmitterDataBase
{
	FDynamicMeshEmitterData(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource, FParticleMeshEmitterInstance* InMeshInstance);

	int32 GetDynamicVertexStride() const override;
	uint32 BuildDynamicVertexData(TArray<FMeshParticleInstanceData>& OutInstances) const;

	FParticleMeshEmitterInstance* MeshInstance = nullptr;
};
