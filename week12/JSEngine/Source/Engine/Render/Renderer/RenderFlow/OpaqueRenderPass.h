#pragma once
#include "RenderPass.h"

class FOpaqueRenderPass : public FBaseRenderPass
{
public:
    bool Initialize() override;
    bool Release() override;

protected:
    bool Begin(const FRenderPassContext* Context) override;
    bool DrawCommand(const FRenderPassContext* Context) override;
    bool DrawEachCommand(const FRenderPassContext* Context, const FRenderCommand& Cmd, const EViewMode ViewMode);
    bool DrawParticleMeshCommand(const FRenderPassContext* Context, const FRenderCommand& Cmd, const EViewMode ViewMode);
    bool End(const FRenderPassContext* Context) override;
};
