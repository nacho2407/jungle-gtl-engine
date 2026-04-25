#pragma once
#include "Render/Execute/Passes/Base/RenderPass.h"

/*
    Pass Summary
    - Role: render editor helper billboards after scene rendering.
    - Inputs: collected billboard primitive proxies and billboard material data.
    - Outputs: viewport color/depth with overlay billboard geometry.
    - Registers: mesh draw-command convention, typically b0/b1 plus PS t0 material texture.
*/
class FOverlayBillboardPass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override
    {
        (void)Context;
        (void)Proxy;
    }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
