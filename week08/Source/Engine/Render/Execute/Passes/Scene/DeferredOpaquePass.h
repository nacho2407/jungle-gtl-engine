#pragma once

#include "Render/Execute/Passes/Base/MeshPassBase.h"

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: render opaque geometry into deferred view-mode surfaces.
    - Inputs: opaque primitive proxies, material textures, per-object/material constants, light-culling inputs.
    - Outputs: deferred base surfaces or viewport color fallback.
    - Registers: PS t0-t2 material textures, PS t6 LocalLights, PS t7 TileMask, PS t8 DebugHitMap,
      VS/PS b0 Frame, VS b1 PerObject, VS/PS b2-b3 material/pass constants, VS/PS b4 GlobalLight.
*/
class FDeferredOpaquePass : public FMeshPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override { (void)Context; }
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
