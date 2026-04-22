#pragma once
#include "Render/Execute/Passes/Base/MeshPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
/*
    깊이�?먼�? 기록???�후 ?�스??early-z ?�율???�이???�스?�니??
*/
class FDepthPrePass : public FMeshPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    // Depth pre-pass??개별 ?�록??경로�??�용?�다.
    void BuildDrawCommands(FRenderPipelineContext& Context) override { (void)Context; }
    // ?�재 Depth pre-pass??별도 draw command�?만들지 ?�는??
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
