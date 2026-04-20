#pragma once

#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Materials/MaterialCore.h"
#include <memory>

class UStaticMeshComponent;

// ============================================================
// FStaticMeshSceneProxy — UStaticMeshComponent 전용 프록시
// ============================================================
// StaticMesh의 섹션별 머티리얼, 메시 버퍼, 셰이더를 캐싱.
// Mesh/Material dirty 시 SectionDraws를 재구축한다.
// LOD: 거리 기반으로 MeshBuffer + SectionDraws를 스왑.
class FStaticMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	static constexpr uint32 MAX_LOD = 4;

	FStaticMeshSceneProxy(UStaticMeshComponent* InComponent);

	void UpdateMaterial() override;
	void UpdateMesh() override;
	void UpdateLOD(uint32 LODLevel) override;

private:
	UStaticMeshComponent* GetStaticMeshComponent() const;

	// 모든 LOD의 SectionDraws 재구축
	void RebuildSectionDraws();

	struct FLODDrawData
	{
		FMeshBuffer* MeshBuffer = nullptr;
		TArray<FMeshSectionDraw> SectionDraws;
		TArray<std::unique_ptr<FMaterialConstantBuffer>> OwnedMaterialCBs;
	};

	FLODDrawData LODData[MAX_LOD];
	TArray<std::unique_ptr<FMaterialConstantBuffer>> ActiveOwnedMaterialCBs;
	uint32 LODCount = 1;
};
