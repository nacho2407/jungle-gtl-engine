#pragma once
#include "Render/Execute/Passes/Base/RenderPass.h"
struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: render editor world grid helper lines.
    - Inputs: generated grid line batch from overlay collection.
    - Outputs: viewport color/depth with grid overlay.
    - Registers: editor line shader convention, primarily frame constants.
*/
class FGridPass : public FRenderPass
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
