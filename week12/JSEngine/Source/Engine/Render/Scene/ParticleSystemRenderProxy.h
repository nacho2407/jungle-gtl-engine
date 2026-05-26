#pragma once

#include "Render/Common/ComPtr.h"
#include "Render/Scene/PrimitiveRenderProxy.h"
#include "Render/Scene/RenderCommand.h"

class UParticleSystemComponent;
struct ID3D11Buffer;

class FParticleSystemRenderProxy : public FPrimitiveRenderProxy
{
public:
	explicit FParticleSystemRenderProxy(UParticleSystemComponent* InComponent);
	~FParticleSystemRenderProxy() override;

	void CollectCommands(const FPrimitiveRenderProxyCollectionContext& Context) override;
	void ReleaseResources() override;

private:
	bool BuildSpriteCommands(const FPrimitiveRenderProxyCollectionContext& Context, TArray<FRenderCommand>& OutCommands);
	bool EnsureSpriteInstanceBuffer(ID3D11Device* Device, uint32 InstanceCount);
	bool UploadSpriteInstances(ID3D11DeviceContext* DeviceContext);

private:
	UParticleSystemComponent* Component = nullptr;
	TArray<FParticleSpriteInstanceData> SpriteInstances;
	TComPtr<ID3D11Buffer> SpriteInstanceBuffer;
	uint32 MaxSpriteInstanceCount = 0;
};
