#pragma once
#include "RenderPass.h"

class FDecalRenderPass : public FBaseRenderPass
{
public:
    bool Initialize() override;
    bool Release() override;

private:
    bool Begin(const FRenderPassContext* Context) override;
    bool DrawCommand(const FRenderPassContext* Context) override;
    bool DrawEachCommand(const FRenderPassContext* Context, const FRenderCommand& Cmd);
    bool End(const FRenderPassContext* Context) override;
};
