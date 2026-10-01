#pragma once
#include "Render/Execute/Passes/Base/MeshPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveProxy;
/*
    Pass Summary
    - Role: render selected primitives into depth/stencil selection mask.
    - Inputs: selected primitive proxies and per-object transforms.
    - Outputs: viewport DSV mask for later outline composition.
    - Registers: depth-only style mesh convention using frame/per-object constants.
*/
class FSelectionMaskPass : public FMeshPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override { (void)Context; }
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
