#pragma once

#include "Render/Execute/Passes/Base/MeshPassBase.h"

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: write scene depth/stencil before later geometry or fullscreen passes.
    - Inputs: opaque mesh draw commands and per-object transforms.
    - Outputs: viewport depth/stencil only.
    - Registers: VS b0 Frame, VS b1 PerObject. PS resources are explicitly cleared.
*/
class FDepthPrePass : public FMeshPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override { (void)Context; }
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override
    {
        (void)Context;
        (void)Proxy;
    }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
