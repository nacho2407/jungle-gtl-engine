#pragma once
#include "Render/Execute/Passes/Base/PostProcessPassBase.h"

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: draw selection outline as a fullscreen post-process.
    - Inputs: stencil copy from SelectionMaskPass and outline constant buffer.
    - Outputs: viewport color with selected-object outline.
    - Registers: PS b2 Outline parameters, PS t13 Stencil.
*/
class FOutlinePass : public FPostProcessPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override
    {
        (void)Context;
        (void)Proxy;
    }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
