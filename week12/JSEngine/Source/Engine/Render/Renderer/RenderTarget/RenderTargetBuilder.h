#pragma once
#include "RenderTarget.h"

/**
 * 사용예시
 * FRenderTargetBuilder()
 *				.SetSize(W, H)
 *				.SetFormat(...)
 *				.Build()
 */
class FRenderTargetBuilder
{
private:
    /**
     * Build 에 필요한 세팅들 (FRenderTarget 이 포함한 데이터와 똑같을 필요 없음)
     */
    uint32 Width = 0;
    uint32 Height = 0;
    DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    bool        bCreateRTV = false;
    bool        bCreateSRV = false;

public:
    FRenderTargetBuilder& SetSize(uint32 InWidth, uint32 InHeight);

	FRenderTargetBuilder& SetFormat(DXGI_FORMAT InFormat);

	FRenderTargetBuilder& WithSRV();

	FRenderTargetBuilder& WithRTV();

	FRenderTarget Build(ID3D11Device* Device);
};
