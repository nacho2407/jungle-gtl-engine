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
	bool BuildSpriteCommands(
		const FPrimitiveRenderProxyCollectionContext& Context,
		TArray<FRenderCommand>& OutSpriteCommands,
		TArray<FRenderCommand>& OutSubUVCommands);
	bool BuildMeshCommands(const FPrimitiveRenderProxyCollectionContext& Context, TArray<FRenderCommand>& OutOpaqueCommands, TArray<FRenderCommand>& OutTranslucentCommands);

	/**
	 * @brief Beam emitter snapshot에서 render command를 생성합니다.
	 *
	 * @param Context render command 수집 context
	 *
	 * @param OutOpaqueCommands opaque material Beam command 목록
	 *
	 * @param OutTranslucentCommands translucent material Beam command 목록
	 *
	 * @return command 생성 경로 처리 성공 여부
	 */
	bool BuildBeamCommands(
		const FPrimitiveRenderProxyCollectionContext& Context,
		TArray<FRenderCommand>& OutOpaqueCommands,
		TArray<FRenderCommand>& OutTranslucentCommands);

	bool EnsureSpriteInstanceBuffer(ID3D11Device* Device, uint32 InstanceCount);
	bool EnsureMeshInstanceBuffer(ID3D11Device* Device, uint32 InstanceCount);
	bool UploadSpriteInstances(ID3D11DeviceContext* DeviceContext);
	bool UploadMeshInstances(ID3D11DeviceContext* DeviceContext);

private:
	UParticleSystemComponent* Component = nullptr;
	TArray<FParticleSpriteInstanceData> SpriteInstances;
	TArray<FParticleMeshInstanceData> MeshInstances;
	TComPtr<ID3D11Buffer> SpriteInstanceBuffer;
	TComPtr<ID3D11Buffer> MeshInstanceBuffer;
	uint32 MaxSpriteInstanceCount = 0;
	uint32 MaxMeshInstanceCount = 0;
};
