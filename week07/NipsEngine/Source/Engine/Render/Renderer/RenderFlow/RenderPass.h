#pragma once
#include "RenderPassContext.h"

class FBaseRenderPass
{
public:
    virtual ~FBaseRenderPass() {}

	virtual bool Initialize() = 0;
    virtual bool Begin(const FRenderPassContext* Context) = 0;
    virtual bool Render(const FRenderPassContext* Context) = 0;
    virtual bool End(const FRenderPassContext* Context) = 0;
    virtual bool Release() = 0;

    ID3D11ShaderResourceView* GetOutSRV() const { return OutSRV; }

protected:
	// Viewport 출력용 최종 View
    ID3D11ShaderResourceView* OutSRV = nullptr;
};