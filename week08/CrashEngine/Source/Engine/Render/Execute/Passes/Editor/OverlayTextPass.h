#pragma once
#include "Render/Execute/Passes/Base/RenderPass.h"
struct FRenderPipelineContext;
class FPrimitiveProxy;
/*
    Pass Summary
    - Role: render world-space and screen-space editor text overlays.
    - Inputs: collected overlay text, font atlases, text batches.
    - Outputs: viewport color with blended text quads.
    - Registers: text draw-command convention, mainly b0 Frame and PS t0 font atlas.
*/
class FOverlayTextPass : public FRenderPass
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
