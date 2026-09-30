#pragma once

#include "Render/Execute/Passes/Base/RenderPass.h"

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: dispatch tile-based local light culling on compute.
    - Inputs: readable depth SRV, local-light structured buffer, frame constants.
    - Outputs: internal light-culling UAV resources such as tile masks and debug hit map.
    - Registers: CS b0 Frame, CS t1 DepthCopy, CS t6 LocalLights, UAVs are owned by FTileBasedLightCulling.
*/
class FLightCullingPass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override {}
    void BuildDrawCommands(FRenderPipelineContext& Context) override {}
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override {}
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
