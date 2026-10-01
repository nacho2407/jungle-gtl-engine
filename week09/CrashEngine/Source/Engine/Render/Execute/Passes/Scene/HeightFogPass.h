#pragma once

#include "Render/Execute/Passes/Base/PostProcessPassBase.h"

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: apply height fog over the current scene color.
    - Inputs: scene-color copy, depth copy, fog scene constants.
    - Outputs: viewport color.
    - Registers: PS t10 SceneDepth, PS t11 SceneColor, PS b2 FogParams.
*/
class FHeightFogPass : public FPostProcessPassBase
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
