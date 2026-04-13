#pragma once
#include "RenderPassContext.h"

class FBaseRenderPass
{
public:
    virtual ~FBaseRenderPass() = 0;

	virtual bool Initialize() = 0;
    virtual bool Begin(const FRenderPassContext* Context) = 0;
    virtual bool Render(const FRenderPassContext* Context) = 0;
    virtual bool End(const FRenderPassContext* Context) = 0;
    virtual bool Release() = 0;

private:
};