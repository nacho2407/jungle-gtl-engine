#pragma once
#include "RenderPass.h"

class FOpaqueRenderPass : public FBaseRenderPass
{
public:
    bool Initialize() override;
    bool Begin(const FRenderPassContext* Context) override;
    bool Render(const FRenderPassContext* Context) override;
    bool End(const FRenderPassContext* Context) override;
    bool Release() override;

private:
};
