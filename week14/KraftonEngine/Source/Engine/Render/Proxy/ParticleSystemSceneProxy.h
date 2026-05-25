#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"

#include "Render/Command/DrawCommand.h"
#include "Render/Geometry/SpriteParticleGeometry.h"
#include "Render/Geometry/MeshParticleGeometry.h"

class UStaticMesh;
class UParticleSystemComponent;
struct FDynamicEmitterReplayDataBase;
struct FParticleEmitterInstance;

struct FParticleGeometrySection
{
	UMaterialInterface* Material = nullptr;
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;
};

struct FParticleDrawBatch
{
	EParticleRenderType Type = EParticleRenderType::Sprite;

	UStaticMesh* Mesh = nullptr;
	uint32 FirstInstance = 0;
	uint32 InstanceCount = 0;

	TArray<FMeshSectionDraw> Sections;
};

class FParticleSystemSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FParticleSystemSceneProxy(UParticleSystemComponent* InComponent);
	~FParticleSystemSceneProxy() override;

	void UpdateMesh() override;
	void UpdateMaterial() override;
	void UpdatePerViewport(const FFrameContext& Frame) override;

	bool PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context,
		FDrawCommandBuffer& OutBuffer) const override;

	const TArray<FParticleDrawBatch>& GetParticleDrawBatches() const { return DrawBatches; }

	bool PrepareParticleDrawBuffer(const FParticleDrawBatch& Batch, ID3D11Device* Device, ID3D11DeviceContext* Context,
		FDrawCommandBuffer& OutBuffer) const;

private:
	void ResetDynamicGeometry();
	void BuildDynamicEmitters(const FFrameContext& Frame, const TArray<FParticleEmitterInstance*>& Instances);
	FDynamicEmitterReplayDataBase BuildEmitterSource(FParticleEmitterInstance* Instance) const;
	void AppendEmitter(const FFrameContext& Frame, int32 EmitterIndex, const FDynamicEmitterReplayDataBase& Source);
	void FinalizeDynamicGeometry();

	void AppendSpriteEmitter(const FFrameContext& Frame, int32 EmitterIndex, const FDynamicEmitterReplayDataBase& Source);
	void AppendRibbonEmitter(const FFrameContext& Frame, int32 EmitterIndex, const FDynamicEmitterReplayDataBase& Source);
	void AppendBeamEmitter(const FFrameContext& Frame, int32 EmitterIndex, const FDynamicEmitterReplayDataBase& Source);
	void AppendMeshEmitter(const FFrameContext& Frame, int32 EmitterIndex, const FDynamicEmitterReplayDataBase& Source);

	void ClearDrawBatches()
	{
		DrawBatches.clear();
		SectionDraws.clear();
	}

	FParticleDrawBatch& FindOrAddDrawBatch(EParticleRenderType Type);
	FParticleDrawBatch& FindOrAddMeshDrawBatch(UStaticMesh* Mesh);

	UMaterialInterface* ResolveEmitterMaterial(const FParticleEmitterInstance* Instance) const;
	UStaticMesh* ResolveTypeDataMesh(UParticleModuleTypeDataMesh* TypeData) const;
	bool ShouldSortMeshParticles(const FParticleDrawBatch& Batch, UMaterialInterface* FallbackMaterial) const;
	UParticleSystemComponent* GetParticleSystemComponent() const;

private:
	mutable FSpriteParticleGeometry SpriteGeometry;
	mutable bool bSpriteGeometryCreated = false;
	uint32 SpriteIndexCount = 0;

	mutable FMeshParticleGeometry MeshGeometry;
	mutable bool bMeshGeometryCreated = false;
	uint32 MeshIndexCount = 0;

	mutable FDynamicVertexBuffer MeshInstanceBuffer;
	mutable bool bMeshInstanceBufferCreated = false;
	mutable TArray<FMeshParticleInstanceData> MeshInstances;

	mutable TArray<FParticleDrawBatch> DrawBatches;
};
