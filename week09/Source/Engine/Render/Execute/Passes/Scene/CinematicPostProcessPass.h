#pragma once

#include "Render/Execute/Passes/Base/PostProcessPassBase.h"

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: apply camera-driven cinematic screen effects as one fullscreen pass.
    - Inputs: scene-color copy SRV and cinematic post-process constant buffer.
    - Outputs: viewport color.
    - Registers: PS t0 SceneColorCopy when prebound, PS t11 SceneColor, PS b2 CinematicPostProcessParams.
*/
class FCinematicPostProcessPass : public FPostProcessPassBase
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

protected:
    bool IsEnabled(const FRenderPipelineContext& Context) const override;
};
