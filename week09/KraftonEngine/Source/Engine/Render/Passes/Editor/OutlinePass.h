#pragma once
#include "Render/Passes/Base/PostProcessPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
/*
    선택 마스크를 읽어 외곽선을 합성하는 후처리 패스입니다.
*/
class FOutlinePass : public FPostProcessPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    // Outline은 fullscreen post-process라서 프록시 입력을 사용하지 않는다.
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
