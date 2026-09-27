#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"

class UBillboardComponent;

// ============================================================
// FBillboardSceneProxy — UBillboardComponent 전용 프록시
// ============================================================
// Quad 메시 + Primitive 셰이더 캐싱.
// bPerViewportUpdate = true (카메라 방향으로 빌보드 회전 필요).
class FBillboardSceneProxy : public FPrimitiveSceneProxy
{
public:
	FBillboardSceneProxy(UBillboardComponent* InComponent);

	void UpdateMesh() override;
	void UpdatePerViewport(const FRenderBus& Bus) override;
	void CollectEntries(FRenderBus& Bus) override;

private:
	UBillboardComponent* GetBillboardComponent() const;
};
