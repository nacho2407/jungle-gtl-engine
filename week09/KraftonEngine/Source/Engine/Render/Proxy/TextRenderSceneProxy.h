#pragma once

#include "Render/Proxy/BillboardSceneProxy.h"

class UTextRenderComponent;

// ============================================================
// FTextRenderSceneProxy — UTextRenderComponent 전용 프록시
// ============================================================
// Font 패스 프록시 큐에 제출. Renderer가 PrepareBatchers에서
// CachedText 등을 읽어 FontGeometry로 렌더링.
// PerObjectConstants는 SelectionMask 전용 아웃라인 행렬.
class FTextRenderSceneProxy : public FBillboardSceneProxy
{
public:
	FTextRenderSceneProxy(UTextRenderComponent* InComponent);

	void UpdateMesh() override;
	void UpdatePerViewport(const FFrameContext& Frame) override;

	// Renderer::PrepareBatchers가 읽는 캐싱된 텍스트 데이터
	FString CachedText;
	float   CachedFontScale = 1.0f;
	FMatrix CachedBillboardMatrix;

private:
	UTextRenderComponent* GetTextRenderComponent() const;
};
