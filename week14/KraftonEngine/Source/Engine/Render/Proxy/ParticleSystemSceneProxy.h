#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"

#include "Render/Geometry/SpriteParticleGeometry.h"

class UParticleSystemComponent;
struct FParticleEmitterInstance;

struct FParticleSpriteSection
{
	UMaterialInterface* Material = nullptr;
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;
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

private:
	void RebuildSpriteParticleGeometry(const FFrameContext& Frame);
	void RebuildSectionDraws();

	UMaterialInterface* ResolveEmitterMaterial(const FParticleEmitterInstance* Instance) const;

	UParticleSystemComponent* GetParticleSystemComponent() const;

private:
	mutable FSpriteParticleGeometry SpriteGeometry;
	mutable TArray<FParticleSpriteSection> SpriteSections;
	mutable bool bSpriteGeometryCreated = false;

	uint32 SpriteIndexCount = 0;
};
