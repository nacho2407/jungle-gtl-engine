#pragma once
#include "Render/Execute/Passes/Base/RenderPass.h"
struct FRenderPipelineContext;
class FPrimitiveProxy;
/*
    Pass Summary
    - Role: render editor and debug line batches over the scene.
    - Inputs: line batch data built from collected overlay/debug geometry.
    - Outputs: viewport color with depth-tested line rendering.
    - Registers: editor line draw-command convention, mainly frame constants.
*/
class FDebugLinePass : public FRenderPass
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
