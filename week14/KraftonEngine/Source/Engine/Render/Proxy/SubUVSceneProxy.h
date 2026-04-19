#pragma once

#include "Render/Proxy/BillboardSceneProxy.h"
#include "Render/Resource/Buffer.h"
#include "Core/ResourceTypes.h"

class USubUVComponent;

// ============================================================
// FSubUVSceneProxy — USubUVComponent 전용 프록시
// ============================================================
// Proxy path 렌더링.
// TexturedQuad + SubUV shader, 자체 CB로 UV region 전달.
class FSubUVSceneProxy : public FBillboardSceneProxy
{
public:
	FSubUVSceneProxy(USubUVComponent* InComponent);
	~FSubUVSceneProxy() override;

	void UpdateMesh() override;
	void UpdateMaterial() override;
	void UpdatePerViewport(const FFrameContext& Frame) override;

private:
	USubUVComponent* GetSubUVComponent() const;

	// 프록시별 UV region CB (공유 풀이 아닌 자체 소유)
	FConstantBuffer UVRegionCB;

	// Owner 접근 없이 UpdatePerViewport에서 사용하기 위한 캐시
	const FParticleResource* CachedParticle = nullptr;
	uint32 CachedFrameIndex = 0;
};
