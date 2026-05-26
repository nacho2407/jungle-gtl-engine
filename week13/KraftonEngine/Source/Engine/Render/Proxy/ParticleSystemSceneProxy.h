#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Engine/Particles/DynamicEmitterData.h"
#include "Render/Resource/Buffer.h"
#include <memory>

class UParticleSystemComponent;
class FDrawCommandList;
class UMaterial;
class FMeshBuffer;

class FParticleSystemSceneProxy : public FPrimitiveSceneProxy
{
public:
	FParticleSystemSceneProxy(UParticleSystemComponent* InComponent);
	~FParticleSystemSceneProxy() override;

	void UpdateLOD(uint32 LODLevel) override;
	void UpdatePerViewport(const FFrameContext& Frame) override;

	// DrawCommandBuilder::BuildProxyCommands에서 Particle 분기로 호출
	void BuildParticleCommands(ID3D11Device* Device, ID3D11DeviceContext* Context,
		const FFrameContext& Frame, FDrawCommandList& OutCmdList, ERenderPass CurrentPass);

private:
	FVertexBuffer QuadVB;
	FIndexBuffer  QuadIB;

	// ── 에미터별 동적 버퍼 (에미터 수만큼) ──
	struct FEmitterRenderBuffer
	{
		FEmitterRenderBuffer() = default;

		FDynamicVertexBuffer InstanceVB;
		FDynamicIndexBuffer  DynamicIB;
		FConstantBuffer      ParticleFrameCB;
		TArray<uint8>        StagingBuffer;
		TArray<uint32>       StagingIndices;

		// UpdatePerViewport에서 채워지고 BuildParticleCommands에서 읽힘
		int32               ActiveParticleCount = 0;
		int32               DynamicVertexCount  = 0;
		int32               DynamicIndexCount   = 0;
		EDynamicEmitterType EmitterType         = EDynamicEmitterType::Sprite;
		EParticleBlendMode  BlendMode           = EParticleBlendMode::AlphaBlend;
		UMaterial*          Material            = nullptr;
		FMeshBuffer*        EmitterMeshBuffer   = nullptr;  // Mesh 에미터 전용
		TArray<UMaterial*>  MeshSectionMaterials;
		TArray<uint32>      MeshSectionFirstIndices;
		TArray<uint32>      MeshSectionIndexCounts;
	};
	TArray<std::unique_ptr<FEmitterRenderBuffer>> EmitterBuffers;

	// UpdatePerViewport에서 컴포넌트로부터 받아온 데이터 (포인터만 빌림, 소유권 없음)
	TArray<FDynamicEmitterDataBase*> CachedEmitterData;
	int32 CachedEmitterCount = 0;

	void BuildQuadGeometry(ID3D11Device* Device);
	void EnsureEmitterBuffers(ID3D11Device* Device, int32 EmitterCount);

	// 파티클 데이터 → 인스턴스 버퍼 포맷 변환 (CPU 전용)
	void FillStagingBuffer(FDynamicEmitterDataBase& EmitterData,
		FEmitterRenderBuffer& OutBuffer, const FFrameContext& Frame);

	// 타입별 GPU 업로드 + FDrawCommand 생성
	void SubmitEmitter(FEmitterRenderBuffer& Buffer,
		ID3D11Device* Device, ID3D11DeviceContext* Context,
		const FFrameContext& Frame, FDrawCommandList& OutCmdList, ERenderPass CurrentPass);

	void SubmitSpriteEmitter(FEmitterRenderBuffer& Buffer,
		ID3D11Device* Device, ID3D11DeviceContext* Context,
		const FFrameContext& Frame, FDrawCommandList& OutCmdList);

	void SubmitMeshEmitter(FEmitterRenderBuffer& Buffer,
		ID3D11Device* Device, ID3D11DeviceContext* Context,
		const FFrameContext& Frame, FDrawCommandList& OutCmdList, ERenderPass CurrentPass);

	void SubmitBeamTrailEmitter(FEmitterRenderBuffer& Buffer,
		ID3D11Device* Device, ID3D11DeviceContext* Context,
		const FFrameContext& Frame, FDrawCommandList& OutCmdList);
};
