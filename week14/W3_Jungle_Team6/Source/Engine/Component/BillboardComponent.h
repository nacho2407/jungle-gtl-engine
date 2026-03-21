#pragma once
#include "PrimitiveComponent.h"

class UBillboardComponent : public UPrimitiveComponent
{
public:
	void UpdateBillboardMatrix(const FMatrix& ViewMatrix);

	bool GetRenderCommand(const FMatrix& ViewMatrix, const FMatrix& ProjMatrix, FRenderCommand& OutCommand);
};

