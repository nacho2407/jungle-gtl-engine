#pragma once

#include "Render/Pipeline/PrimitiveSceneProxy.h"

class UGizmoComponent;

// ============================================================
// FGizmoSceneProxy — UGizmoComponent 전용 프록시
// ============================================================
// Gizmo 셰이더 + 모드별 메시 + ExtraCB(FGizmoConstants) 캐싱.
// bPerViewportUpdate = true (화면 크기 대비 스케일링 필요).
class FGizmoSceneProxy : public FPrimitiveSceneProxy
{
public:
	FGizmoSceneProxy(UGizmoComponent* InComponent);

	void UpdateMesh() override;
	void UpdateMaterial() override;

private:
	UGizmoComponent* GetGizmoComponent() const;
};
