#pragma once
#include "Render/Execute/Passes/Base/MeshPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveProxy;
/*
    Pass Summary
    - Role: render transform gizmo meshes for editor interaction.
    - Inputs: gizmo primitive proxies and gizmo per-shader constants.
    - Outputs: viewport color/depth for inner and outer gizmo layers.
    - Registers: mesh pass convention plus gizmo constants on b2.
*/
class FGizmoPass : public FMeshPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override { (void)Context; }
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
