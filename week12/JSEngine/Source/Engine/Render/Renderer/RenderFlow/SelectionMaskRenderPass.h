#pragma once
#include "RenderPass.h"

class FSelectionMaskRenderPass : public FBaseRenderPass
{
public:
    bool Initialize() override;
    bool Release() override;
    static void WarmUpShaderPrograms();

private:
    bool Begin(const FRenderPassContext* Context) override;
    bool DrawCommand(const FRenderPassContext* Context) override;
    bool End(const FRenderPassContext* Context) override;
};
