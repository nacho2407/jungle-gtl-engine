#pragma once

#include "Render/Execute/Passes/Base/PostProcessPassBase.h"

class FPrimitiveProxy;
struct FRenderPipelineContext;

/*
    Pass Summary
    - Role: visualize non-lit view modes such as scene depth and world normal.
    - Inputs: depth copy or normal surface SRV plus optional scene-depth constants.
    - Outputs: viewport color.
    - Registers: PS t0 NormalSurface for world normal, PS t10 SceneDepth for depth view, PS b2 SceneDepthParams when needed.
*/
class FNonLitViewModePass : public FPostProcessPassBase
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
