#pragma once
#include "DepthStencilResource.h"
#include "Core/CoreMinimal.h"

class FDepthStencilBuilder
{
private:
    uint32 Width = 0;
    uint32 Height = 0;

	/** Stencil 을 따로 안 쓰면 Depth 쪽 데이터 bit 를 늘리는 방식이 적절 */
	bool bUseStencil = false;
    bool bCreateSRV = false;

public:
    FDepthStencilBuilder& SetSize(uint32 InWidth, uint32 InHeight);
    FDepthStencilBuilder& WithStencil();
    FDepthStencilBuilder& WithSRV();
    FDepthStencilResource Build(ID3D11Device* Device);
};
