#pragma once
#include "Render/Passes/Base/PostProcessPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
/*
    SceneDepth를 기반으로 높이 안개를 합성하는 fullscreen 패스입니다.
*/
class FHeightFogPass : public FPostProcessPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    // Height fog는 fullscreen pass이므로 프록시 입력을 사용하지 않는다.
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
