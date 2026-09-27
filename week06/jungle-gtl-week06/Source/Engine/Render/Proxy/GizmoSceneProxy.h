#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"

class UGizmoComponent;

// ============================================================
// FGizmoSceneProxy — UGizmoComponent 전용 프록시
// ============================================================
// 하나의 GizmoComponent에서 Outer/Inner 2개의 프록시를 생성.
// bPerViewportUpdate = true — 매 프레임 카메라 거리 기반 스케일 + ExtraCB 갱신.
class FGizmoSceneProxy : public FPrimitiveSceneProxy
{
public:
	FGizmoSceneProxy(UGizmoComponent* InComponent, bool bInner = false);

	void UpdateMesh() override;
	void UpdatePerViewport(const FRenderBus& Bus) override;

private:
	UGizmoComponent* GetGizmoComponent() const;
	bool bIsInner = false;
};
