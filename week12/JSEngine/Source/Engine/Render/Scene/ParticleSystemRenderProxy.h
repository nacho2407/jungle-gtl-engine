#pragma once

#include "Render/Common/ComPtr.h"
#include "Render/Scene/PrimitiveRenderProxy.h"
#include "Render/Scene/RenderCommand.h"

class UParticleSystemComponent;
struct ID3D11Buffer;

class FParticleSystemSceneProxy : public FPrimitiveRenderProxy
{
public:
	explicit FParticleSystemSceneProxy(UParticleSystemComponent* InComponent);
	~FParticleSystemSceneProxy() override;

	void CollectCommands(const FPrimitiveRenderProxyCollectionContext& Context) override;
	void ReleaseResources() override;

private:
	bool BuildSpriteCommands(
		const FPrimitiveRenderProxyCollectionContext& Context,
		TArray<FRenderCommand>& OutSpriteCommands);
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

	bool BuildRibbonCommands(
		const FPrimitiveRenderProxyCollectionContext& Context,
		TArray<FRenderCommand>& OutOpaqueCommands,
		TArray<FRenderCommand>& OutTranslucentCommands);

	bool EnsureSpriteInstanceBuffer(ID3D11Device* Device, uint32 InstanceCount);
	bool EnsureMeshInstanceBuffer(ID3D11Device* Device, uint32 InstanceCount);
	bool EnsureBeamInstanceBuffer(ID3D11Device* Device, uint32 InstanceCount);
	bool EnsureRibbonInstanceBuffer(ID3D11Device* Device, uint32 InstanceCount);
	bool UploadSpriteInstances(ID3D11DeviceContext* DeviceContext);
	bool UploadMeshInstances(ID3D11DeviceContext* DeviceContext);
	bool UploadBeamInstances(ID3D11DeviceContext* DeviceContext);
	bool UploadRibbonInstances(ID3D11DeviceContext* DeviceContext);

private:
	UParticleSystemComponent* Component = nullptr;
	TArray<FParticleSpriteInstanceData> SpriteInstances;
	TArray<FParticleMeshInstanceData> MeshInstances;
	TArray<FBeamParticleInstanceData> BeamInstances;
	TArray<FParticleRibbonSegmentInstanceData> RibbonInstances;
	// Particle-local transient streams. Promote to a shared dynamic instance
	// buffer manager when another instanced-surface producer appears.
	TComPtr<ID3D11Buffer> SpriteInstanceBuffer;
	TComPtr<ID3D11Buffer> MeshInstanceBuffer;
	TComPtr<ID3D11Buffer> BeamInstanceBuffer;
	TComPtr<ID3D11Buffer> RibbonInstanceBuffer;
	uint32 MaxSpriteInstanceCount = 0;
	uint32 MaxMeshInstanceCount = 0;
	uint32 MaxBeamInstanceCount = 0;
	uint32 MaxRibbonInstanceCount = 0;
};
