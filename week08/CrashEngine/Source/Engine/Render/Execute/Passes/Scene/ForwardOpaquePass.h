#pragma once

#include "Render/Execute/Passes/Base/MeshPassBase.h"

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: render opaque geometry directly to the viewport in forward path.
    - Inputs: opaque primitive proxies, material textures, per-object/material constants, light buffers.
    - Outputs: viewport color/depth.
    - Registers: PS t0-t2 material textures, PS t6 LocalLights,
      VS/PS b0 Frame, VS b1 PerObject, VS/PS b2-b3 material/pass constants, VS/PS b4 GlobalLight.
*/
class FForwardOpaquePass : public FMeshPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override { (void)Context; }
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;

protected:
    void Cleanup(FRenderPipelineContext& Context) override;
};
