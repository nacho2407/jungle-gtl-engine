#pragma once
#include "Render/Passes/Base/PostProcessPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
/*
    최종 화면에 FXAA를 적용하는 fullscreen 후처리 패스입니다.
*/
class FFXAAPass : public FPostProcessPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    // FXAA는 fullscreen pass이므로 프록시 입력을 사용하지 않는다.
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
