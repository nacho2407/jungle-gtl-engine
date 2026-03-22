#pragma once
#include "PrimitiveComponent.h"

class UBillboardComponent : public UPrimitiveComponent
{
public:
	void UpdateBillboardMatrix(const FMatrix& ViewMatrix);

	bool GetRenderCommand(FRenderCommand& OutCommand) override;

	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Quad;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
};

