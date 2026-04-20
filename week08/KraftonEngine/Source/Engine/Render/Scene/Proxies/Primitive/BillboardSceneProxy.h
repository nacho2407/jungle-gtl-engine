#pragma once

#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

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
	void UpdatePerViewport(const FFrameContext& Frame) override;

protected:
	UBillboardComponent* GetBillboardComponent() const;
};
