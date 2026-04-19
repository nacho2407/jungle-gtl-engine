#pragma once

#include "Render/Proxy/BillboardSceneProxy.h"
#include "Core/ResourceTypes.h"

class UTextRenderComponent;
class UMaterial;

// ============================================================
// FTextRenderSceneProxy — UTextRenderComponent 전용 프록시
// ============================================================
// Collector가 CachedText를 읽어 FFontGeometry로 배칭.
// PerObjectConstants는 SelectionMask 전용 아웃라인 행렬.
class FTextRenderSceneProxy : public FBillboardSceneProxy
{
public:
	FTextRenderSceneProxy(UTextRenderComponent* InComponent);

	void UpdateTransform() override;
	void UpdateMesh() override;
	void UpdatePerViewport(const FFrameContext& Frame) override;

	// Collector가 FFontGeometry 배칭에 사용하는 캐싱된 텍스트 데이터
	FString CachedText;
	float   CachedFontScale = 1.0f;
	FMatrix CachedBillboardMatrix;
	const FFontResource* CachedFont = nullptr;

private:
	UTextRenderComponent* GetTextRenderComponent() const;
	UMaterial* TextMaterial = nullptr;

	// 아웃라인 행렬 계산용 캐싱 데이터 (UpdateMesh에서 갱신)
	float CachedCharWidth  = 0.5f;
	float CachedCharHeight = 0.5f;
};
