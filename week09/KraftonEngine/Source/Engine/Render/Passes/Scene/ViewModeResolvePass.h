#pragma once

#include "Render/Passes/Base/FullscreenPassBase.h"

class FPrimitiveSceneProxy;
struct FRenderPipelineContext;

/*
    SceneDepth/Normal 같은 특수 뷰 모드 결과를 최종 화면으로 풀어내는 패스입니다.
*/
class FViewModeResolvePass : public FFullscreenPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    // ViewMode post-process는 fullscreen path만 사용한다.
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
