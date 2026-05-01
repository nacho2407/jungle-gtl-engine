#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Math/Vector.h"

class UShapeComponent;

// 캐싱된 와이어프레임 라인 세그먼트
struct FWireLine
{
	FVector Start;
	FVector End;
};

// ============================================================
// FShapeSceneProxy — Shape 컴포넌트의 와이어프레임 렌더 프록시
//
// EditorOnly 플래그로 PIE/인게임에서 자동 컬링.
// 캐싱된 라인 데이터를 DrawCommandBuilder가 EditorLines에 병합.
// ============================================================
class FShapeSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FShapeSceneProxy(UShapeComponent* InComponent);

	void UpdateTransform() override;
	void UpdateVisibility() override;

	const TArray<FWireLine>& GetCachedLines() const { return CachedLines; }
	const FVector4& GetWireColor() const { return WireColor; }

private:
	void RebuildLines();

	TArray<FWireLine> CachedLines;
	FVector4 WireColor = { 0.0f, 1.0f, 0.0f, 1.0f };
	bool bDrawOnlyIfSelected = false;
};
