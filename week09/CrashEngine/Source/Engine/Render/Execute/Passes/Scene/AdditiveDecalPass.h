#pragma once

#include "Render/Execute/Passes/Base/MeshPassBase.h"

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: render additive decal-style transparent geometry.
    - Inputs: additive decal primitive proxies, material textures, per-object constants.
    - Outputs: viewport color/depth with additive blending.
    - Registers: mesh draw-command convention using b0/b1/b2-b3 and PS t0-t2.
*/
class FAdditiveDecalPass : public FMeshPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override { (void)Context; }
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
