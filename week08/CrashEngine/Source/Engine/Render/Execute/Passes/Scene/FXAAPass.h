#pragma once

#include "Render/Execute/Passes/Base/PostProcessPassBase.h"

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: apply FXAA as a fullscreen post-process.
    - Inputs: scene-color copy SRV and FXAA constant buffer.
    - Outputs: viewport color.
    - Registers: PS t0 SceneColorCopy when prebound, PS t11 SceneColor, PS b2 FXAAParams.
*/
class FFXAAPass : public FPostProcessPassBase
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
