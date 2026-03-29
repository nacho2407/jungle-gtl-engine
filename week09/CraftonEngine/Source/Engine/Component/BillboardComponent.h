#pragma once
#include "PrimitiveComponent.h"

class UBillboardComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

	void TickComponent(float DeltaTime) override;

	void SetBillboardEnabled(bool bEnable) { bIsBillboard = bEnable; }

	// 주어진 카메라 방향으로 빌보드 월드 행렬을 계산 (per-view 렌더링용)
	FMatrix ComputeBillboardMatrix(const FVector& CameraForward) const;

	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Quad;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }

protected:
	bool bIsBillboard = true;
};

