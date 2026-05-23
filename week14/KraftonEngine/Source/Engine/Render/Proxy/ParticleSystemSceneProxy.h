#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"

#include "Render/Command/DrawCommand.h"
#include "Render/Geometry/SpriteParticleGeometry.h"
#include "Render/Geometry/MeshParticleGeometry.h"

class UParticleSystemComponent;
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

	bool PrepareParticleDrawBuffer(EParticleRenderType Type, ID3D11Device* Device, ID3D11DeviceContext* Context,
		FDrawCommandBuffer& OutBuffer) const;

private:
	void RebuildSpriteParticleGeometry(const FFrameContext& Frame);
	void RebuildMeshParticleGeometry();

	void ClearDrawBatches()
	{
		DrawBatches.clear();
		SectionDraws.clear();
	}

	FParticleDrawBatch& FindOrAddDrawBatch(EParticleRenderType Type);

	UMaterialInterface* ResolveEmitterMaterial(const FParticleEmitterInstance* Instance) const;
	UStaticMesh* ResolveTypeDataMesh(UParticleModuleTypeDataMesh* TypeData) const;
	UParticleSystemComponent* GetParticleSystemComponent() const;

private:
	mutable FSpriteParticleGeometry SpriteGeometry;
	mutable bool bSpriteGeometryCreated = false;
	uint32 SpriteIndexCount = 0;

	mutable FMeshParticleGeometry MeshGeometry;
	mutable bool bMeshGeometryCreated = false;
	uint32 MeshIndexCount = 0;

	mutable TArray<FParticleDrawBatch> DrawBatches;
};
