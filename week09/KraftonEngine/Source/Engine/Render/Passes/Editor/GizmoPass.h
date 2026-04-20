#pragma once
#include "Render/Passes/Base/MeshPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
/*
    기즈모 프록시를 별도 상태로 렌더링하는 에디터 패스입니다.
*/
class FGizmoPass : public FMeshPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    // Gizmo pass는 gizmo proxy만 개별적으로 처리한다.
    void BuildDrawCommands(FRenderPipelineContext& Context) override { (void)Context; }
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
