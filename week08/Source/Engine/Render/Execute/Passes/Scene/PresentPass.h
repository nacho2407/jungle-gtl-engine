#pragma once

#include "Render/Execute/Passes/Base/RenderPass.h"

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: copy the rendered viewport texture into the final presentation target.
    - Inputs: viewport render texture or scene-color copy texture.
    - Outputs: swap-chain framebuffer/backbuffer.
    - Registers: none. The pass clears shader-resource bindings before copying.
*/
class FPresentPass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
