#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"

class UStaticMeshComponent;

// ============================================================
// FStaticMeshSceneProxy — UStaticMeshComponent 전용 프록시
// ============================================================
// StaticMesh의 섹션별 머티리얼, 메시 버퍼, 셰이더를 캐싱.
// Mesh/Material dirty 시 SectionDraws를 재구축한다.
class FStaticMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	FStaticMeshSceneProxy(UStaticMeshComponent* InComponent);

	void UpdateMaterial() override;
	void UpdateMesh() override;

private:
	UStaticMeshComponent* GetStaticMeshComponent() const;

	// SectionDraws 재구축 (Mesh/Material 공용)
	void RebuildSectionDraws();
};
