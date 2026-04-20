#pragma once
#include "Render/Passes/Base/MeshPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
/*
    깊이만 먼저 기록해 이후 패스의 early-z 효율을 높이는 패스입니다.
*/
class FDepthPrePass : public FMeshPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    // Depth pre-pass는 개별 프록시 경로만 사용한다.
    void BuildDrawCommands(FRenderPipelineContext& Context) override { (void)Context; }
    // 현재 Depth pre-pass는 별도 draw command를 만들지 않는다.
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
