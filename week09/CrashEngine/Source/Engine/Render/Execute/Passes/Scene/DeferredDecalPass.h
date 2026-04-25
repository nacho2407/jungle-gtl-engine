#pragma once

#include "Render/Execute/Passes/Base/MeshPassBase.h"

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: project decals into deferred view-mode surfaces.
    - Inputs: decal proxies, base/auxiliary surface SRVs, depth copy SRV, decal constants.
    - Outputs: modified deferred surfaces or viewport color for unlit fallback.
    - Registers: PS t0 decal, PS t1-t3 surface inputs, PS t10 SceneDepth, PS b2 DecalParams.
*/
class FDeferredDecalPass : public FMeshPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override { (void)Context; }
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
