#pragma once
#include "Render/Execute/Passes/Base/PostProcessPassBase.h"

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: overlay the light-culling debug hit map on top of scene color.
    - Inputs: scene-color copy and debug hit-map SRV from tile light culling.
    - Outputs: viewport color with light-hit visualization.
    - Registers: PS t0 SceneColorCopy when prebound, PS t8 DebugHitMap, PS t11 SceneColor.
*/
class FLightHitMapPass : public FPostProcessPassBase
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
