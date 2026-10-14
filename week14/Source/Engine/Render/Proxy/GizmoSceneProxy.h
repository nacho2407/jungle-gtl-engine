#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Resource/Buffer.h"

class UGizmoComponent;
class UMaterial;

// ============================================================
// FGizmoSceneProxy — UGizmoComponent 전용 프록시
// ============================================================
// 하나의 GizmoComponent에서 Outer/Inner 2개의 프록시를 생성.
// bPerViewportUpdate = true — 매 프레임 카메라 거리 기반 스케일 + GizmoCB 갱신.
class FGizmoSceneProxy : public FPrimitiveSceneProxy
{
public:
	FGizmoSceneProxy(UGizmoComponent* InComponent, bool bInner = false);
	~FGizmoSceneProxy() override;

	void UpdateMesh() override;
	void UpdatePerViewport(const FFrameContext& Frame) override;

private:
	UGizmoComponent* GetGizmoComponent() const;
	void RebuildGizmoSectionDraws();

	UMaterial* GizmoMaterial = nullptr;
	FConstantBuffer GizmoCB;		// FlushDirtyBuffers에서 lazy 생성
	bool bIsInner = false;
};
