#pragma once
#include "PrimitiveComponent.h"

class UBillboardComponent : public UPrimitiveComponent
{
protected:
	bool bIsBillboard = true;


public:
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)
	
	void TickComponent(float DeltaTime) override;

	void SetBillboardEnabled(bool bEnable) { bIsBillboard = bEnable; }
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Quad;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
};

