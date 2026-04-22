#pragma once
#include "Render/Execute/Passes/Base/MeshPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
/*
    �?모드 중간 ?�면???�칼 ?�정값을 기록?�는 ?�스?�니??
*/
class FDecalPass : public FMeshPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    // Decal pass??개별 decal proxy 처리�??�당?�다.
    void BuildDrawCommands(FRenderPipelineContext& Context) override { (void)Context; }
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
