#pragma once
#include "PrimitiveComponent.h"

class UBillboardComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)
	void UpdateBillboardMatrix(const FMatrix& ViewMatrix);

	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Quad;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
};

