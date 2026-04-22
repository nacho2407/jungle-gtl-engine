#pragma once
#include "Render/Execute/Passes/Base/MeshPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
/*
    기즈�??�록?��? 별도 ?�태�??�더링하???�디???�스?�니??
*/
class FGizmoPass : public FMeshPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    // Gizmo pass??gizmo proxy�?개별?�으�?처리?�다.
    void BuildDrawCommands(FRenderPipelineContext& Context) override { (void)Context; }
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
