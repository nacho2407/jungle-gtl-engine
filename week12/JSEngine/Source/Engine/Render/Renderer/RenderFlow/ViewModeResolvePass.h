#pragma once
#include "RenderPass.h"

class FViewModeResolvePass : public FBaseRenderPass
{
public:
    bool Initialize() override;
    bool Release() override;
	
private:
    bool Begin(const FRenderPassContext* Context) override;
    bool DrawCommand(const FRenderPassContext* Context) override;
    bool End(const FRenderPassContext* Context) override;
};