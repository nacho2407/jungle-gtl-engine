#pragma once

#include "Render/Proxy/BillboardSceneProxy.h"

class UTextRenderComponent;

// ============================================================
// FTextRenderSceneProxy — UTextRenderComponent 전용 프록시
// ============================================================
// FontBatcher로 렌더링 (bBatcherRendered=true).
// 프록시 PerObjectConstants는 SelectionMask 전용 아웃라인 행렬.
class FTextRenderSceneProxy : public FBillboardSceneProxy
{
public:
	FTextRenderSceneProxy(UTextRenderComponent* InComponent);

	void UpdatePerViewport(const FRenderBus& Bus) override;
	void CollectEntries(FRenderBus& Bus) override;

private:
	UTextRenderComponent* GetTextRenderComponent() const;
	FMatrix CachedBillboardMatrix;
};
